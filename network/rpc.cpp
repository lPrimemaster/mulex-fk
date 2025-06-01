// Author : César Godinho
// Date   : 09/10/2024
// Brief  : RPC calls from and to a client via TCP/IP

#include <atomic>
#include <cstdint>
#include <functional>
#include <algorithm>

#include "rpc.h"
#include <mutex>
#include <rpcspec.inl>

#include "../mxlogger.h"

static std::atomic<std::uint64_t> _client_msg_id = 0;

// NOTE: (Cesar) In theory this could be thread unsafe
// 				 But this practically guards vs using
// 				 RPC calls on a local context
// static std::atomic<std::uint64_t> _client_current_caller = 0;
static std::map<std::thread::id, std::uint64_t> _client_current_caller;
static std::map<std::uint64_t, std::string> 	_client_current_user;
static std::shared_mutex 						_client_current_user_lock;

static std::map<mulex::RpcCallerStatDescriptor, std::uint64_t> _rpc_statistics;

namespace mulex
{
	RPCGenericType RPCGenericType::FromData(const std::uint8_t* ptr, std::uint64_t size)
	{
		ZoneScoped;
		RPCGenericType rgt;
		rgt._data.resize(size);
		std::memcpy(rgt._data.data(), ptr, size);
		return rgt;
	}

	RPCGenericType RPCGenericType::FromData(const std::vector<std::uint8_t>& buffer)
	{
		ZoneScoped;
		RPCGenericType rgt;
		rgt._data = std::move(buffer);
		return rgt;
	}

	std::uint64_t GetNextMessageId()
	{
		return _client_msg_id++;
	}

	std::uint64_t GetCurrentCallerId()
	{
		auto cid = _client_current_caller.find(std::this_thread::get_id());
		if(cid == _client_current_caller.end())
		{
			// 0x00 Is Server
			return 0x00;
		}

		return cid->second;
	}

	std::string GetCurrentCallerUser()
	{
		std::shared_lock lock(_client_current_user_lock);
		std::uint64_t cid = GetCurrentCallerId();

		// Local calls cannot have a user
		if(cid == 0x00)
		{
			return "";
		}

		auto user = _client_current_user.find(cid);
		if(user != _client_current_user.end())
		{
			return user->second;
		}
		
		// This can also get triggered if
		// we are using the public RPC API
		return "";
	}

	void RpcAssignUserToClientId(const std::string& username, std::uint64_t cid)
	{
		std::unique_lock lock(_client_current_user_lock);
		_client_current_user.insert_or_assign(cid, username);
	}

	void RpcPurgeUserFromCid(std::uint64_t cid)
	{
		std::unique_lock lock(_client_current_user_lock);
		_client_current_user.erase(cid);
	}

	RPCClientThread::RPCClientThread(const std::string& hostname, std::uint16_t rpcport, std::uint64_t customid, const std::string& username)
	{
		ZoneScoped;
		if(customid > 0)
		{
			_rpc_has_custom_id = true;
			_rpc_custom_id = customid;
			LogDebug("[rpcclient] Client spawned with custom id.");

			// NOTE: (Cesar) Sharing the same memory space as the server
			if(!username.empty())
			{
				_rpc_username = username;

				// Custom ids are real users
				RpcAssignUserToClientId(_rpc_username, _rpc_custom_id);
			}
		}

		_rpc_socket = SocketInit();
		SocketConnect(_rpc_socket, hostname, rpcport);
		_rpc_thread_running.store(true);
		_rpc_thread = std::make_unique<std::thread>(
			std::bind(&RPCClientThread::clientThread, this, _rpc_socket)
		);
	}

	RPCClientThread::~RPCClientThread()
	{
		ZoneScoped;

		// NOTE: (Cesar) Sharing the same memory space as the server
		if(_rpc_has_custom_id)
		{
			RpcPurgeUserFromCid(_rpc_custom_id);
		}
		_rpc_thread_running.store(false);
		_rpc_stream->requestUnblock();
		SocketClose(_rpc_socket);
		_rpc_thread->join();
	}

	void RPCClientThread::clientThread(const Socket& socket)
	{
		ZoneScoped;
		std::unique_ptr<SysRecvThread> recvthread = SysStartRecvThread(socket, sizeof(RPCReturnValue), offsetof(RPCReturnValue, payloadsize));
		SysByteStream& sbs = recvthread->_stream;
		_rpc_stream = &recvthread->_stream;
		static constexpr std::uint64_t buffersize = SYS_RECV_THREAD_BUFFER_SIZE;
		static std::vector<std::uint8_t> fbuffer(buffersize);
		
		while(_rpc_thread_running.load())
		{
			// Read the message
			std::uint64_t read = sbs.fetch(fbuffer.data(), buffersize);

			if(read <= 0 && !_rpc_thread_running.load())
			{
				break;
			}
			
			// Read the header
			RPCReturnValue header;
			std::memcpy(&header, fbuffer.data(), sizeof(RPCReturnValue));

			// Read the payload (if any)
			std::vector<std::uint8_t> buffer(header.payloadsize);
			if(header.payloadsize > 0)
			{
				std::memcpy(buffer.data(), fbuffer.data() + sizeof(RPCReturnValue), header.payloadsize);
				_call_return_stack.push(std::move(buffer));
			}

			// LogTrace("[rpcclient] Got RPC Result:");
			// LogTrace("[rpcclient] \tStatus: %d", static_cast<int>(header.status));
			// LogTrace("[rpcclient] \tPayloadsz: %lu", header.payloadsize);
		}

		recvthread->_handle.join();
	}

	bool RPCServerThread::ready() const
	{
		return _rpc_thread_ready.load();
	}

	void RPCServerThread::serverConnAcceptThread()
	{
		ZoneScoped;
		_server_socket = SocketInit();
		SocketBindListen(_server_socket, RPC_PORT);
		if(!SocketSetNonBlocking(_server_socket))
		{
			LogError("Failed to set listen socket to non blocking mode.");
			SocketClose(_server_socket);
			return;
		}

		bool would_block;
		_rpc_thread_ready.store(true);
		while(_rpc_thread_running.load())
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
				// Push a new client and start a thread for it
				std::lock_guard<std::mutex> lock(_connections_mutex);
				_rpc_thread.emplace(
					client,
					std::make_unique<std::thread>(std::bind(&RPCServerThread::serverThread, this, client))
				);
			}

			// Ignore and keep trying
			// SocketAccept() already handles errors
		}

		SocketClose(_server_socket);
	}

	void RPCServerThread::serverThread(const Socket& socket)
	{
		ZoneScoped;
		// NOTE:
		// This thread will run for every and each client
		// Make sure RPC calls have locked data when accessing
		std::unique_ptr<SysRecvThread> recvthread = SysStartRecvThread(socket, RPC_MESSAGE_HEADER_SIZE, offsetof(RPCMessageHeader, payloadsize));
		SysByteStream& sbs = recvthread->_stream;

		{
			std::lock_guard<std::mutex> lock(_connections_mutex);
			_rpc_stream.emplace(socket, &recvthread->_stream);
			_rpc_thread_sig.emplace(socket, true);
			_client_current_caller.emplace(std::this_thread::get_id(), 0x00);
		}

		static constexpr std::uint64_t buffersize = SYS_RECV_THREAD_BUFFER_SIZE;
		std::vector<std::uint8_t> fbuffer(buffersize);

		while(_rpc_thread_running.load() && _rpc_thread_sig.at(socket).load())
		{
			// Read the message
			std::uint64_t read = sbs.fetch(fbuffer.data(), buffersize);
			// if(read <= 0 && (!_rpc_thread_running.load() || !_rpc_thread_sig.at(socket).load()))
			if(read <= 0)
			{
				break;
			}

			// Read the header
			RPCMessageHeader header;
			std::memcpy(&header, fbuffer.data(), RPC_MESSAGE_HEADER_SIZE);

			// Read the payload (if any)
			std::vector<std::uint8_t> buffer(header.payloadsize);
			if(header.payloadsize > 0)
			{
				std::memcpy(buffer.data(), fbuffer.data() + RPC_MESSAGE_HEADER_SIZE, header.payloadsize);
			}

			LogTrace("[rpcserver] Got RPC Call <%d> from <0x%llx>.", header.procedureid, header.client);
			RpcAccumulateCallStatistics(header.client, header.procedureid);

			// Set the current global client state
			// This works if we have multiple threads serving RPC requests
			_client_current_caller.at(std::this_thread::get_id()) = header.client;
		
			// Execute the request locally on the RPC thread
			std::vector<std::uint8_t> ret = RPCCallLocally(header.procedureid, buffer.data());

			// Pop the current global client state
			_client_current_caller.at(std::this_thread::get_id()) = 0x00;

			RPCReturnValue response;
			response.status = RPCResult::OK;
			response.payloadsize = ret.size();

			// Is there a return type
			if(response.payloadsize > 0)
			{
				std::vector<std::uint8_t> buffer(ret.size() + sizeof(RPCReturnValue));
				std::memcpy(buffer.data(), &response, sizeof(RPCReturnValue));
				std::memcpy(buffer.data() + sizeof(RPCReturnValue), ret.data(), response.payloadsize);
				SocketSendBytes(
					socket,
					buffer.data(),
					response.payloadsize + sizeof(RPCReturnValue)
				);
			}
			else
			{
				SocketSendBytes(
					socket,
					reinterpret_cast<std::uint8_t*>(&response),
					sizeof(RPCReturnValue)
				);
			}
		}

		{
			std::lock_guard<std::mutex> lock(_connections_mutex);
			_client_current_caller.erase(std::this_thread::get_id());
		}

		recvthread->_handle.join();
	}

	RPCServerThread::RPCServerThread()
	{
		ZoneScoped;
		// Ensure we setup the thread spin flag
		_rpc_thread_running.store(true);
		_rpc_thread_ready.store(false);

		// Init the listen thread for this client
		_rpc_accept_thread = std::make_unique<std::thread>(
			std::bind(&RPCServerThread::serverConnAcceptThread, this)
		);
	}

	RPCServerThread::~RPCServerThread()
	{
		ZoneScoped;
		_rpc_thread_running.store(false);
		_rpc_accept_thread->join();
#ifdef WIN32
		std::for_each(_rpc_stream.begin(), _rpc_stream.end(), [](auto& t){ t.second->requestUnblock(); });
#endif
		std::for_each(_rpc_thread.begin(), _rpc_thread.end(), [](auto& t){ t.second->join(); });
		_rpc_thread_ready.store(false);
	}

	bool RpcCallerStatDescriptor::operator<(const RpcCallerStatDescriptor& other) const
	{
		return (client < other.client) || (client == other.client && procid < other.procid);
	}

	void RpcAccumulateCallStatistics(std::uint64_t client, std::uint16_t procid)
	{
		RpcCallerStatDescriptor descriptor{ client, procid };

		auto it = _rpc_statistics.find(descriptor);

		if(it == _rpc_statistics.end())
		{
			_rpc_statistics.emplace(descriptor, 1ULL);
			return;
		}

		it->second++;
	}

	mulex::RPCGenericType RpcGetAllCalls()
	{
		static std::mutex _mtx;
		std::unique_lock lock(_mtx);

		static std::vector<mulex::mxstring<512>> method_signatures;
		if(method_signatures.empty())
		{
			for(const auto& method : RPCGetMethods())
			{
				method_signatures.push_back(method);
			}
		}

		return method_signatures;
	}

	mulex::RPCGenericType RpcGetCallsDebugData()
	{
		std::vector<std::uint8_t> output;
		constexpr std::uint64_t size = (2 * sizeof(std::uint64_t) + sizeof(std::uint16_t));
		output.resize(_rpc_statistics.size() * size);
		std::uint8_t* data = output.data();
		for(const auto& desc : _rpc_statistics)
		{
			std::memcpy(data, &desc.first.client, sizeof(uint64_t));
			std::memcpy(data + sizeof(uint64_t), &desc.first.procid, sizeof(uint16_t));
			std::memcpy(data + sizeof(uint64_t) + sizeof(uint16_t), &desc.second, sizeof(uint64_t));
			data += size;
		}
		return output;
	}
}
