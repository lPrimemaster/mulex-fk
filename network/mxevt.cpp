// Author : César Godinho
// Date   : 07/11/2024
// Brief  : Event handling from and to a client via TCP/IP

#include <atomic>
#include <functional>
#include <cstring>
#include <algorithm>
#include <set>

#include "../mxevt.h"

#include "../mxlogger.h"
#include "socket.h"
#include <rpcspec.inl>

static std::atomic<std::uint64_t> _client_msg_id = 0;

// NOTE: (Cesar) In theory this could be thread unsafe
// 				 But this practically guards vs using
// 				 RPC calls on a local context
static std::atomic<std::uint64_t> _client_current_caller = 0;

static std::map<std::string, std::uint16_t> _evt_server_reg;
static std::shared_mutex _evt_reg_lock;
static std::atomic<std::uint16_t> _evt_server_reg_next = 0;

// NOTE: (Cesar) Map for eventid -> subscribed clientid list
static std::map<std::uint16_t, std::set<std::uint64_t>> _evt_current_subscriptions;
static std::mutex _evt_sub_lock;

static std::map<std::uint16_t, std::function<void(const mulex::Socket&, std::uint64_t, std::uint16_t, const std::uint8_t*, std::uint64_t)>> _evt_server_callbacks;

#ifdef __linux__
static std::map<int, std::uint64_t> _evt_client_socket_pair;
#else
static std::map<SOCKET, std::uint64_t> _evt_client_socket_pair;
#endif
static std::map<std::uint64_t, mulex::Socket> _evt_client_socket_pair_rev;
static std::set<std::uint16_t> _evt_client_ghost;

// TODO: (Cesar) Update this map value type as required
static std::map<std::uint64_t, std::atomic<std::uint64_t>> _evt_client_stats;
static std::mutex _evt_client_stats_lock;

namespace mulex
{
	std::uint64_t GetNextEventMessageId()
	{
		return _client_msg_id++;
	}

	EvtClientThread::EvtClientThread(const std::string& hostname, const Experiment* exp, std::uint16_t evtport, bool ghost, std::uint64_t customid)
	{
		_exp = exp;
		if(_exp)
		{
			LogTrace("[evtclient] Passed in custom experiment dependency.");
		}

		if(customid > 0)
		{
			_evt_has_custom_id = true;
			_evt_custom_id = customid;

			LogDebug("[evtclient] Client spawned with custom id.");
			
			if(!ghost)
			{
				LogError("[evtclient] Specified custom client id without being ghost.");
				LogError("[evtclient] This is not possible.");
				return;
			}
		}

		_evt_socket = SocketInit();
		SocketConnect(_evt_socket, hostname, evtport);
		_evt_thread_running.store(true);
		_evt_listen_thread = std::make_unique<std::thread>(
			std::bind(&EvtClientThread::clientListenThread, this, _evt_socket)
		);
		_evt_emit_thread = std::make_unique<std::thread>(
			std::bind(&EvtClientThread::clientEmitThread, this, _evt_socket)
		);

		if(ghost)
		{
			emit("mxevt::getclientmeta", nullptr, 0);
		}
		else
		{
			// Tell the server who we are
			std::string_view bname = SysGetBinaryName();
			std::string_view hname = SysGetHostname();
			std::string client_name_meta = std::string(bname) + "@" + std::string(hname);

			emit("mxevt::getclientmeta", reinterpret_cast<const std::uint8_t*>(client_name_meta.c_str()), client_name_meta.size() + 1);
		}
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
		std::vector<std::uint8_t> fbuffer(buffersize);

		while(_evt_thread_running.load())
		{
			// Read next event
			std::uint64_t read = sbs.fetch(fbuffer.data(), buffersize);

			// if(read <= 0 && !_evt_thread_running.load())
			if(read <= 0)
			{
				break;
			}
			
			// TODO: (Cesar): Place the new event on the event stack
			//  			  For now we just run the event callback from this thread
			//				  and see how it goes
			EvtHeader header;
			std::memcpy(&header, fbuffer.data(), sizeof(EvtHeader));
			LogTrace("[evtclient] Got Event <%d> from <0x%llx>.", header.eventid, header.client);
			
			// TODO: (Cesar): Add some userdata instead of passing nullptr
			// 				  _evt_userdata;
			auto callback = _evt_callbacks.find(header.eventid);
			if(callback != _evt_callbacks.end())
			{
				callback->second(fbuffer.data() + sizeof(EvtHeader), header.payloadsize, nullptr);
			}
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
			LogTrace("[evtclient] Emitting Event <%d> from <0x%llx> [size=%llu].", header.eventid, header.client, data.size());
			
			SocketSendBytes(socket, data.data(), data.size());
		}
	}

	std::uint16_t EvtClientThread::findEvent(const std::string& event)
	{
		auto evt = _evt_registry.find(event);
		std::uint16_t eventid = 0;
		if(evt == _evt_registry.end())
		{
			LogDebug("[evtclient] Could not find event <%s> in local registry.", event.c_str());
			LogDebug("[evtclient] Looking in server for <%s>.", event.c_str());
			std::uint16_t eid = 0;

			if(_exp)
			{
				eid = _exp->_rpc_client->call<std::uint16_t>(RPC_CALL_MULEX_EVTGETID, string32(event));
			}
			else
			{
				std::optional<const Experiment*> experiment = SysGetConnectedExperiment();
				if(!experiment.has_value())
				{
					LogError("[evtclient] Failed to emit event. Not connected to an experiment.");
					return 0;
				}

				eid = experiment.value()->_rpc_client->call<std::uint16_t>(RPC_CALL_MULEX_EVTGETID, string32(event));
			}
			if(eid != 0)
			{
				// Cache it
				_evt_registry.emplace(event, eid);
			}
			return eid;
		}
		return evt->second;
	}

	void EvtClientThread::emit(const std::string& event, const std::uint8_t* data, std::uint64_t len)
	{
		std::uint16_t eid = findEvent(event);
		if(eid == 0)
		{
			LogError("[evtclient] Failed to find event in server. Emit aborted.");
			return;
		}

		// Make header
		EvtHeader header;
		if(_evt_has_custom_id)
		{
			header.client = _evt_custom_id;
		}
		else
		{
			header.client = SysGetClientId();
		}
		header.eventid = eid;
		header.msgid = GetNextEventMessageId();
		header.payloadsize = static_cast<std::uint32_t>(len);

		LogTrace("payloadsize on client: %d", header.payloadsize);

		std::vector<std::uint8_t> vdata;
		vdata.resize(sizeof(EvtHeader) + len);

		std::memcpy(vdata.data(), &header, sizeof(EvtHeader));
		if(len > 0)
		{
			std::memcpy(vdata.data() + sizeof(EvtHeader), data, len);
		}

		// _evt_emit_stack.push(std::move(vdata));
		_evt_emit_stack.push(vdata);
	}

	void EvtClientThread::regist(const std::string& event)
	{
		// Ask server to register event via RPC
		const Experiment* exp;
		if(_exp)
		{
			exp = _exp;
		}
		else
		{
			std::optional<const Experiment*> experiment = SysGetConnectedExperiment();
			if(!experiment.has_value())
			{
				LogError("[evtclient] Failed to register event. Not connected to an experiment.");
				return;
			}
			exp = experiment.value();
		}
		
		bool reg = exp->_rpc_client->call<bool>(RPC_CALL_MULEX_EVTREGISTER, string32(event));
		if(!reg)
		{
			LogError("[evtclient] Failed to register event. EvtRegister() returned false.");
		}

		std::uint16_t eventid = exp->_rpc_client->call<std::uint16_t>(RPC_CALL_MULEX_EVTGETID, string32(event));
		if(eventid == 0)
		{
			LogError("[evtclient] Failed to register event.");
			return;
		}

		_evt_registry.emplace(event, eventid);
		LogTrace("[evtclient] Registered event <%s> with id <%d>.", event.c_str(), eventid);
	}

	void EvtClientThread::subscribe(const std::string& event, EvtCallbackFunc callback)
	{
		// Ask server for the event id via RPC
		const Experiment* exp;
		if(_exp)
		{
			exp = _exp;
		}
		else
		{
			std::optional<const Experiment*> experiment = SysGetConnectedExperiment();
			if(!experiment.has_value())
			{
				LogError("[evtclient] Failed to subscribe to event. Not connected to an experiment.");
				return;
			}
			exp = experiment.value();
		}
		
		std::uint16_t eventid = exp->_rpc_client->call<std::uint16_t>(RPC_CALL_MULEX_EVTGETID, string32(event));
		if(eventid == 0)
		{
			LogError("[evtclient] Failed to subscribe to event. Event <%s> is not registered.");
			return;
		}

		if(!exp->_rpc_client->call<bool>(RPC_CALL_MULEX_EVTSUBSCRIBE, string32(event)))
		{
			LogError("[evtclient] Failed to subscribe to event.");
			return;
		}

		// Register this subscription on the rdb
		// This is informative only and does not reflect internal state subscription
		// RdbAccess rda;
		// std::string skey = "/system/clients/" + std::to_string(SysGetClientId()) + "/events/subscribed/totalcalls";
		// if(!rda[skey].exists())
		// {
		// 	rda.create(skey, RdbValueType::UINT64, 0);
		// }
		//
		// // TODO: (Cesar): Maybe come up with a syntax for rda[key]++
		// std::uint64_t calls = rda[skey];
		// rda[skey] = calls + 1;
		//
		// std::string ckey = "/system/clients/" + std::to_string(SysGetClientId()) + "/events/subscribed/count";
		// if(!rda[ckey].exists())
		// {
		// 	rda.create(ckey, RdbValueType::UINT32, 0);
		// }
		// std::uint32_t count = rda[ckey];
		// rda[ckey] = count + 1;
		// std::string rkey = "/system/clients/" + std::to_string(SysGetClientId()) + "/events/subscribed/ids";
		// if(!rda[rkey].exists())
		// {
		// 	rda.create(rkey, RdbValueType::UINT16, nullptr, 100);
		// }
		//
		// std::vector<std::uint16_t> ids = rda[rkey];
		// ids[count] = eventid;
		// rda[rkey] = ids;

		auto eid_callbacks = _evt_callbacks.find(eventid);
		if(eid_callbacks != _evt_callbacks.end())
		{
			LogWarning("[evtclient] Subscribing to already subscribed event. Only one callback per event is allowed. Replacing...");
		}
		_evt_callbacks[eventid] = callback;
		_evt_subscriptions.insert(event);

		LogTrace("[evtclient] Subscribed to event <%s> [%d].", event.c_str(), eventid);
	}

	void EvtClientThread::unsubscribe(const std::string& event)
	{
		// Ask server for the event id via RPC
		const Experiment* exp;
		if(_exp)
		{
			exp = _exp;
		}
		else
		{
			std::optional<const Experiment*> experiment = SysGetConnectedExperiment();
			if(!experiment.has_value())
			{
				LogError("[evtclient] Failed to unsubscribe to event. Not connected to an experiment.");
				return;
			}
			exp = experiment.value();
		}
		
		std::uint16_t eventid = exp->_rpc_client->call<std::uint16_t>(RPC_CALL_MULEX_EVTGETID, string32(event));
		if(eventid == 0)
		{
			LogError("[evtclient] Failed to unsubscribe to event. Event <%s> is not registered.");
			return;
		}

		if(!exp->_rpc_client->call<bool>(RPC_CALL_MULEX_EVTUNSUBSCRIBE, string32(event)))
		{
			LogError("[evtclient] Failed to unsubscribe to event.");
			return;
		}

		auto eid_callbacks = _evt_callbacks.find(eventid);
		if(eid_callbacks == _evt_callbacks.end())
		{
			LogError("[evtclient] Failed to unsubscribe to event.");
			return;
		}
		_evt_callbacks.erase(eid_callbacks);
		_evt_subscriptions.erase(event);

		LogTrace("[evtclient] Unsubscribed to event <%s> [%d].", event.c_str(), eventid);
	}

	void EvtClientThread::unsubscribeAll()
	{
		for(const auto event : _evt_subscriptions)
		{
			unsubscribe(event);
		}
	}

	static void SetRdbClientConnectionStatus(std::uint64_t cid, bool connected)
	{
		std::string root_key = "/system/backends/" + SysI64ToHexString(cid) + "/";
		if(!RdbNewEntry(root_key + "connected", RdbValueType::BOOL, &connected))
		{
			RdbWriteValueDirect(root_key + "connected", connected);
		}
	}

	static void RegisterClientRdb(const mxstring<512>& name, const mxstring<512>& host, std::uint64_t cid)
	{
		std::string root_key = "/system/backends/" + SysI64ToHexString(cid) + "/";

		if(!RdbNewEntry(root_key + "name", RdbValueType::STRING, name.c_str()))
		{
			RdbWriteValueDirect(root_key + "name", name);
		}

		if(!RdbNewEntry(root_key + "host", RdbValueType::STRING, host.c_str()))
		{
			RdbWriteValueDirect(root_key + "host", host);
		}

		std::int64_t time = SysGetCurrentTime();
		if(!RdbNewEntry(root_key + "last_connect_time", RdbValueType::INT64, &time))
		{
			RdbWriteValueDirect(root_key + "last_connect_time", time);
		}

		if(!RdbNewEntry(root_key + "user_status/text", RdbValueType::STRING, "None"))
		{
			RdbWriteValueDirect(root_key + "user_status/text", mxstring<512>("None"));
		}

		if(!RdbNewEntry(root_key + "user_status/color", RdbValueType::STRING, "#000000"))
		{
			RdbWriteValueDirect(root_key + "user_status/color", mxstring<512>("#000000"));
		}

		std::uint32_t init = 0;
		RdbNewEntry(root_key + "statistics/event/read" , RdbValueType::UINT32, &init);
		RdbNewEntry(root_key + "statistics/event/write", RdbValueType::UINT32, &init);

		SetRdbClientConnectionStatus(cid, true);
	}

	static bool ClientIsGhost(std::uint16_t cid)
	{
		return (_evt_client_ghost.find(cid) != _evt_client_ghost.end());
	}

	static void OnClientConnectMetadata(const Socket& socket, std::uint64_t cid, std::uint16_t eid, const std::uint8_t* data, std::uint64_t size)
	{
		LogDebug("[evtserver] Registering client <0x%llx>.", cid);
		_evt_client_socket_pair[socket._handle] = cid;
		_evt_client_socket_pair_rev[cid] = socket;

		if(size == 0)
		{
			// This is a "ghost" client
			// Not registered on the rdb
			LogTrace("[evtserver] Client is ghost.");
			_evt_client_ghost.insert(cid);
			return;
		}

		{
			std::unique_lock<std::mutex> lock(_evt_client_stats_lock);
			_evt_client_stats[cid] = 0;
		}

		LogDebug("[evtserver] New connected client <%s>.", reinterpret_cast<const char*>(data));
		std::string name_data = reinterpret_cast<const char*>(data);
		auto token = name_data.find_first_of("@");
		mxstring<512> bname = name_data.substr(0, token);
		mxstring<512> hname = name_data.substr(token + 1);
		RegisterClientRdb(bname, hname, cid);
	}

	static void OnClientDisconnect(std::uint64_t cid)
	{
		for(const auto& evt : _evt_current_subscriptions)
		{
			EvtUnsubscribe(cid, evt.first);
		}

		if(ClientIsGhost(cid))
		{
			_evt_client_ghost.erase(cid);
			return; // Don't set the status
		}

		// Reset custom status
		RdbWriteValueDirect("/system/backends/" + SysI64ToHexString(cid) + "/user_status/text" , mxstring<512>("None"));
		RdbWriteValueDirect("/system/backends/" + SysI64ToHexString(cid) + "/user_status/color", mxstring<512>("#000000"));

		SetRdbClientConnectionStatus(cid, false);

		{
			std::unique_lock<std::mutex> lock(_evt_client_stats_lock);
			_evt_client_stats.erase(cid);
			RdbWriteValueDirect("/system/backends/" + SysI64ToHexString(cid) + "/statistics/event/read" , 0);
			RdbWriteValueDirect("/system/backends/" + SysI64ToHexString(cid) + "/statistics/event/write", 0);
		}
	}

	static void RegisterServerSideEvents()
	{
		// Register server side event for metadata
		EvtRegister("mxevt::getclientmeta");
		EvtServerRegisterCallback("mxevt::getclientmeta", OnClientConnectMetadata);
	}

	EvtServerThread::EvtServerThread()
	{
		_evt_thread_running.store(true);
		_evt_thread_ready.store(false);

		RegisterServerSideEvents();
		
		_evt_accept_thread = std::make_unique<std::thread>(
			std::bind(&EvtServerThread::serverConnAcceptThread, this)
		);

		_evt_stats_thread = std::make_unique<std::thread>(
			std::bind(&EvtServerThread::clientStatisticsThread, this)
		);
	}

	EvtServerThread::~EvtServerThread()
	{
		_evt_thread_running.store(false);
		_evt_accept_thread->join();
		_evt_stats_thread->join();
		std::for_each(_evt_emit_stack.begin(), _evt_emit_stack.end(), [](auto& t){ t.second.requestUnblock(); });
#ifdef WIN32
		std::for_each(_evt_stream.begin(), _evt_stream.end(), [](auto& t){ t.second->requestUnblock(); });
#endif
		std::for_each(_evt_emit_thread.begin(), _evt_emit_thread.end(), [](auto& t){ t.second->join(); });
		std::for_each(_evt_listen_thread.begin(), _evt_listen_thread.end(), [](auto& t){ t.second->join(); });
		_evt_thread_ready.store(false);
	}

	bool EvtServerThread::ready() const
	{
		return _evt_thread_ready.load();
	}

	bool EvtServerThread::emit(const std::string& event, const std::uint8_t* data, std::uint64_t len)
	{
		std::uint16_t eid = EvtGetId(event);
		if(eid == 0)
		{
			LogError("[evtserver] Failed to find event. Emit aborted.");
			return false;
		}

		auto cidit = _evt_current_subscriptions.find(eid);
		if(cidit == _evt_current_subscriptions.end())
		{
			// Not in registry
			// Silently ignore
			return false;
		}

		if(cidit->second.empty())
		{
			// Dangling event with no subscriptions
			// Silently ignore
			LogTrace("Dangling event <%s>.", event.c_str());
			return false;
		}

		// Make header
		EvtHeader header;
		header.client = SysGetClientId(); // Should be 0x00
		header.eventid = eid;
		header.msgid = GetNextEventMessageId();
		header.payloadsize = static_cast<std::uint32_t>(len);

		std::vector<std::uint8_t> vdata;
		vdata.resize(sizeof(EvtHeader) + len);

		std::memcpy(vdata.data(), &header, sizeof(EvtHeader));
		if(len > 0)
		{
			std::memcpy(vdata.data() + sizeof(EvtHeader), data, len);
		}

		for(const auto& cid : cidit->second)
		{
			_evt_emit_stack.at(_evt_client_socket_pair_rev.at(cid)).push(vdata);
		}
		return true;
	}

	void EvtServerThread::relay(const std::uint64_t clientid, const std::uint8_t* data, std::uint64_t len)
	{
		std::vector<std::uint8_t> vdata(data, data + len);
		_evt_emit_stack.at(_evt_client_socket_pair_rev.at(clientid)).push(vdata);
	}

	void EvtServerThread::serverConnAcceptThread()
	{
		_server_socket = SocketInit();
		SocketBindListen(_server_socket, EVT_PORT);
		if(!SocketSetNonBlocking(_server_socket))
		{
			LogError("Failed to set listen socket to non blocking mode.");
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

		{
			std::unique_lock<std::mutex> lock(_connections_mutex);
			_evt_stream.emplace(socket, &recvthread->_stream);
			_evt_thread_sig.emplace(socket, true);
			_evt_emit_stack.emplace(std::piecewise_construct, std::forward_as_tuple(socket), std::forward_as_tuple());
		}
		_evt_notifier.notify_one();

		static constexpr std::uint64_t buffersize = SYS_RECV_THREAD_BUFFER_SIZE;
		static std::vector<std::uint8_t> fbuffer(buffersize);

		while(_evt_thread_running.load() && _evt_thread_sig.at(socket).load())
		{
			// Read next event
			std::uint64_t read = sbs.fetch(fbuffer.data(), buffersize);

			// if(read <= 0 && (!_evt_thread_running.load() || !_evt_thread_sig.at(socket).load()))
			if(read <= 0)
			{
				break;
			}
			
			// TODO: (Cesar): Place the new event on the event stack
			//  			  For now we just run the event callback from this thread
			//				  and see how it goes
			EvtHeader header;
			std::memcpy(&header, fbuffer.data(), sizeof(EvtHeader));
			LogTrace("[evtserver] Got Event <%d> from <0x%llx>.", header.eventid, header.client);

			// Relay event to clients that are subscribed
			{
				std::unique_lock<std::mutex> lock(_evt_sub_lock);
				for(const std::uint64_t cid : _evt_current_subscriptions.at(header.eventid))
				{
					LogTrace("[evtserver] Relaying event <%d> from <0x%llx> to <0x%llx>.", header.eventid, header.client, cid);
					relay(cid, fbuffer.data(), read);
				}
			}

			// Perform server side tasks (if any)
			EvtTryRunServerCallback(header.client, header.eventid, fbuffer.data() + sizeof(EvtHeader), header.payloadsize, socket);

			if(!ClientIsGhost(header.client))
			{
				// Write statistics
				std::uint64_t upload = sizeof(EvtHeader) + header.payloadsize;
				EvtAccumulateClientStatistics(header.client, upload & 0xFFFFFFFF); // Lo DWORD
			}
		}

		// On client disconnect unsubscribe from events
		// We can run into issues if there is a crash on the client side, which we don't control
		OnClientDisconnect(_evt_client_socket_pair.at(socket._handle));

		{
			std::unique_lock<std::mutex> lock(_connections_mutex);
			_evt_emit_stack.at(socket).requestUnblock();
			_evt_stream.erase(socket);
		}
		recvthread->_handle.join();
	}

	void EvtServerThread::serverEmitThread(const Socket& socket)
	{
		{
			// Wait for _evt_thread_sig to be populated by serverListenThread
			std::unique_lock<std::mutex> lock(_connections_mutex);
			_evt_notifier.wait(lock);
		}

		while(_evt_thread_running.load() && _evt_thread_sig.at(socket).load())
		{
			// Read next event to emit from stack
			std::vector<std::uint8_t> data = _evt_emit_stack.at(socket).pop();

			// if(data.size() == 0 && (!_evt_thread_running.load() || ! _evt_thread_sig.at(socket).load()))
			if(data.size() == 0)
			{
				break;
			}
			
			// TODO: (Cesar): Place the new event on the event stack
			//  			  For now we just run the event callback from this thread
			//				  and see how it goes
			EvtHeader header;
			std::memcpy(&header, data.data(), sizeof(EvtHeader));
			std::uint64_t cid = _evt_client_socket_pair.at(socket._handle);
			LogTrace("[evtserver] Emitting Event <%d> to <0x%llx>.", header.eventid, cid);
			
			SocketSendBytes(socket, data.data(), data.size());

			if(!ClientIsGhost(cid))
			{
				// Write statistics
				std::uint64_t download = sizeof(EvtHeader) + header.payloadsize;
				EvtAccumulateClientStatistics(cid, (download & 0xFFFFFFFF) << 32); // Hi DWORD
			}
		}
	}

	bool EvtRegister(string32 name)
	{
		std::unique_lock lock(_evt_reg_lock);
		auto evtrit = _evt_server_reg.find(name.c_str());
		if(evtrit != _evt_server_reg.end())
		{
			LogTrace("[evtserver] Cannot register event <%s>. Already exists.", name.c_str());
			return false;
		}

		const std::uint16_t event_id = ++_evt_server_reg_next;
		_evt_current_subscriptions.emplace(event_id, std::set<std::uint64_t>());
		_evt_server_reg.emplace(name.c_str(), event_id);
		LogTrace("[evtserver] Registered event <%s> [id=%d].", name.c_str(), event_id);
		return true;
	}

	std::uint16_t EvtGetId(mulex::string32 name)
	{
		std::shared_lock lock(_evt_reg_lock);
		auto evtrit = _evt_server_reg.find(name.c_str());
		if(evtrit != _evt_server_reg.end())
		{
			return evtrit->second;
		}

		LogError("[evtserver] Cannot get event id. Event <%s> is not registered.", name.c_str());
		return 0;
	}

	bool EvtSubscribe(mulex::string32 name)
	{
		std::uint16_t eid = EvtGetId(name);
		std::uint64_t cid = GetCurrentCallerId();
		if(eid == 0)
		{
			LogError("[evtserver] Cannot subscribe to event <%s>.", name.c_str());
			return false;
		}

		if(cid == 0)
		{
			LogError("[evtserver] Mxserver cannot manually subscribe to events.");
			return false;
		}

		std::unique_lock<std::mutex> lock(_evt_sub_lock);
		_evt_current_subscriptions.at(eid).insert(cid);
		LogTrace("[evtserver] Subscribed <0x%llx> to event <%s> [id=%d].", cid, name.c_str(), eid);
		return true;
	}

	void EvtServerRegisterCallback(
		mulex::string32 name,
		std::function<void(const Socket&, std::uint64_t, std::uint16_t, const std::uint8_t*, std::uint64_t)> callback
	)
	{
		std::uint16_t eid = EvtGetId(name);
		if(eid == 0)
		{
			return;
		}
		_evt_server_callbacks.insert_or_assign(eid, callback);
	}

	void EvtTryRunServerCallback(std::uint64_t clientid, std::uint16_t eventid, const std::uint8_t* data, std::uint64_t len, const Socket& socket)
	{
		auto eidit = _evt_server_callbacks.find(eventid);
		if(eidit != _evt_server_callbacks.end())
		{
			eidit->second(socket, clientid, eventid, data, len);
		}
	}

	void EvtServerThread::clientStatisticsThread()
	{
		// Loop
		while(_evt_thread_running.load())
		{
			std::int64_t start = SysGetCurrentTime();

			{
				std::unique_lock<std::mutex> lock(_evt_client_stats_lock);
				for(auto it = _evt_client_stats.begin(); it != _evt_client_stats.end(); it++)
				{
					std::uint64_t stats = it->second.exchange(0);
					std::uint32_t upload = static_cast<std::uint32_t>(stats & 0xFFFFFFFF);
					std::uint32_t download = static_cast<std::uint32_t>(stats >> 32);

					std::string key = "/system/backends/";
					key += SysI64ToHexString(it->first);
					key += "/statistics/event";

					// TODO: (Cesar) Entry is always valid here (skip checks)
					//				 Have some write unsafe kind of function
					RdbWriteValueDirect(key + "/read", download);
					RdbWriteValueDirect(key + "/write", upload);
				}
			}

			// Every second
			std::this_thread::sleep_for(std::chrono::milliseconds(1000 - ((SysGetCurrentTime() - start))));
		}
	}

	void EvtAccumulateClientStatistics(std::uint64_t clientid, std::uint64_t framebytes)
	{
		_evt_client_stats[clientid] += framebytes; // Atomic op
	}

	mulex::RPCGenericType EvtGetAllRegisteredEvents()
	{
		std::shared_lock lock(_evt_reg_lock);
		std::vector<mxstring<512>> output;
		output.reserve(_evt_server_reg.size());
		for(const auto& reg : _evt_server_reg)
		{
			output.push_back(reg.first);
		}
		return output;
	}

	bool EvtUnsubscribe(mulex::string32 name)
	{
		std::uint16_t eid = EvtGetId(name);
		return EvtUnsubscribe(GetCurrentCallerId(), eid);
	}

	bool EvtUnsubscribe(std::uint64_t clientid, std::uint16_t eventid)
	{
		if(clientid == 0)
		{
			LogError("[evtserver] Mxserver cannot manually unsubscribe from events.");
			return false;
		}

		std::unique_lock<std::mutex> lock(_evt_sub_lock);
		auto eidit = _evt_current_subscriptions.find(eventid);
		if(eidit == _evt_current_subscriptions.end())
		{
			LogError("[evtserver] Error unsubscribing. Not subscribed to event <%d>.", eventid);
			return false;
		}

		auto cidsub = eidit->second.find(clientid);
		if(cidsub == eidit->second.end())
		{
			// Client is not subscribed to event
			// Silently ignore
			return false;
		}
		
		eidit->second.erase(clientid);
		LogTrace("[evtserver] Unsubscribing client <0x%llx> from event <%d>.", clientid, eventid);
		return true;
	}
}
