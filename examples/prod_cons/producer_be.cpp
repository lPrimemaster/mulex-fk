#include "../mxbackend.h"
#include "../mxlogger.h"
#include "../mxsystem.h"
#include <chrono>
#include <rpcspec.inl>
#include <thread>

using namespace mulex;

class ProducerBackend : public MxBackend
{
public:
	ProducerBackend(int argc, char* argv[]) : MxBackend(argc, argv)
	{
		registerEvent("ProducerBackend::data0");
		registerEvent("ProducerBackend::data1");

		running = true;
		std::thread([this](){
			while(running)
			{
				static std::vector<std::uint8_t> buffer(100);
				buffer.clear();
				dispatchEvent("ProducerBackend::data0", MxEventBuilder(buffer)
					.add(SysGetCurrentTime())
				);
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			}
		}).detach();
	}

	~ProducerBackend()
	{
		running = false;
	}

	virtual void onRunStart(std::uint64_t runno) override
	{
	}

	virtual void onRunStop(std::uint64_t runno) override
	{
	}

	virtual void periodic() override
	{
		static std::vector<std::uint8_t> buffer(100);
		buffer.clear();
		dispatchEvent("ProducerBackend::data1", MxEventBuilder(buffer)
			.add(std::vector<std::uint8_t>(1024 * 1024 * 10, 1))
		);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

private:
	std::atomic<bool> running = false;
};

int main(int argc, char* argv[])
{
	ProducerBackend backend(argc, argv);
	backend.startEventLoop();
	return 0;
}
