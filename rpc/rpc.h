#pragma once
#include <cstdint>
#include <thread>
#include <atomic>
#include <mutex>
#include <map>

#include "socket.h"
#include "../mxtypes.h"
#include "../mxlogger.h"
#include "../mxsystem.h"

namespace mulex
{
	static constexpr std::uint16_t RPC_PORT = 5701;
	static constexpr std::uint16_t RPC_RECV_TIMEOUT = 10000; // 10 sec

	enum class RPCResult
	{
		OK,
		WRONG_ARGS,
		TIMEOUT
	};
	
	struct RPCMessageHeader
	{
		string32 	  client;
		std::uint16_t procedureid;
		std::uint64_t msgid;
		std::uint32_t payloadsize;
		std::uint8_t  padding[8];
	};

	struct RPCReturnValue
	{
		RPCResult 	  status;
		std::uint32_t payloadsize;
	};

	constexpr std::uint64_t RPC_MESSAGE_HEADER_SIZE = sizeof(RPCMessageHeader);

	std::uint64_t GetNextMessageId();

	template<typename T, typename... Args>
	inline T CallRemoteFunction(std::uint16_t procedureid, Args&&... args)
	{
		std::optional<const mulex::Experiment*> cexperiment = SysGetConnectedExperiment();
		if(!cexperiment.has_value())
		{
			LogError("Failed to call remote function. Could not get a connected experiment.");
			return T();
		}
		const mulex::Socket& conn = cexperiment.value()->_rpc_socket;
		mulex::RPCMessageHeader header;
		header.procedureid = procedureid;
		header.msgid = GetNextMessageId();
		header.payloadsize = static_cast<std::uint32_t>(SysVargSize<Args...>());

		mulex::SocketResult result;
		result = mulex::SocketSendBytes(conn, (std::uint8_t*)&header, sizeof(header));
		if(result == mulex::SocketResult::ERROR)
		{
			mulex::LogError("CallRemoteFunction failed to send header data.");
			return T();
		}

		if constexpr(sizeof...(args) > 0)
		{
			std::vector<std::uint8_t> params = SysPackArguments(args...);
			result = mulex::SocketSendBytes(conn, params.data(), header.payloadsize);
			if(result == mulex::SocketResult::ERROR)
			{
				mulex::LogError("CallRemoteFunction failed to send payload.");
				return T();
			}
		}

		mulex::RPCReturnValue ret;
		T out; // NOTE: T needs to be trivially constructible (and copyable)
		result = mulex::SocketRecvBytes(conn, (std::uint8_t*)&ret, sizeof(mulex::RPCReturnValue), nullptr, 1000);
		if(result == mulex::SocketResult::ERROR)
		{
			mulex::LogError("CallRemoteFunction failed to receive answer.");
			return T();
		}

		if(ret.payloadsize > 0)
		{
			result = mulex::SocketRecvBytes(conn, (std::uint8_t*)&out, sizeof(T), nullptr, 1000);
			if(result == mulex::SocketResult::ERROR)
			{
				mulex::LogError("CallRemoteFunction failed to receive return data.");
				return T();
			}
			mulex::LogTrace("Got data: %x", &out);
		}
		return out;
	}

	template<typename... Args>
	inline void CallRemoteFunction(std::uint16_t procedureid, Args&&... args)
	{
		std::optional<const mulex::Experiment*> cexperiment = SysGetConnectedExperiment();
		if(!cexperiment.has_value())
		{
			LogError("Failed to call remote function. Could not get a connected experiment.");
			return;
		}
		const mulex::Socket& conn = cexperiment.value()->_rpc_socket;
		mulex::RPCMessageHeader header;
		header.procedureid = procedureid;
		header.msgid = GetNextMessageId();
		header.payloadsize = static_cast<std::uint32_t>(SysVargSize<Args...>());

		mulex::SocketResult result;
		result = mulex::SocketSendBytes(conn, (std::uint8_t*)&header, sizeof(header));
		if(result == mulex::SocketResult::ERROR)
		{
			mulex::LogError("CallRemoteFunction failed to send header data.");
			return;
		}

		if constexpr(sizeof...(args) > 0)
		{
			std::vector<std::uint8_t> params = SysPackArguments(args...);
			result = mulex::SocketSendBytes(conn, params.data(), header.payloadsize);
			if(result == mulex::SocketResult::ERROR)
			{
				mulex::LogError("CallRemoteFunction failed to send payload.");
				return;
			}
		}

		mulex::RPCReturnValue ret;
		result = mulex::SocketRecvBytes(conn, (std::uint8_t*)&ret, sizeof(mulex::RPCReturnValue), nullptr, 1000);
		if(result == mulex::SocketResult::ERROR)
		{
			mulex::LogError("CallRemoteFunction failed to receive answer.");
		}
	}

	class RPCServerThread
	{
	public:
		RPCServerThread();
		~RPCServerThread();
		bool ready() const;

	private:
		void serverConnAcceptThread();
		void serverThread(const Socket& socket);

	private:
		Socket _server_socket;
		std::map<Socket, std::unique_ptr<std::thread>> _rpc_thread;
		std::unique_ptr<std::thread> _rpc_accept_thread;
		std::atomic<bool> _rpc_thread_running = false;
		std::atomic<bool> _rpc_thread_ready = false;
		std::mutex _connections_mutex;
	};
} // namespace mulex
