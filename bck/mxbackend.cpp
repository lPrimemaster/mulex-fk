#include "../mxbackend.h"
#include "../mxsystem.h"
#include "../mxlogger.h"
#include "../mxevt.h"
#include "../mxrun.h"
#include <condition_variable>
#include <thread>
#include <vector>
#include <rpcspec.inl>

static const std::atomic<bool>* stop;

namespace mulex
{
	mulex::RPCGenericType BckCallUserRpc(mulex::string32 bck, mulex::RPCGenericType data, std::int64_t timeout)
	{
		std::string evt = bck.c_str();
		std::string evt_emt = evt + "::rpc";
		std::string evt_res = evt + "::rpc_res";
		std::vector<std::uint8_t> response_full;

		LogTrace("[mxbackend] User RPC called for <%s>.", bck.c_str());

		if(EvtGetId(evt_emt.c_str()) == 0)
		{
			response_full.push_back(static_cast<std::uint8_t>(BckUserRpcStatus::NO_SUCH_BACKEND));
			return response_full;
		}

		if(!EvtEmit(evt_emt, data.getData(), data.getSize()))
		{
			response_full.push_back(static_cast<std::uint8_t>(BckUserRpcStatus::EMIT_FAILED));
			return response_full;
		}

		// Wait for response
		std::mutex mtx;
		std::condition_variable cv;
		bool proceed = false;
		std::vector<std::uint8_t> response;
		EvtServerRegisterCallback(evt_res, [&](auto, auto, auto, const std::uint8_t* data, std::uint64_t len) {
			LogTrace("[bck] Relaying RPC response.");
			if(len > 0)
			{
				std::unique_lock<std::mutex> lock(mtx);
				response.resize(len);
				std::memcpy(response.data(), data, len);
				proceed = true;
			}
			cv.notify_one();
		});

		// Wait timeout
		std::unique_lock<std::mutex> lock(mtx);
		if(timeout > 0)
		{
			if(!cv.wait_for(lock, std::chrono::milliseconds(timeout), [&](){ return proceed; }))
			{
				response_full.push_back(static_cast<std::uint8_t>(BckUserRpcStatus::RESPONSE_TIMEOUT));
				return response_full;
			}
		}
		else
		{
			static std::once_flag flag;
			std::call_once(flag, [](){ LogWarning("[mxbackend] Calling user RPC functions with zero timeout is discouraged."); });
			cv.wait(lock, [&](){ return proceed; });
		}

		if(response.empty())
		{
			response_full.push_back(static_cast<std::uint8_t>(BckUserRpcStatus::OK));
			return response_full;
		}

		response_full.push_back(static_cast<std::uint8_t>(BckUserRpcStatus::OK));
		response_full.insert(response_full.end(), response.begin(), response.end());
		return response_full;
	}

	MxBackend::MxBackend(int argc, char* argv[])
	{
		if(!SysInitializeBackend(argc, argv))
		{
			LogError("[mxbackend] Failed to initialize the backend.");
			return;
		}

		std::optional<const Experiment*> experiment = SysGetConnectedExperiment();

		if(!experiment.has_value())
		{
			LogError("[mxbackend] Failed to get connected experiment.");
			return;
		}

		_experiment = experiment.value();

		_init_ok = true;
	}

	MxBackend::~MxBackend()
	{
		if(_init_ok)
		{
			_experiment = nullptr;
			SysDisconnectFromExperiment();
			SysUnlockCurrentProcess();
		}
	}

	void MxBackend::registerEvent(const std::string& name)
	{
		if(_init_ok && _experiment)
		{
			_experiment->_evt_client->regist(name);
		}
	}

	void MxBackend::dispatchEvent(const std::string& evt, const std::uint8_t* data, std::uint64_t size)
	{
		// NOTE: (Cesar) We could ignore these checks unless the user uses this function out of place
		if(_init_ok && _experiment)
		{
			_experiment->_evt_client->emit(evt, data, size);
		}
	}

	void MxBackend::dispatchEvent(const std::string& evt, const std::vector<std::uint8_t>& data)
	{
		dispatchEvent(evt, data.data(), data.size());
	}

	void MxBackend::subscribeEvent(const std::string& evt, EvtClientThread::EvtCallbackFunc func)
	{
		if(_init_ok && _experiment)
		{
			_experiment->_evt_client->subscribe(evt, func);
		}
	}

	void MxBackend::unsubscribeEvent(const std::string& evt)
	{
		if(_init_ok && _experiment)
		{
			_experiment->_evt_client->unsubscribe(evt);
		}
	}

	void MxBackend::registerUserRpcEvent()
	{
		std::string rpc_event = std::string(SysGetBinaryName()) + "::rpc";
		std::string rpc_response = std::string(SysGetBinaryName()) + "::rpc_res";
		registerEvent(rpc_event);
		registerEvent(rpc_response);
		subscribeEvent(rpc_event, [this](auto* data, auto len, auto*){
			this->userRpcInternal(data, len, nullptr);
		});
	}

	void MxBackend::emitRpcResponse(const RPCGenericType& data)
	{
		static std::string rpc_response = std::string(SysGetBinaryName()) + "::rpc_res";
		dispatchEvent(rpc_response, data._data);
	}

	void MxBackend::userRpcInternal(const std::uint8_t* data, std::uint64_t len, const std::uint8_t* udata)
	{
		if(_user_rpc)
		{
			std::vector<std::uint8_t> vdata(data, data + len);
			_io.schedule([this, vdata]() {
				RPCGenericType response = (*this.*_user_rpc)(vdata);
				emitRpcResponse(response);
			});
		}
	}

	void MxBackend::setStatus(const std::string& status, const std::string& color)
	{
		// This only happens on non-ghost backends so one can call the SysGetClientId function
		static const std::string root_key = "/system/backends/" + SysI64ToHexString(SysGetClientId()) + "/";
		rdb[root_key + "user_status/text"] = mxstring<512>(status);
		rdb[root_key + "user_status/color"] = mxstring<512>(color);
	}

	RexDependencyManager MxBackend::registerDependency(const std::string& backend)
	{
		return RexDependencyManager(this, backend, 0x0, [this]() -> MsgEmitter& { return log; });
	}

	RexDependencyManager MxBackend::registerDependency(const std::uint64_t id)
	{
		return RexDependencyManager(this, "", id, [this]() -> MsgEmitter& { return log; });
	}

	std::tuple<BckUserRpcStatus, RPCGenericType> MxBackend::callUserRpc(const std::string& backend, const std::vector<uint8_t>& data, std::int64_t timeout)
	{
		RPCGenericType retval = _experiment->_rpc_client->call<RPCGenericType>(
			RPC_CALL_MULEX_BCKCALLUSERRPC,
			string32(backend),
			RPCGenericType(data),
			timeout
		);

		const std::uint8_t* ptr = retval.getData();
		BckUserRpcStatus status = static_cast<BckUserRpcStatus>(*ptr);
		if(status == BckUserRpcStatus::OK && retval.getSize() > sizeof(BckUserRpcStatus))
		{
			return std::make_tuple(
				status,
				RPCGenericType::FromData(retval.getData() + sizeof(BckUserRpcStatus), retval.getSize() - sizeof(BckUserRpcStatus))
			);
		}
		
		return std::make_tuple(status, RPCGenericType());
	}

	void MxBackend::terminate() const
	{
		LogDebug("[mxbackend] Received stop signal via terminate().");
		_prog_stop.store(true);
	}

	void MxBackend::deferExec(std::function<void()> func, std::int64_t delay, std::int64_t interval)
	{
		_io.schedule(func, delay, interval);
	}

	void MxBackend::init()
	{
		if(!_init_ok)
		{
			return;
		}

		// Register start/stop listeners
		// std::shared_ptr<MxBackend> self = std::shared_ptr<MxBackend>(this);
		rdb["/system/run/status"].watch([this](const RdbKeyName&, const RPCGenericType& value) {
			RunStatus status = static_cast<mulex::RunStatus>(value.asType<std::uint8_t>());

			if(status == RunStatus::RUNNING)
			{
				std::uint64_t no = rdb["/system/run/number"];
				LogTrace("[mxbacked] Run %llu starting.", no);
				if(_user_run_start)
				{
					_io.schedule([this, no](){ (*this.*_user_run_start)(no); });
				}
			}
			else if(status == RunStatus::STOPPED)
			{
				std::uint64_t no = rdb["/system/run/number"];
				LogTrace("[mxbacked] Run %llu stopping.", no);
				if(_user_run_stop)
				{
					_io.schedule([this, no](){ (*this.*_user_run_stop)(no); });
				}
			}

			// Silently ignore other transition states
		});
		
		// So we stop on ctrl-C
		LogMessage("[mxbackend] Running...");
		LogMessage("[mxbackend] Press ctrl-C to exit.");
		stop = mulex::SysSetupExitSignal();

		// Check the current status and schedule start
		std::uint8_t status = rdb["/system/run/status"];
		if(static_cast<RunStatus>(status) == RunStatus::RUNNING)
		{
			std::uint64_t no = rdb["/system/run/number"];
			LogTrace("[mxbacked] Run %llu starting.", no);
			if(_user_run_start)
			{
				_io.schedule([this, no](){ (*this.*_user_run_start)(no); });
			}
		}

		// Register user rpc call function
		registerUserRpcEvent();
	}

	void MxBackend::spin()
	{
		if(!_init_ok)
		{
			return;
		}

		while(!*stop && !_prog_stop)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			std::this_thread::yield();
		}
	}
} // namespace mulex
