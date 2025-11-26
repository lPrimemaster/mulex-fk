#include "test.h"
#include "../mxbackend.h"
#include "../mxlogger.h"
#include "../mxsystem.h"
#include <cstdint>
#include <thread>

using namespace mulex;

class TestBackend : public MxBackend
{
public:
	TestBackend(int argc, char* argv[]) : MxBackend(argc, argv)
	{
		registerUserRpc(&TestBackend::userRpc);

		deferExec([this](){ setStatus("MyStatus", "#ff0000"); log.info("Done."); }, 5000);

		registerEvent("TestBackend::dummy");
		deferExec([this](){
			static MxEventBuilder data(1024);

			static std::int16_t inc = 0;
			static std::int16_t coffee = 0xCAFE;

			data.reset().add(coffee).add(coffee).add(coffee).add(coffee + inc++);

			dispatchEvent("TestBackend::dummy", data);
			log.info("Test message triggering on events page...");
		}, 0, 500);

		registerRunStartStop(&TestBackend::onRunStart, &TestBackend::onRunStop);

		auto [status, ret] = callUserRpc<float>(
			"backend.py",
			CallTimeout(1000),
			std::int32_t(2),
			77.7f
		);
		std::cout << static_cast<int>(status) << " -> " << ret << std::endl;
		// registerDependency("pmc8742.exe").required(true).onFail(MxRexDependencyManager::LOG_ERROR);
		// registerDependency("backend.py").required(true).onFail(MxRexDependencyManager::TERMINATE);
	}

	void onRunStart(std::uint64_t runno)
	{
		std::string data = "Bing Xiling";
		logRunWriteFile("somefile_bx.txt.random", reinterpret_cast<const std::uint8_t*>(data.data()), data.size());

		std::this_thread::sleep_for(std::chrono::seconds(2));

		data = "Bing Xiling 2";
		logRunWriteFile("somefile_bx.txt.random", reinterpret_cast<const std::uint8_t*>(data.data()), data.size());
	}

	void onRunStop(std::uint64_t runno)
	{
		std::string data = "Bing Xiling";
		logRunWriteFile("somefile_bx_stop.txt", reinterpret_cast<const std::uint8_t*>(data.data()), data.size());
	}

	RPCGenericType userRpc(const std::int32_t& data, const float& ft)
	{
		log.info("User RPC called!");
		log.info("P0: %d", data);
		log.info("P1: %f", ft);

		// std::uint8_t forty_two = *data.data();
		// mulex::LogDebug("forty_two: %d", forty_two);
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
