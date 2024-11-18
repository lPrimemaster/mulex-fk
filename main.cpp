#include "mxsystem.h"
#include "mxlogger.h"
#include <signal.h>
static volatile sig_atomic_t stop = 0;

int main(int argc, char* argv[])
{
	if(!mulex::SysInitializeExperiment(argc, argv))
	{
		::exit(0);
	}

	mulex::SysRegisterSigintAction([](int s){
		stop = s;
	});

	mulex::LogMessage("[mxserver] Running...");
	mulex::LogMessage("[mxserver] Press ctrl-C to exit.");

	while(!stop)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	mulex::LogMessage("[mxserver] ctrl-C detected. Exiting...");

	mulex::SysCloseExperiment();

	return 0;
}
