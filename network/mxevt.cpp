// Author : César Godinho
// Date   : 07/11/2024
// Brief  : Event handling from and to a client via TCP/IP

#include <atomic>
#include <functional>
#include <cstring>
#include <algorithm>

#include "../mxevt.h"

#include "../mxlogger.h"
#include "socket.h"
#include <rpcspec.inl>

static std::atomic<std::uint64_t> _client_msg_id = 0;

// NOTE: (Cesar) In theory this could be thread unsafe
// 				 But this practically guards vs using
// 				 RPC calls on a local context
static std::atomic<std::uint64_t> _client_current_caller = 0;

namespace mulex
{
	std::uint64_t GetNextMessageId()
	{
		return _client_msg_id++;
	}

	EvtClientThread::EvtClientThread(const std::string& hostname, std::uint16_t evtport)
	{
		_evt_socket = SocketInit();
		SocketConnect(_evt_socket, hostname, evtport);
		_evt_thread_running.store(true);
		_evt_listen_thread = std::make_unique<std::thread>(
			std::bind(&EvtClientThread::clientListenThread, this, _evt_socket)
		);
		_evt_emit_thread = std::make_unique<std::thread>(
			std::bind(&EvtClientThread::clientEmitThread, this, _evt_socket)
		);
	}

	EvtClientThread::~EvtClientThread()
	{
		_evt_thread_running.store(false);
		_evt_stream->requestUnblock();
		_evt_emit_stack.requestUnblock();
		SocketClose(_evt_socket);
		_evt_listen_thread->join();
		_evt_emit_thread->join();
	}

	void EvtClientThread::clientListenThread(const Socket& socket)
	{
		std::unique_ptr<SysRecvThread> recvthread = SysStartRecvThread(socket, sizeof(EvtHeader), offsetof(EvtHeader, payloadsize));
		SysByteStream& sbs = recvthread->_stream;
		_evt_stream = &recvthread->_stream;
		static constexpr std::uint64_t buffersize = SYS_RECV_THREAD_BUFFER_SIZE;
		static std::vector<std::uint8_t> fbuffer(buffersize);

		while(_evt_thread_running.load())
		{
			// Read next event
			std::uint64_t read = sbs.fetch(fbuffer.data(), buffersize);

			if(read <= 0 && !_evt_thread_running.load())
			{
				break;
			}
			
			// TODO: (Cesar): Place the new event on the event stack
			//  			  For now we just run the event callback from this thread
			//				  and see how it goes
			EvtHeader header;
			std::memcpy(&header, fbuffer.data(), sizeof(EvtHeader));
			LogTrace("[evtclient] Got Event <%d> from <%llu>", header.eventid, header.client);
			// TODO: For now just trace
		}
		
		recvthread->_handle.join();
	}

	void EvtClientThread::clientEmitThread(const Socket& socket)
	{
		while(_evt_thread_running.load())
		{
			// Read next event to emit from stack
			std::vector<std::uint8_t> data = _evt_emit_stack.pop();

			if(data.size() == 0 && !_evt_thread_running.load())
			{
				break;
			}
			
			// TODO: (Cesar): Place the new event on the event stack
			//  			  For now we just run the event callback from this thread
			//				  and see how it goes
			EvtHeader header;
			std::memcpy(&header, data.data(), sizeof(EvtHeader));
			LogTrace("[evtclient] Emitting Event <%d> from <%llu>", header.eventid, header.client);
			
			SocketSendBytes(socket, data.data(), data.size());
		}
	}

	void EvtClientThread::emit(const std::string& event, const std::uint8_t* data, std::uint64_t len)
	{
		auto evt = _evt_registry.find(event);
		if(evt == _evt_registry.end())
		{
			LogError("[evtclient] Could not find event <%s> in registry.", event.c_str());
			return;
		}

		// Make header
		EvtHeader header;
		header.client = SysGetClientId();
		header.eventid = evt->second;
		header.msgid = GetNextMessageId();
		header.payloadsize = static_cast<std::uint32_t>(len);

		std::vector<std::uint8_t> vdata;
		vdata.resize(sizeof(EvtHeader) + len);

		std::memcpy(vdata.data(), &header, sizeof(EvtHeader));
		if(len > 0)
		{
			std::memcpy(vdata.data() + sizeof(EvtHeader), data, len);
		}

		_evt_emit_stack.push(std::move(vdata));
	}

	void EvtClientThread::regist(const std::string& event)
	{
		// Ask server to register event via RPC
		std::optional<const Experiment*> experiment = SysGetConnectedExperiment();
		if(!experiment.has_value())
		{
			LogError("[evtclient] Failed to register event. Not connected to and experiment.");
			return;
		}
		
		bool reg = experiment.value()->_rpc_client->call<bool>(RPC_CALL_MULEX_EVTREGISTER, string32(event));
		if(!reg)
		{
			LogError("[evtclient] Failed to register event. EvtRegister() returned false.");
		}
	}

	void EvtClientThread::subscribe(const std::string& event, EvtCallbackFunc callback)
	{
		// Ask server for the event id via RPC
		std::optional<const Experiment*> experiment = SysGetConnectedExperiment();
		if(!experiment.has_value())
		{
			LogError("[evtclient] Failed to subscribe to event. Not connected to and experiment.");
			return;
		}
		
		std::uint16_t eventid = experiment.value()->_rpc_client->call<std::uint16_t>(RPC_CALL_MULEX_EVTGETID, string32(event));

		if(eventid == 0)
		{
			LogError("[evtclient] Failed to subscribe to event. Event <%s> is not registered.");
			return;
		}

		// Register this subscription on the rdb
		RdbAccess rda;
		std::string skey = "/system/clients/" + std::to_string(SysGetClientId()) + "/events/subscribed/totalcalls";
		if(!rda[skey].exists())
		{
			rda.create(skey, RdbValueType::UINT64, 0);
		}

		// TODO: (Cesar): Maybe come up with a syntax for rda[key]++
		std::uint64_t calls = rda[skey];
		rda[skey] = calls + 1;

		std::string ckey = "/system/clients/" + std::to_string(SysGetClientId()) + "/events/subscribed/count";
		if(!rda[ckey].exists())
		{
			rda.create(ckey, RdbValueType::UINT32, 0);
		}
		std::uint32_t count = rda[ckey];
		rda[ckey] = count + 1;
		std::string rkey = "/system/clients/" + std::to_string(SysGetClientId()) + "/events/subscribed/ids";
		if(!rda[rkey].exists())
		{
			rda.create(rkey, RdbValueType::UINT16, nullptr, 0);
		}
		// This syntax simply avoids creating a std::vector copy
		// Here it would not matter but just as a practice
		rda[rkey].asPointer<std::uint16_t>()[count] = eventid;
		rda[rkey].flush();

		_evt_callbacks.emplace(eventid, callback);
		LogTrace("[evtclient] Subscribed to event <%s> [%d].", event.c_str(), eventid);
	}

	EvtServerThread::EvtServerThread()
	{
		_evt_thread_running.store(true);
		_evt_thread_ready.store(false);
		
		_evt_accept_thread = std::make_unique<std::thread>(
			std::bind(&EvtServerThread::serverConnAcceptThread, this)
		);
	}

	EvtServerThread::~EvtServerThread()
	{
		_evt_thread_running.store(false);
		_evt_accept_thread->join();
		_evt_emit_stack.requestUnblock();
		std::for_each(_evt_stream.begin(), _evt_stream.end(), [](auto& t){ t.second->requestUnblock(); });
		std::for_each(_evt_emit_thread.begin(), _evt_emit_thread.end(), [](auto& t){ t.second->join(); });
		std::for_each(_evt_listen_thread.begin(), _evt_listen_thread.end(), [](auto& t){ t.second->join(); });
		_evt_thread_ready.store(false);
	}

	void EvtServerThread::serverConnAcceptThread()
	{
		_server_socket = SocketInit();
		SocketBindListen(_server_socket, EVT_PORT);
		if(!SocketSetNonBlocking(_server_socket))
		{
			LogError("Failed to set listen socket to no blocking mode.");
			SocketClose(_server_socket);
			return;
		}

		bool would_block;
		_evt_thread_ready.store(true);
		while(_evt_thread_running.load())
		{
			Socket client = SocketAccept(_server_socket, &would_block);

			if(would_block)
			{
				// Loop and recheck
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				continue;
			}

			if(!client._error)
			{
				std::lock_guard<std::mutex> lock(_connections_mutex);
				_evt_emit_thread.emplace(
					client,
					std::make_unique<std::thread>(std::bind(&EvtServerThread::serverEmitThread, this, client))
				);
				_evt_listen_thread.emplace(
					client,
					std::make_unique<std::thread>(std::bind(&EvtServerThread::serverListenThread, this, client))
				);
			}

			// Ignore and keep trying
			// SocketAccept() already handles errors
		}

		SocketClose(_server_socket);
	}

	void EvtServerThread::serverListenThread(const Socket& socket)
	{
		std::unique_ptr<SysRecvThread> recvthread = SysStartRecvThread(socket, sizeof(EvtHeader), offsetof(EvtHeader, payloadsize));
		SysByteStream& sbs = recvthread->_stream;
		_evt_stream.emplace(socket, &recvthread->_stream);
		static constexpr std::uint64_t buffersize = SYS_RECV_THREAD_BUFFER_SIZE;
		static std::vector<std::uint8_t> fbuffer(buffersize);

		while(_evt_thread_running.load() && _evt_thread_sig.at(socket).load())
		{
			// Read next event
			std::uint64_t read = sbs.fetch(fbuffer.data(), buffersize);

			if(read <= 0 && (!_evt_thread_running.load() && !_evt_thread_sig.at(socket).load()))
			{
				break;
			}
			
			// TODO: (Cesar): Place the new event on the event stack
			//  			  For now we just run the event callback from this thread
			//				  and see how it goes
			EvtHeader header;
			std::memcpy(&header, fbuffer.data(), sizeof(EvtHeader));
			LogTrace("[evtclient] Got Event <%d> from <%llu>", header.eventid, header.client);
			// TODO: For now just trace
			
			// Relay event to clients that are subscribed
		}
		
		recvthread->_handle.join();
	}

	void EvtServerThread::serverEmitThread(const Socket& socket)
	{
		while(_evt_thread_running.load() && _evt_thread_sig.at(socket).load())
		{
			// Read next event to emit from stack
			std::vector<std::uint8_t> data = _evt_emit_stack.pop();

			if(data.size() == 0 && (!_evt_thread_running.load() || !_evt_thread_running.load()))
			{
				break;
			}
			
			// TODO: (Cesar): Place the new event on the event stack
			//  			  For now we just run the event callback from this thread
			//				  and see how it goes
			EvtHeader header;
			std::memcpy(&header, data.data(), sizeof(EvtHeader));
			LogTrace("[evtclient] Emitting Event <%d> from <%llu>", header.eventid, header.client);
			
			SocketSendBytes(socket, data.data(), data.size());
		}
	}

	bool EvtRegister(string32 name)
	{
		return true;
	}
}
