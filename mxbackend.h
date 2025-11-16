#pragma once

// NOTE: (César): Guard against this header usage on install by the user
//				  This is proper but keep in mind that it is not using RPC_CALL_KEYWORD nor RPC_PERM_KEYWORD
//				  So one should be carefull if that would change (unlikely)
#ifndef MX_RPC_METHOD
#define MX_RPC_METHOD
#define MX_PERMISSION(...)
#endif


#include "mxevt.h"
#include "mxrdb.h"
#include "mxmsg.h"
#include "mxsystem.h"
#include "network/rpc.h"
#include <algorithm>
#include <string>
#include <functional>
#include <tuple>

namespace mulex
{
	enum class BckUserRpcStatus : std::uint8_t
	{
		OK,
		EMIT_FAILED,
		RESPONSE_TIMEOUT,
		NO_SUCH_BACKEND
	};

	MX_RPC_METHOD mulex::RPCGenericType BckCallUserRpc(mulex::string32 evt, mulex::RPCGenericType data, std::int64_t timeout);

	class MxRexDependencyManager;

	class MxBackend
	{
	public:
		virtual ~MxBackend();
	protected:
		MxBackend(int argc, char* argv[]);

		// Events
		void dispatchEvent(const std::string& evt, const std::uint8_t* data, std::uint64_t size);
		void dispatchEvent(const std::string& evt, const std::vector<std::uint8_t>& data);
		void registerEvent(const std::string& evt);
		void subscribeEvent(const std::string& evt, EvtClientThread::EvtCallbackFunc func);
		void unsubscribeEvent(const std::string& evt);

		// User RPC
		template<std::derived_from<MxBackend> D, typename... Args>
		void registerUserRpc(RPCGenericType (D::* func)(const Args&...))
		{
			_user_rpc = [this, func](const std::vector<std::uint8_t>& data) -> RPCGenericType {
				return std::apply([&](auto&&... unpacked_args) -> RPCGenericType {
					return (*static_cast<D*>(this).*func)(std::forward<decltype(unpacked_args)>(unpacked_args)...);
				}, SysUnpackArguments<Args...>(data));
			};
		}

		// Start/stop
		template<std::derived_from<MxBackend> D>
		void registerRunStartStop(void (D::* start)(std::uint64_t), void (D::* stop)(std::uint64_t))
		{
			_user_run_start = static_cast<void(MxBackend::*)(std::uint64_t)>(start);
			_user_run_stop = static_cast<void(MxBackend::*)(std::uint64_t)>(stop);
		}

		// Set the backend custom status flag
		void setStatus(const std::string& status, const std::string& color);

		// Setup possible dependencies
		MxRexDependencyManager registerDependency(const std::string& backend);
		MxRexDependencyManager registerDependency(const std::uint64_t id);

		// Call user RPC
		std::tuple<BckUserRpcStatus, RPCGenericType> callUserRpc(const std::string& backend, const std::vector<uint8_t>& data, std::int64_t timeout);

		// Make timeout meaningfull on a multiple parameter call
		struct CallTimeout
		{
			explicit constexpr CallTimeout(std::int64_t timeout) : _timeout(timeout) { }
			explicit CallTimeout() = delete;
			CallTimeout(const CallTimeout&) = delete;
			CallTimeout(CallTimeout&&) = delete;
			CallTimeout& operator=(const CallTimeout&) = delete;
			CallTimeout& operator=(CallTimeout&&) = delete;
			std::int64_t _timeout;
		};

		template<typename T, typename... Args>
		inline std::tuple<BckUserRpcStatus, T> callUserRpc(const std::string& backend, const CallTimeout& timeout, Args... args)
		{
			auto [status, ret] = callUserRpc(backend, SysPackArguments(args...), timeout._timeout);
			if(status != BckUserRpcStatus::OK)
			{
				return std::make_tuple(status, T());
			}
			return std::make_tuple(status, static_cast<T>(ret));
		}

		template<typename... Args>
		inline BckUserRpcStatus callUserRpc(const std::string& backend, const CallTimeout& timeout, Args... args)
		{
			auto [status, _] = callUserRpc(backend, SysPackArguments(args...), timeout._timeout);
			return status;
		}

		// Run log
		void logRunWriteFile(const std::string& alias, const std::vector<std::uint8_t>& buffer);
		void logRunWriteFile(const std::string& alias, const std::uint8_t* buffer, std::uint64_t size);

		void bypassIntHandler(bool value);

	private:
		// User RPC
		void userRpcInternal(const std::uint8_t* data, std::uint64_t len, const std::uint8_t* udata);
		void registerUserRpcEvent();
		void emitRpcResponse(const RPCGenericType& data);

	public:
		void init();
		void spin();

		// Stop via code
		void terminate() const;

		bool checkStatus() const;

		// Execution defer
		template<std::derived_from<MxBackend> D>
		void deferExec(void (D::* func)(void), std::int64_t delay = 0, std::int64_t interval = 0)
		{
			_io.schedule(std::bind(static_cast<void(MxBackend::*)(void)>(func), this), delay, interval);
		}
		void deferExec(std::function<void()> func, std::int64_t delay = 0, std::int64_t interval = 0);

	protected:
		RdbAccess rdb;
		PdbAccessRemote pdb;
		MsgEmitter log;

	private:
		// Init metadata
		bool _init_ok = false;
		const Experiment* _experiment = nullptr;

		// Backend's async loop
		SysAsyncEventLoop _io;

		// User rpc function
		std::function<RPCGenericType(const std::vector<std::uint8_t>&)> _user_rpc;
		// RPCGenericType (MxBackend::*_user_rpc)(const std::vector<std::uint8_t>&) = nullptr;

		// User start/stop
		void (MxBackend::* _user_run_start)(std::uint64_t) = nullptr;
		void (MxBackend::* _user_run_stop)(std::uint64_t) = nullptr;

		// Programatically stop backend
		mutable std::atomic<bool> _prog_stop = false;

		// State Context
		std::atomic<bool> _inside_stop_ctx = false;

		// Interrupt signal bypass
		bool _bypass_int_sig = false;
	};

	template<typename T>
	concept TriviallyCopyable = std::is_trivially_copyable_v<T>;

	class MxEventBuilder
	{
	public:
		explicit MxEventBuilder(std::uint64_t size)
		{
			_buffer.reserve(size);
		}

		inline MxEventBuilder& reset()
		{
			_buffer.clear();
			return *this;
		}

		template<TriviallyCopyable T>
		inline MxEventBuilder& add(const T& t)
		{
			const std::uint8_t* ptr = reinterpret_cast<const std::uint8_t*>(&t);
			_buffer.insert(_buffer.end(), ptr, ptr + sizeof(T));
			return *this;
		}

		template<TriviallyCopyable T>
		inline MxEventBuilder& add(const std::vector<T>& t)
		{
			std::for_each(t.begin(), t.end(), [this](const auto& tt){ add(tt); });
			return *this;
		}

		operator const std::vector<std::uint8_t>&() { return _buffer; }
	private:
		std::vector<std::uint8_t> _buffer;
	};

	class MxRexDependencyManager
	{
	public:
		enum RDMFailFlags
		{
			LOG_WARN,
			LOG_ERROR,
			TERMINATE
		};

		MxRexDependencyManager(
			const MxBackend* bck,
			const std::string& dependency,
			std::uint64_t cid,
			std::function<MsgEmitter&()> log_hook
		);
		~MxRexDependencyManager();

		MxRexDependencyManager& required(bool r);
		MxRexDependencyManager& onFail(const RDMFailFlags& flag);

	private:
		bool checkDependencyExists();
		bool checkDependencyRunning();
		void calculateMissingVariables();


	private:
		std::string   				 _dep_name = "";
		std::uint64_t 				 _dep_cid = 0;
		bool		  		 		 _dep_req = false;
		bool						 _dep_con = false;
		RDMFailFlags  				 _dep_fail = LOG_WARN;
		std::function<MsgEmitter&()> _dep_hook;
		const MxBackend*			 _dep_ptr;
	};

} // namespace mulex
