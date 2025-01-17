#include "../mxbackend.h"
#include "../mxsystem.h"
#include "../mxlogger.h"
#include "../mxevt.h"
#include "../mxrun.h"
#include <thread>

static const std::atomic<bool>* stop;

namespace mulex
{
	mulex::BckUserRpcStatus BckCallUserRpc(mulex::string32 bck, mulex::RPCGenericType data)
	{
		std::string evt = bck.c_str();
		evt += "::rpc";

		LogTrace("[mxbackend] User RPC called for <%s>.", bck.c_str());

		if(EvtGetId(evt.c_str()) == 0)
		{
			return BckUserRpcStatus::NO_SUCH_BACKEND;
		}

		if(!EvtEmit(evt, data.getData(), data.getSize()))
		{
			return BckUserRpcStatus::EMIT_FAILED;
		}

		return BckUserRpcStatus::OK;
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
		registerEvent(rpc_event);
		subscribeEvent(rpc_event, [this](auto* data, auto len, auto*){
			this->userRpcInternal(data, len, nullptr);
		});
	}

	void MxBackend::userRpcInternal(const std::uint8_t* data, std::uint64_t len, const std::uint8_t* udata)
	{
		if(_user_rpc)
		{
			std::vector<std::uint8_t> vdata(data, data + len);
			_io.schedule([this, vdata](){ (*this.*_user_rpc)(vdata); });
		}
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
		while(!*stop)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			std::this_thread::yield();
		}
	}
} // namespace mulex
