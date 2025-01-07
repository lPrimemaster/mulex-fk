#include "../mxbackend.h"
#include "../mxlogger.h"
#include "../mxsystem.h"
#include "../mxdrv.h"
#include <chrono>
#include <rpcspec.inl>
#include <thread>

using namespace mulex;

class SignalReader : public MxBackend
{
public:
	SignalReader(int argc, char* argv[]) : MxBackend(argc, argv)
	{
		log.error("This is a very big error string from the constructor of SignalReader!");
		log.warn("This is a very big warning string from the constructor of SignalReader!");
		log.info("This is a very big info string from the constructor of SignalReader!");

		registerEvent("SignalReader::data");
		LogTrace("Backend name: %s", std::string(SysGetBinaryName()).c_str());

		evt_buffer.reserve(2048);
		last_signal.resize(1024);
	}

	~SignalReader()
	{
	}

	virtual void onRunStart(std::uint64_t runno) override
	{
		run = true;

		std::thread([this](){
			DrvTCP handle = DrvTCPInit("localhost", 1357);
			while(run)
			{
				std::uint64_t rlen;
				static std::uint8_t buffer[1024];
				DrvTCPRecv(handle, buffer, 1024, 1024, &rlen);

				{
					std::unique_lock lock(smutex);
					std::memcpy(last_signal.data(), buffer, 1024);
				}
				cv.notify_one();
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

		std::unique_lock lock(smutex);
		cv.wait(lock);
		// log.info("Signal size seen by backend: %llu.", last_signal.size());
		dispatchEvent("SignalReader::data", MxEventBuilder(evt_buffer)
			.add(SysGetCurrentTime())
			.add(last_signal)
		);
	}

private:
	std::vector<std::uint8_t> evt_buffer;
	std::atomic<bool> run = false;

	std::mutex smutex;
	std::vector<std::uint8_t> last_signal;
	std::condition_variable cv;
};

int main(int argc, char* argv[])
{
	SignalReader backend(argc, argv);
	backend.startEventLoop();
	return 0;
}
