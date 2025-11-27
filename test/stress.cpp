#include "../mxbackend.h"
#include "../mxlogger.h"
#include "../mxsystem.h"
#include <cstdint>
#include <thread>

using namespace mulex;

class StressBackend : public MxBackend
{
public:
	StressBackend(int argc, char* argv[], int& nj, int& tr, std::uint64_t& sz, const std::string& name) : MxBackend(argc, argv)
	{
		registerUserRpc(&StressBackend::userRpc);

		log.info("Setting up %d jobs with %d trigger rate.", nj, tr);
		static std::vector<std::uint8_t> buffer(sz);
		for(int i = 0; i < nj; i++)
		{
			std::string jname = name + "::sj-" + std::to_string(i);
			registerEvent(jname);
			deferExec([this, jname, sz](){
				// static std::int16_t coffee = 0xCAFE;

				// data.reset().add(coffee).add(coffee).add(coffee).add(coffee);

				dispatchEvent(jname, buffer);
			}, 0, 1000.0 / tr);
		}

		// Every 10s call user rpc of other job incrementing
		deferExec([this, nj, &name](){
			static int job = -1;
			if (++job >= nj) return;

			auto status = callUserRpc(
				"Job"+std::to_string(job),
				CallTimeout(1000),
				std::int32_t(std::stoi(name.substr(3)))
			);

		}, 1'000, 5'000);

		// Run for 1 min and terminate
		deferExec([this](){ terminate(); }, 60'000);
	}

	RPCGenericType userRpc(const std::int32_t& job)
	{
		log.info("User RPC called from job %d!", job);
		return {};
	}
};

int main(int argc, char* argv[])
{
	int event_nj = 1;
	int event_tr = 1; // Hz
	std::uint64_t event_sz = 1024;
	std::string name;
	
	SysAddArgument("n-jobs", 0, true, [&event_nj](const std::string& arg) { event_nj = std::stoi(arg); }, "Number of stress events on this backend.");
	SysAddArgument("j-rate", 0, true, [&event_tr](const std::string& arg) { event_tr = std::stoi(arg); }, "Global event trigger rate.");
	SysAddArgument("j-size", 0, true, [&event_sz](const std::string& arg) { event_sz = std::stoull(arg) * 1024; }, "Job buffer size (kB).");
	SysAddArgument("j-name", 0, true, [&name](const std::string& arg) { name = arg; SysOverrideBinaryName(arg); }, "Backend name override.");

	StressBackend backend(argc, argv, event_nj, event_tr, event_sz, name);
	backend.init();
	backend.spin();
	return 0;
}
