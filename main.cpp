#include "rpc/rpc.h"
#include "mxrdb.h"

#include <signal.h>
static volatile sig_atomic_t stop = 0;

int main(int argc, char* argv[])
{
	mulex::RdbInit(1024 * 1024);
	mulex::RPCServerThread rpcThread;
	mulex::SysRegisterSigintAction([](int s){
		stop = s;
	});

	while(!rpcThread.ready())
	{
		std::this_thread::yield();
	}

	mulex::LogMessage("[mxserver] Running...");
	mulex::LogMessage("[mxserver] Press ctrl-C to exit.");

	while(!stop)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	mulex::LogMessage("[mxserver] ctrl-C detected. Exiting...");

	mulex::RdbClose();

	return 0;
}
