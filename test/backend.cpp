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
		std::string ekey = "/user/" + std::string(SysGetBinaryName()) + "/config/";
		RdbAccess config(ekey);
		// RdbAccess config = getConfigRdbRoot();

		// std::int32_t value = config["period_ms"];
		// std::cout << value << std::endl;
		log.error("This is a very big error string from the constructor of TestBackend!");
		log.warn("This is a very big warning string from the constructor of TestBackend!");
		log.info("This is a very big info string from the constructor of TestBackend!");
		config["period_ms"].erase();
		config["period_ms"].create(RdbValueType::INT32, std::int32_t(89));
		config["period_ms"].history(true);
		// _period_ms = config["period_ms"];
		// config["period_ms"] = 3;

		PdbAccessRemote pdb;

		pdb.createTable(
			"table0",
			{
				"id INTEGER PRIMARY KEY AUTOINCREMENT",
				"value TEXT NOT NULL"
			}
		);

		auto writer = pdb.getWriter<int, PdbString>("table0", {"id", "value"});

		writer(std::nullopt, "some value");
		writer(std::nullopt, "other value");
		writer(3, "modified value");

		auto reader = pdb.getReader<int, PdbString>("table0", {"id", "value"});
		auto values = reader("WHERE id = 3");
		auto [id, value] = values[0];
		
		log.info("id0, value0: %d, %s", id, value.c_str());

		registerEvent("TestBackend::data");
		LogTrace("Backend name: %s", std::string(SysGetBinaryName()).c_str());

		RdbAccess roota;
		roota["/system/*/test"].watch([](const RdbKeyName& key, const RPCGenericType& value){
			LogTrace("%s changed.", key.c_str());
			LogTrace("value = %d.", value.asType<std::int32_t>());
		});

		roota["/system/nested/test"].erase();
		roota["/system/nested/test"].create(RdbValueType::INT32, 3);

		roota["/system/nested/failtest"].erase();
		roota["/system/nested/failtest"].create(RdbValueType::INT32, 4);
	}

	virtual void onRunStart(std::uint64_t runno) override
	{
		subscribeEvent("TestBackend::data", [](auto* data, auto len, auto* udata){
			LogTrace("Got event with len: %llu", len);
		});
	}

	virtual void onRunStop(std::uint64_t runno) override
	{
		unsubscribeEvent("TestBackend::data");
	}

	virtual void periodic() override
	{
		// RdbAccess config = getConfigRdbRoot();
		std::string ekey = "/user/" + std::string(SysGetBinaryName()) + "/config/";
		RdbAccess config(ekey);
		std::int32_t s = config["period_ms"];
		config["period_ms"] = ++s;
		static std::vector<std::uint8_t> buffer(100);
		buffer.clear();
		dispatchEvent("TestBackend::data", MxEventBuilder(buffer)
			.add(SysGetCurrentTime())
			.add(3.14159265358979)
		);
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}
};

int main(int argc, char* argv[])
{
	TestBackend backend(argc, argv);
	backend.startEventLoop();
	return 0;
}
