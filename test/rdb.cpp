#include "../mxrdb.h"
#include "test.h"

int main(void)
{
	using namespace mulex;
	RdbInit(1024 * 100);

	RPCServerThread rst;

	float f = 7.0f;
	float f2 = 8.0f;
	std::vector<float> f3(10);
	std::fill(f3.begin(), f3.end(), 9.0f);
	float f4 = 10.0f;
	{
		timed_block tb("Key creation");
		RdbNewEntry("/system/test2", RdbValueType::FLOAT32, &f2);
		RdbNewEntry("/system/test4", RdbValueType::FLOAT32, &f4);
		RdbNewEntry("/system/test", RdbValueType::FLOAT32, &f);
		RdbNewEntry("/system/test3", RdbValueType::FLOAT32, f3.data(), f3.size());

		// Add loads of keys
		// for(int i = 5; i < 105; i++)
		// {
		// 	char key[32];
		// 	snprintf(key, 32, "%s%d", "/system/test", i);
		// 	RdbNewEntry(key, RdbValueType::FLOAT32, f3.data(), f3.size());
		// }
	}

	RdbEntry* entry;
	RdbEntry* entry2;
	RdbEntry* entry3;

	entry = RdbFindEntryByName("/system/test");
	entry2 = RdbFindEntryByName("/system/test2");
	float fr = entry->as<float>();
	float fr2 = entry2->as<float>();
	std::cout << fr << std::endl;
	std::cout << fr2 << std::endl;

	RdbDeleteEntry("/system/test2");
	
	entry3 = RdbFindEntryByName("/system/test3");

	std::cout << entry << " " << entry2 << " " << entry3 << std::endl;

	float* e3rdata = entry3->as<float*>();
	std::vector<float> e3data(e3rdata, e3rdata + entry3->_count);

	ASSERT_THROW(fr == 7.0f);
	ASSERT_THROW(fr2 == 8.0f);
	ASSERT_THROW(RdbFindEntryByName("/system/test2") == nullptr);
	for(const auto&d : e3data)
	{
		ASSERT_THROW(d == 9.0f);
	}

	RdbNewEntry("/system/ntest2", RdbValueType::FLOAT32, &f2);

	while(!rst.ready()) continue;

	SysConnectToExperiment("localhost");

	RdbAccess ra;

	timed_block tb("", false);
	tb.mstart();
	std::vector<float> fa = ra["/system/test3"];
	std::cout << "Key array remote read: " << tb.mstop() << " ms." << std::endl;

	for(auto& f : fa)
	{
		std::cout << f << std::endl;
	}

	tb.mstart();
	float fb = ra["/system/test2"];
	std::cout << "Key remote read: " << tb.mstop() << " ms." << std::endl;
	std::cout << fb << std::endl;

	ra["/system/test"] = 42.0f;

	float fc = ra["/system/test"];
	std::cout << fc << std::endl;

	ra["/system/ctest"].create(RdbValueType::INT32, 5);

	std::cout << static_cast<int>(ra["/system/ctest"]) << std::endl;
	ra["/system/ctest"].erase();
	std::cout << static_cast<int>(ra["/system/ctest"]) << std::endl;

	ra["/system/ctest2"].create(RdbValueType::INT32, 5);

	SysDisconnectFromExperiment();

	RdbDumpMetadata("rdbmeta.txt");

	RdbClose();
	return 0;
}
