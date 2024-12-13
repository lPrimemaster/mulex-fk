#include "../mxbackend.h"
#include "../mxlogger.h"
#include "../mxsystem.h"
#include <chrono>
#include <rpcspec.inl>
#include <thread>

using namespace mulex;

class ConsumerBackend : public MxBackend
{
public:
	ConsumerBackend(int argc, char* argv[]) : MxBackend(argc, argv)
	{
	}

	virtual void onRunStart(std::uint64_t runno) override
	{
		subscribeEvent("ProducerBackend::data0", [](auto* data, auto len, auto* user_data) {
			LogTrace("Got data0 with len: %llu", len);
		});

		subscribeEvent("ProducerBackend::data1", [](auto* data, auto len, auto* user_data) {
			LogTrace("Got data1 with len: %llu", len);
		});
	}

	virtual void onRunStop(std::uint64_t runno) override
	{
		unsubscribeEvent("ProducerBackend::data0");
		unsubscribeEvent("ProducerBackend::data1");
	}

	virtual void periodic() override
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
};

int main(int argc, char* argv[])
{
	ConsumerBackend backend(argc, argv);
	backend.startEventLoop();
	return 0;
}
