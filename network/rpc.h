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
		std::uint64_t client;
		std::uint16_t procedureid;
		std::uint64_t msgid;
		std::uint32_t payloadsize;
		// std::uint8_t  padding[10];
	};

	struct RPCReturnValue
	{
		RPCResult 	  status;
		std::uint32_t payloadsize;
	};

	struct RPCGenericType
	{
		static RPCGenericType FromData(const std::uint8_t* ptr, std::uint64_t size);
		static RPCGenericType FromData(const std::vector<std::uint8_t>& buffer);

		RPCGenericType() = default;

		template<typename T>
		/* implicit */ RPCGenericType(const T& t)
		{
			static_assert(std::is_trivially_copyable_v<T>, "RPCGenericType, type must be trivially copyable.");
			static_assert(!std::is_pointer_v<T>, "RPCGenericType, type must not be a pointer.");
			_data.resize(sizeof(T));
			std::memcpy(_data.data(), &t, sizeof(T));
		}

		/* implicit */ RPCGenericType(const std::nullptr_t& t)
		{
			_data.clear();
		}
		
		template<typename T>
		/* implicit */ RPCGenericType(const std::vector<T>& vt)
		{
			static_assert(std::is_trivially_copyable_v<T>, "RPCGenericType, type must be trivially copyable.");
			static_assert(!std::is_pointer_v<T>, "RPCGenericType, type must not be a pointer.");
			_data.resize(sizeof(T) * vt.size());
			std::memcpy(_data.data(), vt.data(), sizeof(T) * vt.size());
		}

		template<typename T>
		operator T() const
		{
			return asType<T>();
		}

		template<typename T>
		operator std::vector<T>() const
		{
			return asVectorType<T>();
		}

		template<typename T>
		RPCGenericType& operator=(const T& t)
		{
			static_assert(std::is_trivially_copyable_v<T>, "RPCGenericType, type must be trivially copyable.");
			static_assert(!std::is_pointer_v<T>, "RPCGenericType, type must not be a pointer.");
			_data.resize(sizeof(T));
			std::memcpy(_data.data(), &t, sizeof(T));
			return *this;
		}

		template<typename T>
		RPCGenericType& operator=(const std::vector<T>& vt)
		{
			static_assert(std::is_trivially_copyable_v<T>, "RPCGenericType, type must be trivially copyable.");
			static_assert(!std::is_pointer_v<T>, "RPCGenericType, type must not be a pointer.");
			_data.resize(sizeof(T) * vt.size());
			std::memcpy(_data.data(), vt.data(), sizeof(T) * vt.size());
			return *this;
		}

		template<typename T>
		T asType() const
		{
			static_assert(std::is_trivially_copyable_v<T>, "RPCGenericType, type must be trivially copyable.");
			static_assert(!std::is_pointer_v<T>, "RPCGenericType, type must not be a pointer.");

			if(_data.empty())
			{
				LogError("Failed to cast an empty data to requested type");
				LogWarning("Returning default constructed object");
				return T();
			}

			return *reinterpret_cast<const T*>(_data.data());
		}

		template<typename T>
		std::vector<T> asVectorType() const
		{
			static_assert(std::is_trivially_copyable_v<T>, "RPCGenericType, type must be trivially copyable.");
			static_assert(!std::is_pointer_v<T>, "RPCGenericType, type must not be a pointer.");

			if(_data.empty())
			{
				LogError("Failed to cast an empty data to requested type");
				LogWarning("Returning default constructed object");
				return std::vector<T>();
			}

			std::vector<T> ret;
			ret.resize(_data.size() / sizeof(T));
			std::memcpy(ret.data(), _data.data(), _data.size());
			return ret;
		}

		template<typename T>
		T* asPointer()
		{
			static_assert(std::is_trivially_copyable_v<T>, "RPCGenericType, type must be trivially copyable.");
			static_assert(!std::is_pointer_v<T>, "RPCGenericType, type must not be a pointer.");

			if(_data.empty())
			{
				LogError("Failed to cast an empty data to requested type");
				LogWarning("Returning default constructed object");
				return nullptr;
			}

			return reinterpret_cast<T*>(_data.data());
		}

		std::uint8_t* getData()
		{
			return _data.empty() ? nullptr : _data.data();
		}

		std::vector<std::uint8_t> _data;
	};

	constexpr std::uint64_t RPC_MESSAGE_HEADER_SIZE = sizeof(RPCMessageHeader);

	std::uint64_t GetNextMessageId();
	std::uint64_t GetCurrentCallerId();

	class RPCClientThread
	{
	public:
		RPCClientThread(const std::string& hostname, std::uint16_t rpcport = RPC_PORT);
		~RPCClientThread();

		template<typename T, typename... Args>
		inline T call(std::uint16_t procedureid, Args&&... args);

		template<typename... Args>
		inline void call(std::uint16_t procedureid, Args&&... args);

	private:
		void clientThread(const Socket& socket);

	private:
		Socket _rpc_socket;
		std::unique_ptr<std::thread> _rpc_thread;
		SysByteStream* _rpc_stream;
		std::atomic<bool> _rpc_thread_running = false;
		std::atomic<bool> _rpc_thread_ready = false;
		SysBufferStack _call_return_stack;
	};

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
		std::map<Socket, SysByteStream*> _rpc_stream;
		std::map<Socket, std::atomic<bool>> _rpc_thread_sig;
		std::unique_ptr<std::thread> _rpc_accept_thread;
		std::atomic<bool> _rpc_thread_running = false;
		std::atomic<bool> _rpc_thread_ready = false;
		std::mutex _connections_mutex;
	};

	template<typename T, typename... Args>
	inline T RPCClientThread::call(std::uint16_t procedureid, Args&&... args)
	{
		const mulex::Socket& conn = _rpc_socket;
		mulex::RPCMessageHeader header;
		header.client = SysGetClientId();
		header.procedureid = procedureid;
		header.msgid = GetNextMessageId();
		if constexpr(sizeof...(args) > 0)
		{
			header.payloadsize = static_cast<std::uint32_t>(SysVargSize<Args...>());
		}
		else
		{
			header.payloadsize = 0;
		}

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

		std::vector<std::uint8_t> payload = _call_return_stack.pop();
		if constexpr(std::is_same_v<T, mulex::RPCGenericType>)
		{
			mulex::RPCGenericType rgt;
			std::uint64_t size;

			std::memcpy(&size, payload.data(), sizeof(std::uint64_t));
			rgt._data.resize(size);
			std::memcpy(rgt._data.data(), payload.data() + sizeof(std::uint64_t), size);

			mulex::LogTrace("Got data: %x", &rgt);
			return rgt;
		}
		else
		{
			T out; // NOTE: T needs to be trivially constructible (and copyable)
			std::memcpy(&out, payload.data(), sizeof(T));
			return out;
		}
	}

	template<typename... Args>
	inline void RPCClientThread::call(std::uint16_t procedureid, Args&&... args)
	{
		const mulex::Socket& conn = _rpc_socket;
		mulex::RPCMessageHeader header;
		header.client = SysGetClientId();
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
	}
} // namespace mulex
