#include "network/rpc.h"
#include "mxevt.h"
#include "mxrdb.h"
#include "mxhttp.h"

#include <signal.h>
static volatile sig_atomic_t stop = 0;

int main(int argc, char* argv[])
{
	if(!mulex::SysInitializeExperiment(argc, argv))
	{
		::exit(0);
	}

	mulex::RdbInit(1024 * 1024);
	mulex::PdbInit();
	mulex::RPCServerThread rpcThread;
	mulex::EvtServerThread evtThread;
	mulex::SysRegisterSigintAction([](int s){
		stop = s;
	});

	while(!rpcThread.ready())
	{
		std::this_thread::yield();
	}

	mulex::HttpStartServer(8080);

	mulex::LogMessage("[mxserver] Running...");
	mulex::LogMessage("[mxserver] Press ctrl-C to exit.");

	while(!stop)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	mulex::LogMessage("[mxserver] ctrl-C detected. Exiting...");

	mulex::HttpStopServer();

	mulex::PdbClose();
	mulex::RdbClose();

	return 0;
}
