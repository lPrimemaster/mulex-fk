// Author : César Godinho
// Date   : 09/10/2024
// Brief  : RPC calls from and to a client via TCP/IP

#include <atomic>
#include <functional>

#include "rpc.h"
#include <rpcspec.inl>

#include "../mxlogger.h"

static std::atomic<std::uint64_t> _client_msg_id = 0;

namespace mulex
{

	RPCGenericType RPCGenericType::FromData(const std::uint8_t* ptr, std::uint64_t size)
	{
		RPCGenericType rgt;
		rgt._data.resize(size);
		std::memcpy(rgt._data.data(), ptr, size);
		return rgt;
	}

	RPCGenericType RPCGenericType::FromData(const std::vector<std::uint8_t>& buffer)
	{
		RPCGenericType rgt;
		rgt._data = std::move(buffer);
		return rgt;
	}

	std::uint64_t GetNextMessageId()
	{
		return _client_msg_id++;
	}

	RPCClientThread::RPCClientThread(const std::string& hostname, std::uint16_t rpcport)
	{
		_rpc_socket = SocketInit();
		SocketConnect(_rpc_socket, hostname, rpcport);
		_rpc_thread_running.store(true);
		_rpc_thread = std::make_unique<std::thread>(
			std::bind(&RPCClientThread::clientThread, this, _rpc_socket)
		);
	}

	RPCClientThread::~RPCClientThread()
	{
		_rpc_thread_running.store(false);
		_rpc_stream->requestUnblock();
		SocketClose(_rpc_socket);
		_rpc_thread->join();
	}

	void RPCClientThread::clientThread(const Socket& socket)
	{
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

			LogTrace("[rpcclient] Got RPC Result:");
			LogTrace("[rpcclient] \tStatus: %d", static_cast<int>(header.status));
			LogTrace("[rpcclient] \tPayloadsz: %lu", header.payloadsize);
		}

		recvthread->_handle.join();
	}

	bool RPCServerThread::ready() const
	{
		return _rpc_thread_ready.load();
	}

	void RPCServerThread::serverConnAcceptThread()
	{
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
		// NOTE:
		// This thread will run for every and each client
		// Make sure RPC calls have locked data when accessing
		std::unique_ptr<SysRecvThread> recvthread = SysStartRecvThread(socket, RPC_MESSAGE_HEADER_SIZE, offsetof(RPCMessageHeader, payloadsize));
		SysByteStream& sbs = recvthread->_stream;

		{
			std::lock_guard<std::mutex> lock(_connections_mutex);
			_rpc_stream.emplace(socket, &recvthread->_stream);
			_rpc_thread_sig.emplace(socket, true);
		}

		static constexpr std::uint64_t buffersize = 8192;
		std::uint8_t fbuffer[buffersize];

		while(_rpc_thread_running.load() && _rpc_thread_sig.at(socket).load())
		{
			// Read the message
			std::uint64_t read = sbs.fetch(fbuffer, buffersize);
			if(read <= 0 && (!_rpc_thread_running.load() || !_rpc_thread_sig.at(socket).load()))
			{
				break;
			}

			// Read the header
			RPCMessageHeader header;
			std::memcpy(&header, fbuffer, RPC_MESSAGE_HEADER_SIZE);

			// Read the payload (if any)
			std::vector<std::uint8_t> buffer(header.payloadsize);
			if(header.payloadsize > 0)
			{
				std::memcpy(buffer.data(), fbuffer + RPC_MESSAGE_HEADER_SIZE, header.payloadsize);
			}

			LogTrace("[rpcserver] Got RPC Header:");
			// LogTrace("[rpcserver] \tClient: %s", header.client);
			LogTrace("[rpcserver] \tMethodid: %d", header.procedureid);
			LogTrace("[rpcserver] \tMsgid: %llu", header.msgid);
			LogTrace("[rpcserver] \tPayloadsz: %lu", header.payloadsize);


			// Execute the request locally on the RPC thread
			std::vector<std::uint8_t> ret = RPCCallLocally(header.procedureid, buffer.data());

			RPCReturnValue response;
			response.status = RPCResult::OK;
			response.payloadsize = ret.size();

			// Acknowledge the execution
			SocketSendBytes(
				socket,
				reinterpret_cast<std::uint8_t*>(&response),
				sizeof(RPCReturnValue)
			);

			// Is there a return type
			if(response.payloadsize > 0)
			{
				SocketSendBytes(
					socket,
					ret.data(),
					response.payloadsize
				);
			}
		}

		recvthread->_handle.join();
	}

	RPCServerThread::RPCServerThread()
	{
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
		_rpc_thread_running.store(false);
		_rpc_accept_thread->join();
		std::for_each(_rpc_stream.begin(), _rpc_stream.end(), [](auto& t){ t.second->requestUnblock(); });
		std::for_each(_rpc_thread.begin(), _rpc_thread.end(), [](auto& t){ t.second->join(); });
		_rpc_thread_ready.store(false);
	}
}
