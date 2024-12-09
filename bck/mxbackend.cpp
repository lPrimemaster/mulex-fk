#include "../mxbackend.h"
#include "../mxsystem.h"
#include "../mxlogger.h"
#include "../mxevt.h"

#include <signal.h>
static volatile sig_atomic_t stop = 0;

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
		
		// So we stop on ctrl-C
		LogMessage("[mxbackend] Running...");
		LogMessage("[mxbackend] Press ctrl-C to exit.");
		mulex::SysRegisterSigintAction([](int s){
			stop = s;
		});

		_init_ok = true;
		_period_ms = 0;
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

	void MxBackend::subscribeEvent(const std::string& evt, EvtClientThread::EvtCallbackFunc func)
	{
		if(_init_ok && _experiment)
		{
			_experiment->_evt_client->subscribe(evt, func);
		}
	}

	void MxBackend::startEventLoop()
	{
		if(!_init_ok)
		{
			return;
		}

		while(!stop)
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
