#include "test.h"
#include "../rpc/rpc.h"
#include "../rpc/socket.h"
#include "../mxlogger.h"
#include <cstring>
#include "rpc.h"

#include <rpcspec.inl>

int RpcEchoValue(int v)
{
	return v;
}

mulex::string32 RpcEchoString(mulex::string32 v)
{
	return v;
}

mulex::string32 RpcEchoMultiArg(mulex::string32 v0, int v1)
{
	char out[32];
	sprintf(out, "%s%d", v0.c_str(), v1);
	return out;
}

void RpcValueVoid(int v)
{
	mulex::LogDebug("%d", v);
}

void RpcMultiArgVoid(mulex::string32 v0, int v1)
{
	mulex::LogDebug("%s", v0.c_str());
	mulex::LogDebug("%d", v1);
}

int main(int argc, char* argv[])
{
	mulex::RPCServerThread rst;
	while(!rst.ready())
	{
		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}

	mulex::SysConnectToExperiment("localhost");

	int echo = mulex::CallRemoteFunction<int, int>(RPC_CALL_RPCECHOVALUE, 42);

	mulex::LogDebug("Got echo: %d", echo);
	std::this_thread::sleep_for(std::chrono::milliseconds(10));

	mulex::string32 echos = mulex::CallRemoteFunction<mulex::string32, mulex::string32>(
		RPC_CALL_RPCECHOSTRING,
		"randomstr"
	);
	mulex::LogDebug("Got echo: %s", echos.c_str());

	mulex::string32 echosm = mulex::CallRemoteFunction<mulex::string32, mulex::string32, int>(
		RPC_CALL_RPCECHOMULTIARG,
		"randomstr",
		75
	);
	mulex::LogDebug("Got echo: %s", echosm.c_str());

	mulex::CallRemoteFunction(RPC_CALL_RPCVALUEVOID, 4);
	mulex::CallRemoteFunction(RPC_CALL_RPCMULTIARGVOID, mulex::string32("randomstr"), 4);

	ASSERT_THROW(echo == 42);
	ASSERT_THROW(echos.c_str() == std::string("randomstr"));
	ASSERT_THROW(echosm.c_str() == std::string("randomstr75"));

	mulex::SysDisconnectFromExperiment();

	return 0;
}
