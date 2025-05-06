#include "test.h"
#include "../mxbackend.h"
#include "../mxsystem.h"
#include "../rexs/mxrexs.h"
#include "rpcspec.inl"

using namespace mulex;

class RexTestBackend : public MxBackend
{
public:
	RexTestBackend(int argc, char* argv[], const std::string& client) : MxBackend(argc, argv)
	{
		std::uint64_t cid = std::stoull(client, 0, 16);
		log.info("Remotely starting via REx: 0x%llx.", cid);

		std::optional<const Experiment*> exp = SysGetConnectedExperiment();
		if(!exp.has_value())
		{
			return;
		}

		mulex::RexCommandStatus status = exp.value()->_rpc_client->call<mulex::RexCommandStatus>(RPC_CALL_MULEX_REXSENDSTARTCOMMAND, cid);
		log.info("RPC_CALL_MULEX_REXSENDSTARTCOMMAND sent: %d", static_cast<std::uint8_t>(status));
	}
};

int main(int argc, char* argv[])
{
	std::string client;
	SysAddArgument("cid", 'c', true, [&](const std::string& cid){ client = cid; }, "The backend to start remotely (HEX cid).");

	RexTestBackend backend(argc, argv, client);
	backend.init();
	return 0;
}
