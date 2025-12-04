#pragma once
#include <queue>
#include <type_traits>
#include <vector>
#include <cstdint>
#include <optional>
#include <condition_variable>
#include <thread>
#include <stack>
#include <queue>
#include <mutex>
#include <functional>
#include "mxtypes.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace mulex
{
	struct Socket;
	struct RPCGenericType;
	class RPCClientThread;
	class EvtClientThread;
}

namespace mulex
{
	template<typename T>
	inline void SysPackArguments(std::vector<std::uint8_t>& buffer, T& t)
	{
		static_assert(
			std::is_same_v<T, mulex::RPCGenericType> ||
			std::is_trivially_copyable_v<T>,
			"SysPackArguments requires trivially copyable arguments."
		);

		if constexpr(std::is_same_v<T, mulex::RPCGenericType>)
		{
			std::uint8_t sbuf[sizeof(std::uint64_t)];
			*reinterpret_cast<std::uint64_t*>(sbuf) = t._data.size();
			buffer.insert(buffer.end(), sbuf, sbuf + sizeof(std::uint64_t));
			buffer.insert(buffer.end(), t._data.begin(), t._data.end());
		}
		else
		{

			std::uint8_t ibuf[sizeof(T)];
			*reinterpret_cast<T*>(ibuf) = t; // NOTE: Copy constructor
			buffer.insert(buffer.end(), ibuf, ibuf + sizeof(T));
		}
	}

	template<typename T>
	inline void SysPackArguments(std::vector<std::uint8_t>& buffer, const T& t)
	{
		static_assert(
			std::is_same_v<T, mulex::RPCGenericType> ||
			std::is_trivially_copyable_v<T> ||
			std::is_same_v<T, std::vector<std::uint8_t>>,
			"SysPackArguments requires trivially copyable arguments or a binary buffer."
		);

		if constexpr(std::is_same_v<T, mulex::RPCGenericType>)
		{
			std::uint8_t sbuf[sizeof(std::uint64_t)];
			*reinterpret_cast<std::uint64_t*>(sbuf) = t._data.size();
			buffer.insert(buffer.end(), sbuf, sbuf + sizeof(std::uint64_t));
			buffer.insert(buffer.end(), t._data.begin(), t._data.end());
		}
		else if constexpr(std::is_same_v<T, std::vector<std::uint8_t>>)
		{
			buffer.insert(buffer.end(), t.begin(), t.end());
		}
		else
		{
			std::uint8_t ibuf[sizeof(T)];
			*reinterpret_cast<T*>(ibuf) = t; // NOTE: Copy constructor
			buffer.insert(buffer.end(), ibuf, ibuf + sizeof(T));
		}
	}

	template<typename ...Args>
	inline std::vector<std::uint8_t> SysPackArguments(Args&... args)
	{
		std::vector<std::uint8_t> buffer;
		(SysPackArguments(buffer, args), ...);
		return buffer;
	}

	template<typename ...Args>
	inline std::vector<std::uint8_t> SysPackArguments(const Args&... args)
	{
		std::vector<std::uint8_t> buffer;
		(SysPackArguments(buffer, args), ...);
		return buffer;
	}

	template<typename T, typename ...Args>
	inline std::tuple<T, Args...> SysUnpackArguments(const std::uint8_t* data, std::uint64_t size)
	{
		static_assert(
			std::is_same_v<T, mulex::RPCGenericType> ||
			(std::is_trivially_copyable_v<T> && std::is_default_constructible_v<T>) ||
			(std::is_same_v<T, std::vector<std::uint8_t>> && sizeof...(Args) == 0),
			"SysUnpackArguments requires trivially copyable and default constructible arguments or a single binary buffer."
		);

		if constexpr(std::is_same_v<T, mulex::RPCGenericType>)
		{
			const std::uint64_t rgt_size = *reinterpret_cast<const std::uint64_t*>(data);
			if constexpr(sizeof...(Args) > 0)
			{
				return std::tuple_cat(SysUnpackArguments<T>(data, size), SysUnpackArguments<Args...>(data + rgt_size, size - rgt_size));
			}
			else
			{
				return std::make_tuple(T::FromData(data + sizeof(std::uint64_t), rgt_size));
			}
		}
		else if constexpr(std::is_same_v<T, std::vector<std::uint8_t>>)
		{
			return std::make_tuple(std::vector<std::uint8_t>(data, data + size));
		}
		else
		{
			if constexpr(sizeof...(Args) > 0)
			{
				return std::tuple_cat(SysUnpackArguments<T>(data, size), SysUnpackArguments<Args...>(data + sizeof(T), size - sizeof(T)));
			}
			else
			{
				if(size < sizeof(T))
				{
					return std::make_tuple(T());
				}
				return std::make_tuple(*reinterpret_cast<const T*>(data));
			}
		}
	}

	template<typename T, typename ...Args>
	inline std::tuple<T, Args...> SysUnpackArguments(const std::vector<std::uint8_t>& buffer)
	{
		return SysUnpackArguments<T, Args...>(const_cast<const std::uint8_t*>(buffer.data()), buffer.size());
	}

	template<typename T>
	inline constexpr std::size_t SysVargSize(T& t)
	{
		if constexpr(std::is_same_v<T, mulex::RPCGenericType>)
		{
			return t.getSize() + sizeof(std::uint64_t);
		}
		else
		{
			return sizeof(T);
		}
	}

	template<typename T, typename U, typename ...Args>
	inline constexpr std::size_t SysVargSize(T& t, U& u, Args&... args)
	{
		return SysVargSize<T>(t) + SysVargSize<U, Args...>(u, args...);
	}

	class SysBufferStack
	{
	public:
		void push(std::vector<std::uint8_t>&& data);
		void push(const std::vector<std::uint8_t>& data);
		std::vector<std::uint8_t> pop();
		void requestUnblock();
		std::uint64_t size() const;

	private:
		std::stack<std::vector<std::uint8_t>> _stack;
		mutable std::mutex _mutex;
		std::atomic<bool> _sig_unblock = false;
		std::condition_variable _notifier;
	};

	class SysRefBufferStack
	{
	public:
		void push(std::vector<std::uint8_t>&& data, std::uint16_t ref);
		std::vector<std::uint8_t> pop();
		const std::vector<std::uint8_t>* peek();
		void decref();
		void requestUnblock();

	private:
		std::atomic<std::uint16_t> _refcount = 0;
		std::stack<std::vector<std::uint8_t>> _stack;
		std::mutex _mutex;
		std::condition_variable _notifier;
	};
	
	class SysByteStream
	{
	public:
		SysByteStream(std::uint64_t size, std::uint64_t headersize, std::uint64_t headeroffset);
		
		bool push(std::uint8_t* data, std::uint64_t size);
		std::uint64_t fetch(std::uint8_t* buffer, std::uint64_t size);
		void requestUnblock();
		bool unblockRequested() const;

	private:
		std::vector<std::uint8_t> _buffer;
		std::uint64_t 	  		  _buffer_offset;
		std::uint64_t			  _header_size;
		std::uint64_t			  _header_size_offset;
		std::atomic<bool> 		  _unblock_sig;
		std::mutex 				  _mutex;
		std::condition_variable   _notifier;
	};

	class SysFileWatcher
	{
	public:
		enum class FileOp
		{
			CREATED,
			MODIFIED,
			DELETED
		};
		SysFileWatcher(
			const std::string& dir,
			std::function<void(const FileOp op, const std::string& filename, const std::int64_t timestamp)> f,
			std::uint32_t interval = 1000,
			bool recursive = true
		);
		~SysFileWatcher();

	private:
		std::unique_ptr<std::thread> _thread;
		std::atomic<bool> _watcher_on;
	};

	struct SysAsyncTask
	{
		using Job = std::function<void()>;
		Job 		 _job;
		std::int64_t _scheduled_exec;
		std::int64_t _interval;

		inline bool operator>(const SysAsyncTask& other) const
		{
			return _scheduled_exec > other._scheduled_exec;
		}
	};

	class SysAsyncEventLoop
	{
	public:
		SysAsyncEventLoop();
		~SysAsyncEventLoop();
		void schedule(SysAsyncTask::Job job, std::int64_t delay = 0, std::int64_t interval = 0);

	private:
		void schedule(SysAsyncTask& task);

	private:
		std::priority_queue<SysAsyncTask, std::vector<SysAsyncTask>, std::greater<>> _queue;
		std::atomic<bool> _running;
		std::mutex _mutex;
		std::condition_variable _cv;
		std::thread _handle;
	};

	struct SysRecvThread
	{
		SysRecvThread(const Socket& socket, std::uint64_t ssize, std::uint64_t sheadersize, std::uint64_t sheaderoffset);
		SysByteStream _stream;
		std::thread   _handle;
	};

	static constexpr std::uint64_t SYS_RECV_THREAD_BUFFER_SIZE = 0x640000; // 10MB Maximum return size !

	[[nodiscard]] std::unique_ptr<SysRecvThread> SysStartRecvThread(const Socket& socket, std::uint64_t headersize, std::uint64_t headeroffset);
	bool SysRecvThreadCanStart([[maybe_unused]] const Socket& socket);

	struct Experiment
	{
		std::string _exp_name;
		std::unique_ptr<RPCClientThread> _rpc_client;
		std::unique_ptr<EvtClientThread> _evt_client;
	};

	static constexpr std::uint16_t EXP_DEFAULT_PORT = 5700;

	std::optional<const Experiment*> SysGetConnectedExperiment();
	bool SysConnectToExperiment(const char* hostname, std::uint16_t port = EXP_DEFAULT_PORT);
	void SysDisconnectFromExperiment();

#ifdef __linux__
	using SysSigintActionFunc = void(*)(int);
#else
	using SysSigintActionFunc = BOOL(*)(DWORD type);
#endif
	void SysRegisterSigintAction(SysSigintActionFunc f);
	const std::atomic<bool>* SysSetupExitSignal();
	void SysKillProcess(const std::string& message);

	bool SysDaemonize();
	bool SysInitializeExperiment(int argc, char* argv[]);
	void SysCloseExperiment();
	bool SysInitializeBackend(int argc, char* argv[]);
	void SysAddArgument(const std::string& longname, const char shortname, bool needvalue, std::function<void(const std::string&)> action, const std::string& helptxt = "");
	bool SysParseArguments(int argc, char* argv[]);
	std::int64_t SysGetCurrentTime();
	std::string_view SysGetCacheDir();
	std::string_view SysGetCacheLockDir();
	std::string_view SysGetCachePrivateDir();
	bool SysCreateNewExperiment(const std::string& expname);
	std::string SysGetExperimentHome();
	MX_RPC_METHOD mulex::string512 SysGetExperimentName();
	void SysOverrideBinaryName(const std::string& name);
	std::string_view SysGetBinaryName();
	std::string_view SysGetBinaryFullCommandLineArguments();
	std::string_view SysGetBinaryFullName();
	std::string_view SysGetHostname();
	std::uint64_t SysGetClientId();
	std::string SysI64ToHexString(std::uint64_t value);
	std::string SysI16ToHexString(std::uint16_t value);
	std::vector<std::uint8_t> SysReadBinFile(const std::string& file);
	void SysWriteBinFile(const std::string& file, const std::vector<std::uint8_t>& data);
	void SysAppendBinFile(const std::string& file, const std::vector<std::uint8_t>& data);
	void SysDeleteFile(const std::string& file);
	void SysCopyFile(const std::string& source, const std::string& destination);

	std::string SysSHA256Hex(const std::vector<std::uint8_t>& data);
	std::string SysBufferToHex(const std::uint8_t* buffer, std::uint64_t size);
	std::string SysGenerateSecureRandom256Hex();

	std::vector<std::string> SysStringSplitOnTokenSkipCommas(const std::string& input, char token);
	std::uint64_t SysStringHash64(const std::string& key);
	bool SysMatchPattern(const std::string& pattern, const std::string& target);

	bool SysSpawnProcess(const std::string& binary, const std::string& workdir, const std::vector<std::string>& argv);
#ifdef __linux__
	using SysProcHandle = pid_t;
#else
	using SysProcHandle = DWORD;
#endif
	bool SysInterruptProcess(SysProcHandle handle);
	std::string SysGetProcessBinaryName(SysProcHandle handle);
	bool SysLockCurrentProcess();
	bool SysUnlockCurrentProcess();

	struct SysPerformanceMetrics
	{
		double _cpu_usage;
		double _cpu_temp;
		std::uint64_t _ram_total;
		std::uint64_t _ram_used;
		std::uint64_t _disk_total;
		std::uint64_t _disk_free;
		std::uint64_t _disk_io_read;
		std::uint64_t _disk_io_write;
	};

	SysPerformanceMetrics SysGetPerformanceMetrics();
	void SysStartPerformanceMetricsThread();
	void SysStopPerformanceMetricsThread();

	void SysMarkUptimeNow();
	MX_RPC_METHOD std::int64_t SysGetUptimeMark();

	struct SysHandshakeHeader
	{
		string32 _mx_version;
		std::uint32_t _mx_rpc_version;
		// std::uint8_t _padding[28];
	};

	 SysHandshakeHeader SysGetHandshakeHeader();
} // namespace mulex
