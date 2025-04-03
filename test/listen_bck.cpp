#include "test.h"
#include "../mxbackend.h"
#include "../mxsystem.h"

using namespace mulex;

class ListenBackend : public MxBackend
{
public:
	ListenBackend(int argc, char* argv[], const std::string& event) : MxBackend(argc, argv)
	{
		log.info("ListenBackend looking for event: %s", event.c_str());
		subscribeEvent(event, [this, event](auto* d, auto sz, auto* udata){ log.info("[%s] Received %lld bytes.", event.c_str(), sz); });
	}
};

int main(int argc, char* argv[])
{
	std::string event;
	SysAddArgument("evt", 'e', true, [&](const std::string& evt){ event = evt; }, "The event name to subscribe to.");

	ListenBackend backend(argc, argv, event);
	backend.init();
	backend.spin();
	return 0;
}
