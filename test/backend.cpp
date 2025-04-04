#include "test.h"
#include "../mxbackend.h"
#include "../mxlogger.h"
#include "../mxsystem.h"
#include <chrono>
#include <rpcspec.inl>
#include <thread>

using namespace mulex;

class TestBackend : public MxBackend
{
public:
	TestBackend(int argc, char* argv[]) : MxBackend(argc, argv)
	{
		// Decide wether the user can set their arguments or just prefer using config files / rdb / pdb entries
		// std::string ekey = "/user/" + std::string(SysGetBinaryName()) + "/config/";
		// RdbAccess config(ekey);
		// RdbAccess config = getConfigRdbRoot();

		// std::int32_t value = config["period_ms"];
		// std::cout << value << std::endl;
		// log.error("This is a very big error string from the constructor of TestBackend!");
		// log.warn("This is a very big warning string from the constructor of TestBackend!");
		// log.info("This is a very big info string from the constructor of TestBackend!");
		// config["period_ms"].erase();
		// config["period_ms"].create(RdbValueType::INT32, std::int32_t(89));
		// config["period_ms"].history(true);
		// _period_ms = config["period_ms"];
		// config["period_ms"] = 3;

		// pdb.createTable(
		// 	"table0",
		// 	{
		// 		"id INTEGER PRIMARY KEY AUTOINCREMENT",
		// 		"value TEXT NOT NULL"
		// 	}
		// );
		//
		// auto writer = pdb.getWriter<int, PdbString>("table0", {"id", "value"});
		//
		// writer(std::nullopt, "some value");
		// writer(std::nullopt, "other value");
		// writer(3, "modified value");
		//
		// auto reader = pdb.getReader<int, PdbString>("table0", {"id", "value"});
		// auto values = reader("WHERE id = 3");
		// auto [id, value] = values[0];
		// 
		// log.info("id0, value0: %d, %s", id, value.c_str());
		//
		// registerEvent("TestBackend::data");
		// LogTrace("Backend name: %s", std::string(SysGetBinaryName()).c_str());
		//
		// RdbAccess roota;
		// roota["/system/*/test"].watch([](const RdbKeyName& key, const RPCGenericType& value){
		// 	LogTrace("%s changed.", key.c_str());
		// 	LogTrace("value = %d.", value.asType<std::int32_t>());
		// });
		//
		// roota["/system/metrics/cpu_usage"].watch([this](const RdbKeyName& key, const RPCGenericType& value) {
		// 	double cpu = value;
		// 	log.info("CPU: %.2lf%%", cpu);
		// });
		//
		// roota["/system/metrics/mem_used"].watch([this](const RdbKeyName& key, const RPCGenericType& value) {
		// 	std::uint64_t mem = value;
		// 	log.info("MEM: %llu", mem);
		// });
		//
		// roota["/system/nested/test"].erase();
		// roota["/system/nested/test"].create(RdbValueType::INT32, 3);
		//
		// roota["/system/nested/failtest"].erase();
		// roota["/system/nested/failtest"].create(RdbValueType::INT32, 4);

		// deferExec(&TestBackend::periodic, 0, 1000);
		registerUserRpc(&TestBackend::userRpc);

		deferExec([this](){ setStatus("MyStatus", "#ff0000"); log.info("Done."); }, 3000);

		registerEvent("TestBackend::dummy");
		deferExec([this](){
			// Random 1024 bytes
			static std::vector<std::uint8_t> data(1024);
			dispatchEvent("TestBackend::dummy", data);
			log.info("Test message triggering on events page...");
		}, 0, 1000);

		// subscribeEvent("TestBackend::dummy", [this](auto* d, auto sz, auto* udata){ log.info("Loopback..."); });

		// registerRunStartStop(&TestBackend::onRunStart, &TestBackend::onRunStop);
	}

	void onRunStart(std::uint64_t runno)
	{
		// subscribeEvent("TestBackend::data", [](auto* data, auto len, auto* udata){
		// 	LogTrace("Got event with len: %llu", len);
		// });
	}

	void onRunStop(std::uint64_t runno)
	{
		// unsubscribeEvent("TestBackend::data");
	}

	RPCGenericType userRpc(const std::vector<std::uint8_t>& data)
	{
		// This function gets called when someone calls
		// BckCallUserRpc("test_bck", some_data);
		log.info("User RPC called!");
		log.info("Data size: %llu", data.size());
		// mulex::LogDebug("Bytes:");
		// for(int i = 0; i < data.size(); i++)
		// {
		// 	mulex::LogDebug("0x%02x", data[i]);
		// }

		// As string:
		// const char* str = reinterpret_cast<const char*>(data.data());
		// mulex::LogDebug("As string: %s", str);

		// As uint8 + float
		std::uint8_t forty_two = *data.data();
		// float pi = *reinterpret_cast<const float*>(data.data() + sizeof(std::uint8_t));
		mulex::LogDebug("forty_two: %d", forty_two);
		return 3.1415f;
	}

	void periodic()
	{
		// std::string ekey = "/user/" + std::string(SysGetBinaryName()) + "/config/";
		// RdbAccess config(ekey);
		// std::int32_t s = config["period_ms"];
		// config["period_ms"] = ++s;
		// static std::vector<std::uint8_t> buffer(100);
		// buffer.clear();
		//
		// log.info("Hello from deferred period function.");
		// 
		// dispatchEvent("TestBackend::data", MxEventBuilder(buffer)
		// 	.add(SysGetCurrentTime())
		// 	.add(3.14159265358979)
		// );
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
