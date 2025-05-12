#include "test.h"
#include "../mxbackend.h"
#include "../mxlogger.h"
#include "../mxsystem.h"

using namespace mulex;

class TestBackend : public MxBackend
{
public:
	TestBackend(int argc, char* argv[]) : MxBackend(argc, argv)
	{
		registerUserRpc(&TestBackend::userRpc);

		deferExec([this](){ setStatus("MyStatus", "#ff0000"); log.info("Done."); }, 3000);

		registerEvent("TestBackend::dummy");
		deferExec([this](){
			static MxEventBuilder data(1024);

			static std::int16_t inc = 0;
			static std::int16_t coffee = 0xCAFE;

			data.reset().add(coffee).add(coffee).add(coffee).add(coffee + inc++);

			dispatchEvent("TestBackend::dummy", data);
			log.info("Test message triggering on events page...");
		}, 0, 500);

		// registerDependency("pmc8742.exe").required(true).onFail(RexDependencyManager::LOG_ERROR);
	}

	void onRunStart(std::uint64_t runno)
	{
	}

	void onRunStop(std::uint64_t runno)
	{
	}

	RPCGenericType userRpc(const std::vector<std::uint8_t>& data)
	{
		log.info("User RPC called!");
		log.info("Data size: %llu", data.size());

		std::uint8_t forty_two = *data.data();
		mulex::LogDebug("forty_two: %d", forty_two);
		return 3.1415f;
	}
};

int main(int argc, char* argv[])
{
	SysAddArgument("custom-arg", 0, false, [](const std::string&) {}, "My custom arg.");

	TestBackend backend(argc, argv);
	backend.init();
	backend.spin();
	return 0;
}
