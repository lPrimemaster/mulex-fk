#include "../mxsystem.h"
#include "../mxevt.h"
#include "../mxlogger.h"
#include "../mxrdb.h"
#include "test.h"

int main(void)
{
	using namespace mulex;

	RdbInit(1024 * 1024);
	RPCServerThread rst;
	EvtServerThread est;
	while(!est.ready() || !rst.ready())
	{
		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}

	// EvtClientThread ect("localhost");
	// RPCClientThread rct("localhost");

	SysConnectToExperiment("localhost");

	const Experiment* exp = SysGetConnectedExperiment().value();
	EvtClientThread& ect = *exp->_evt_client.get();

	ect.regist("test_evt");

	ect.subscribe("test_evt", [](auto* data, auto len, auto* userdata){});
	
	LogDebug("Emitting event");
	ect.emit("test_evt", reinterpret_cast<const std::uint8_t*>("Hello From Ect"), 15);

	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	SysDisconnectFromExperiment();

	RdbClose();

	return 0;
}
