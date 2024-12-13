#include "../mxbackend.h"
#include "../mxlogger.h"
#include "../mxsystem.h"
#include "../mxdrv.h"
#include <chrono>
#include <rpcspec.inl>
#include <thread>

using namespace mulex;

class CoincidenceAnalyzer : public MxBackend
{
public:
	CoincidenceAnalyzer(int argc, char* argv[]) : MxBackend(argc, argv)
	{
		log.error("This is a very big error string from the constructor of CoincidenceAnalyzer!");
		log.warn("This is a very big warning string from the constructor of CoincidenceAnalyzer!");
		log.info("This is a very big info string from the constructor of CoincidenceAnalyzer!");

		LogTrace("Backend name: %s", std::string(SysGetBinaryName()).c_str());
	}

	~CoincidenceAnalyzer()
	{
	}

	virtual void onRunStart(std::uint64_t runno) override
	{
		subscribeEvent("TemperatureReader::data", [this](auto* data, auto len, auto* udata) {
			temperature.store(*reinterpret_cast<const float*>(data + sizeof(std::int64_t)));
		});

		// Trigger signal to write some data
		subscribeEvent("SignalReader::data", [this](auto* data, auto len, auto* udata) {
			std::int64_t timestamp = *reinterpret_cast<const std::int64_t*>(data);
			const std::uint8_t* signal = data + sizeof(std::int64_t);
			std::array<std::uint8_t, 1024> signalarr;
			std::memcpy(signalarr.data(), signal, 1024);
			datav.push_back(
				std::make_tuple(
					timestamp,
					temperature.load(),
					std::move(signalarr)
				)
			);

		});
	}

	virtual void onRunStop(std::uint64_t runno) override
	{
		unsubscribeEvent("TemperatureReader::data");
		unsubscribeEvent("SignalReader::data");

		log.info("Writing coincidence csv file.");
		std::ofstream file("mx_coincidence_demo.csv");
		for(const auto& [timestamp, temperature, signal] : datav)
		{
			// Write CSV
			file << timestamp << "," << temperature << "," << "\"[";

			file << static_cast<int>(signal[0]);
			for(std::uint64_t i = 1; i < signal.size(); i++)
			{
				file << "," << static_cast<int>(signal[i]);
			}
			file << "]\"\n";
		}
		datav.clear();
		log.info("Done!");
	}

	virtual void periodic() override
	{
		log.info("Analyzer is looping!");
		std::this_thread::sleep_for(std::chrono::milliseconds(10000));
	}

private:
	std::vector<std::tuple<std::int64_t, float, std::array<std::uint8_t, 1024>>> datav;
	std::atomic<float> temperature = 0;
};

int main(int argc, char* argv[])
{
	CoincidenceAnalyzer backend(argc, argv);
	backend.startEventLoop();
	return 0;
}
