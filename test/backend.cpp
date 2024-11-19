#include "test.h"
#include "../mxbackend.h"
#include "../mxlogger.h"
#include "../mxsystem.h"
#include <rpcspec.inl>

using namespace mulex;

class TestBackend : public MxBackend
{
public:
	TestBackend(int argc, char* argv[]) : MxBackend(argc, argv)
	{
		// Decide wether the user can set their arguments or just prefer using config files / rdb / pdb entries
		std::string ekey = "/system/backends/" + std::string(SysGetBinaryName()) + "/config/period_ms";
		RdbAccess config = getConfigRdbRoot();

		// _period_ms = config["period_ms"];
		// config["period_ms"] = 3;
		_period_ms = 1000;

		registerEvent("TestBackend::data");
		LogTrace("Backend name: %s", std::string(SysGetBinaryName()).c_str());

		subscribeEvent("TestBackend::data", [](auto* data, auto len, auto* udata){
			char datac[5];
			datac[4] = 0;
			memcpy(datac, data, len);
			LogTrace("Got event with data: %s", datac);
		});

		RdbAccess roota;
		roota["/system/*/test"].watch([](const RdbKeyName& key, const RPCGenericType& value){
			LogTrace("%s changed.", key.c_str());
			LogTrace("value = %d.", value.asType<std::int32_t>());
		});


		std::this_thread::sleep_for(std::chrono::milliseconds(1000));

		roota["/system/nested/test"].erase();
		roota["/system/nested/test"].create(RdbValueType::INT32, 3);

		roota["/system/nested/failtest"].erase();
		roota["/system/nested/failtest"].create(RdbValueType::INT32, 4);
	}

	virtual void periodic() override
	{
		std::cout << "Loop" << std::endl;
		// RdbAccess config = getConfigRdbRoot();
		// static std::int32_t s = 0;
		// config["period_ms"] = s++;
		// dispatchEvent("TestBackend::data", reinterpret_cast<const std::uint8_t*>("DATA"), 4);
	}
};

int main(int argc, char* argv[])
{
	TestBackend backend(argc, argv);
	backend.startEventLoop();
	return 0;
}