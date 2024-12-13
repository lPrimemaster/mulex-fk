#include "../mxbackend.h"
#include "../mxlogger.h"
#include "../mxsystem.h"
#include "../mxdrv.h"
#include <chrono>
#include <rpcspec.inl>
#include <thread>

using namespace mulex;

class TemperatureReader : public MxBackend
{
public:
	TemperatureReader(int argc, char* argv[]) : MxBackend(argc, argv)
	{
		log.error("This is a very big error string from the constructor of TemperatureReader!");
		log.warn("This is a very big warning string from the constructor of TemperatureReader!");
		log.info("This is a very big info string from the constructor of TemperatureReader!");

		registerEvent("TemperatureReader::data");
		LogTrace("Backend name: %s", std::string(SysGetBinaryName()).c_str());

		evt_buffer.reserve(100);
	}

	~TemperatureReader()
	{
	}

	virtual void onRunStart(std::uint64_t runno) override
	{
		run = true;

		std::thread([this](){
			DrvTCP handle = DrvTCPInit("localhost", 2468);
			while(run)
			{
				static std::uint8_t buffer[1024];
				std::uint64_t rlen;
				DrvTCPRecv(handle, buffer, 1024, sizeof(float), &rlen);
				last_temp = *reinterpret_cast<float*>(buffer);
			}
			DrvTCPClose(handle);
		}).detach();
	}

	virtual void onRunStop(std::uint64_t runno) override
	{
		run = false;
	}

	virtual void periodic() override
	{
		if(!run)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			return;
		}

		evt_buffer.clear();
		log.info("Temperature seen by backend: %f.", last_temp.load());
		dispatchEvent("TemperatureReader::data", MxEventBuilder(evt_buffer)
			.add(SysGetCurrentTime())
			.add(last_temp.load())
		);
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}

private:
	std::vector<std::uint8_t> evt_buffer;
	std::atomic<bool> run = false;
	std::atomic<float> last_temp = 0.0f;
};

int main(int argc, char* argv[])
{
	TemperatureReader backend(argc, argv);
	backend.startEventLoop();
	return 0;
}
