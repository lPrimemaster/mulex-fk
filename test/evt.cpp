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

	SysConnectToExperiment("localhost");

	const Experiment* exp = SysGetConnectedExperiment().value();
	EvtClientThread& ect = *exp->_evt_client.get();

	ect.regist("test_evt");

	ect.subscribe("test_evt", [](auto* data, auto len, auto* userdata){
		ASSERT_THROW(std::string(reinterpret_cast<const char*>(data)) == "Hello From Ect");
		ASSERT_THROW(len == 15);
		ASSERT_THROW(userdata == nullptr);
		LogDebug("test_evt callback -> ptr: %s, len: %d, usrptr: %p", data, len, userdata);
	});
	
	ect.emit("test_evt", reinterpret_cast<const std::uint8_t*>("Hello From Ect"), 15);
	ect.emit("test_evt", reinterpret_cast<const std::uint8_t*>("Hello From Ect"), 15);
	ect.emit("test_evt", reinterpret_cast<const std::uint8_t*>("Hello From Ect"), 15);

	SysDisconnectFromExperiment();

	RdbClose();

	return 0;
}
