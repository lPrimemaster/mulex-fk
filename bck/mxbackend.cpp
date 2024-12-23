#include "../mxbackend.h"
#include "../mxsystem.h"
#include "../mxlogger.h"
#include "../mxevt.h"
#include "../mxrun.h"

static const std::atomic<bool>* stop;

namespace mulex
{
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

		// Register start/stop listeners
		rdb["/system/run/status"].watch([this](const RdbKeyName&, const RPCGenericType& value) {
			RunStatus status = static_cast<mulex::RunStatus>(value.asType<std::uint8_t>());

			if(status == RunStatus::RUNNING)
			{
				std::uint64_t no = rdb["/system/run/number"];
				LogTrace("[mxbacked] Run %llu starting.", no);
				this->onRunStart(no);
			}
			else if(status == RunStatus::STOPPED)
			{
				std::uint64_t no = rdb["/system/run/number"];
				LogTrace("[mxbacked] Run %llu stopping.", no);
				this->onRunStop(no);
			}

			// Silently ignore other transition states
		});
		
		// So we stop on ctrl-C
		LogMessage("[mxbackend] Running...");
		LogMessage("[mxbackend] Press ctrl-C to exit.");
		stop = mulex::SysSetupExitSignal();

		_init_ok = true;
		_period_ms = 0;

		// Check the current status and schedule start
		std::uint8_t status = rdb["/system/run/status"];
		if(static_cast<RunStatus>(status) == RunStatus::RUNNING)
		{
			std::uint64_t no = rdb["/system/run/number"];
			LogTrace("[mxbacked] Run %llu starting.", no);
			this->onRunStart(no);
		}
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

	void MxBackend::onRunStart(std::uint64_t runno)
	{
	}

	void MxBackend::onRunStop(std::uint64_t runno)
	{
	}

	void MxBackend::deferExec(std::function<void()> wrapfunc)
	{
		// TODO: (Cesar)
	}

	void MxBackend::startEventLoop()
	{
		if(!_init_ok)
		{
			return;
		}

		while(!*stop)
		{
			std::int64_t looptime = SysGetCurrentTime();
			periodic();
			std::this_thread::sleep_for(std::chrono::milliseconds(_period_ms - (SysGetCurrentTime() - looptime)));
		}
	}

	void MxBackend::eventLoop()
	{
	}
} // namespace mulex
