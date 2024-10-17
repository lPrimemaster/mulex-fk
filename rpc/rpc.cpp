// Author : César Godinho
// Date   : 09/10/2024
// Brief  : RPC calls from and to a client via TCP/IP

#include <atomic>
#include <functional>
#include <rpcspec.inl>

#include "rpc.h"
#include "../mxlogger.h"

static std::atomic<std::uint64_t> _client_msg_id = 0;

namespace mulex
{
	std::uint64_t GetNextMessageId()
	{
		return _client_msg_id++;
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
		
		// Read a message
		while(_rpc_thread_running.load())
		{
			// Read the header
			RPCMessageHeader header;
			SocketResult result = SocketRecvBytes(
				socket,
				reinterpret_cast<std::uint8_t*>(&header),
				RPC_MESSAGE_HEADER_SIZE,
				&_rpc_thread_running
			);

			if(result == SocketResult::ERROR)
			{
				continue;
			}
			else if(result == SocketResult::DISCONNECT)
			{
				break;
			}

			LogTrace("[rpcthread] Got RPC Header:");
			LogTrace("[rpcthread] \tClient: %s", header.client);
			LogTrace("[rpcthread] \tMethodid: %d", header.procedureid);
			LogTrace("[rpcthread] \tMsgid: %llu", header.msgid);
			LogTrace("[rpcthread] \tPayloadsz: %lu", header.payloadsize);

			// Read the data
			// This should not block as the data should be available
			std::vector<std::uint8_t> buffer(header.payloadsize);
			if(header.payloadsize > 0)
			{
				SocketRecvBytes(
					socket,
					buffer.data(),
					header.payloadsize,
					&_rpc_thread_running
				);
			}

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

		// Spawn a thread to manage this one's exit
		// Join this thread and clear connection from list
		std::thread([&](){
			std::lock_guard<std::mutex> lock(_connections_mutex);
			_rpc_thread.at(socket)->join();
			_rpc_thread.erase(socket);
		}).detach();
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
		std::for_each(_rpc_thread.begin(), _rpc_thread.end(), [](auto& t){ t.second->join(); });
		_rpc_thread_ready.store(false);
	}
}
