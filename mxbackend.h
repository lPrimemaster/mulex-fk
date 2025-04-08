#pragma once
#include "mxevt.h"
#include "mxrdb.h"
#include "mxmsg.h"
#include "mxsystem.h"
#include "network/rpc.h"
#include <algorithm>
#include <string>
#include <functional>

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
		template<std::derived_from<MxBackend> D>
		void registerUserRpc(RPCGenericType (D::* func)(const std::vector<std::uint8_t>&))
		{
			_user_rpc = static_cast<RPCGenericType(MxBackend::*)(const std::vector<std::uint8_t>&)>(func);
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

	private:
		// User RPC
		void userRpcInternal(const std::uint8_t* data, std::uint64_t len, const std::uint8_t* udata);
		void registerUserRpcEvent();
		void emitRpcResponse(const RPCGenericType& data);

	public:
		void init();
		void spin();

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
		RPCGenericType (MxBackend::* _user_rpc)(const std::vector<std::uint8_t>&) = nullptr;

		// User start/stop
		void (MxBackend::* _user_run_start)(std::uint64_t) = nullptr;
		void (MxBackend::* _user_run_stop)(std::uint64_t) = nullptr;
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

} // namespace mulex
