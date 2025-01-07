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

#include <tracy/Tracy.hpp>

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
			ZoneScoped;
			static_assert(std::is_trivially_copyable_v<T>, "RPCGenericType, type must be trivially copyable.");
			static_assert(!std::is_pointer_v<T>, "RPCGenericType, type must not be a pointer.");
			_data.resize(sizeof(T));
			std::memcpy(_data.data(), &t, sizeof(T));
		}

		/* implicit */ RPCGenericType(const std::nullptr_t& t)
		{
			ZoneScoped;
			_data.clear();
		}
		
		template<typename T>
		/* implicit */ RPCGenericType(const std::vector<T>& vt)
		{
			ZoneScoped;
			static_assert(std::is_trivially_copyable_v<T>, "RPCGenericType, type must be trivially copyable.");
			static_assert(!std::is_pointer_v<T>, "RPCGenericType, type must not be a pointer.");
			_data.resize(sizeof(T) * vt.size());
			std::memcpy(_data.data(), vt.data(), sizeof(T) * vt.size());
		}

		template<typename T>
		operator T() const
		{
			ZoneScoped;
			return asType<T>();
		}

		template<typename T>
		operator std::vector<T>() const
		{
			ZoneScoped;
			return asVectorType<T>();
		}

		template<typename T>
		RPCGenericType& operator=(const T& t)
		{
			ZoneScoped;
			static_assert(std::is_trivially_copyable_v<T>, "RPCGenericType, type must be trivially copyable.");
			static_assert(!std::is_pointer_v<T>, "RPCGenericType, type must not be a pointer.");
			_data.resize(sizeof(T));
			std::memcpy(_data.data(), &t, sizeof(T));
			return *this;
		}

		template<typename T>
		RPCGenericType& operator=(const std::vector<T>& vt)
		{
			ZoneScoped;
			static_assert(std::is_trivially_copyable_v<T>, "RPCGenericType, type must be trivially copyable.");
			static_assert(!std::is_pointer_v<T>, "RPCGenericType, type must not be a pointer.");
			_data.resize(sizeof(T) * vt.size());
			std::memcpy(_data.data(), vt.data(), sizeof(T) * vt.size());
			return *this;
		}

		template<typename T>
		T asType() const
		{
			ZoneScoped;
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
			ZoneScoped;
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
			ZoneScoped;
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
			ZoneScoped;
			return _data.empty() ? nullptr : _data.data();
		}

		std::uint64_t getSize() const
		{
			ZoneScoped;
			return _data.size();
		}

		std::vector<std::uint8_t> _data;
	};

	constexpr std::uint64_t RPC_MESSAGE_HEADER_SIZE = sizeof(RPCMessageHeader);

	std::uint64_t GetNextMessageId();
	std::uint64_t GetCurrentCallerId();

	class RPCClientThread
	{
	public:
		RPCClientThread(const std::string& hostname, std::uint16_t rpcport = RPC_PORT, std::uint64_t customid = 0x0);
		~RPCClientThread();

		template<typename T, typename... Args>
		inline T call(std::uint16_t procedureid, Args&&... args);

		template<typename... Args>
		inline void call(std::uint16_t procedureid, Args&&... args);

		inline void callRaw(std::uint16_t procedureid, const std::vector<std::uint8_t>& data, std::vector<std::uint8_t>* retdata);

	private:
		void clientThread(const Socket& socket);

	private:
		Socket _rpc_socket;
		std::unique_ptr<std::thread> _rpc_thread;
		SysByteStream* _rpc_stream;
		std::atomic<bool> _rpc_thread_running = false;
		std::atomic<bool> _rpc_thread_ready = false;
		SysBufferStack _call_return_stack;
		bool _rpc_has_custom_id = false;
		std::uint64_t _rpc_custom_id;
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
		ZoneScoped;
		const mulex::Socket& conn = _rpc_socket;
		mulex::RPCMessageHeader header;
		if(_rpc_has_custom_id)
		{
			header.client = _rpc_custom_id;
		}
		else
		{
			header.client = SysGetClientId();
		}
		header.procedureid = procedureid;
		header.msgid = GetNextMessageId();
		if constexpr(sizeof...(args) > 0)
		{
			header.payloadsize = static_cast<std::uint32_t>(SysVargSize<Args...>(args...));
		}
		else
		{
			header.payloadsize = 0;
		}

		if constexpr(sizeof...(args) > 0)
		{
			mulex::SocketResult result;
			std::vector<std::uint8_t> buffer(sizeof(header) + header.payloadsize);
			std::vector<std::uint8_t> params = SysPackArguments(args...);
			std::memcpy(buffer.data(), &header, sizeof(header));
			std::memcpy(buffer.data() + sizeof(header), params.data(), header.payloadsize);
			result = mulex::SocketSendBytes(conn, buffer.data(), sizeof(header) + header.payloadsize);
			if(result == mulex::SocketResult::ERROR)
			{
				mulex::LogError("CallRemoteFunction failed to send data.");
				return T();
			}
		}
		else
		{
			mulex::SocketResult result;
			result = mulex::SocketSendBytes(conn, (std::uint8_t*)&header, sizeof(header));
			if(result == mulex::SocketResult::ERROR)
			{
				mulex::LogError("CallRemoteFunction failed to send data.");
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
		ZoneScoped;
		const mulex::Socket& conn = _rpc_socket;
		mulex::RPCMessageHeader header;
		if(_rpc_has_custom_id)
		{
			header.client = _rpc_custom_id;
		}
		else
		{
			header.client = SysGetClientId();
		}
		header.procedureid = procedureid;
		header.msgid = GetNextMessageId();
		header.payloadsize = static_cast<std::uint32_t>(SysVargSize<Args...>(args...));


		if constexpr(sizeof...(args) > 0)
		{
			mulex::SocketResult result;
			std::vector<std::uint8_t> buffer(sizeof(header) + header.payloadsize);
			std::vector<std::uint8_t> params = SysPackArguments(args...);
			std::memcpy(buffer.data(), &header, sizeof(header));
			std::memcpy(buffer.data() + sizeof(header), params.data(), header.payloadsize);
			result = mulex::SocketSendBytes(conn, buffer.data(), sizeof(header) + header.payloadsize);
			if(result == mulex::SocketResult::ERROR)
			{
				mulex::LogError("CallRemoteFunction failed to send data.");
				return;
			}
		}
		else
		{
			mulex::SocketResult result;
			result = mulex::SocketSendBytes(conn, (std::uint8_t*)&header, sizeof(header));
			if(result == mulex::SocketResult::ERROR)
			{
				mulex::LogError("CallRemoteFunction failed to send data.");
				return;
			}
		}
	}

	inline void RPCClientThread::callRaw(std::uint16_t procedureid, const std::vector<std::uint8_t>& data, std::vector<std::uint8_t>* retdata)
	{
		ZoneScoped;
		const mulex::Socket& conn = _rpc_socket;
		mulex::RPCMessageHeader header;
		if(_rpc_has_custom_id)
		{
			header.client = _rpc_custom_id;
		}
		else
		{
			header.client = SysGetClientId();
		}
		header.procedureid = procedureid;
		header.msgid = GetNextMessageId();
		header.payloadsize = static_cast<std::uint32_t>(data.size());

		if(header.payloadsize > 0)
		{
			mulex::SocketResult result;
			std::vector<std::uint8_t> buffer(sizeof(header) + header.payloadsize);
			std::memcpy(buffer.data(), &header, sizeof(header));
			std::memcpy(buffer.data() + sizeof(header), data.data(), header.payloadsize);
			result = mulex::SocketSendBytes(conn, buffer.data(), sizeof(header) + header.payloadsize);
			if(result == mulex::SocketResult::ERROR)
			{
				mulex::LogError("CallRemoteFunction failed to send data.");
				return;
			}
		}
		else
		{
			mulex::SocketResult result;
			result = mulex::SocketSendBytes(conn, (std::uint8_t*)&header, sizeof(header));
			if(result == mulex::SocketResult::ERROR)
			{
				mulex::LogError("CallRemoteFunction failed to send data.");
				return;
			}
		}

		if(retdata)
		{
			*retdata = _call_return_stack.pop();
		}
	}

	MX_RPC_METHOD mulex::RPCGenericType RpcGetAllCalls();

} // namespace mulex
