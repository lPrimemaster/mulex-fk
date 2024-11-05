#include "test.h"
#include "../network/rpc.h"
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

mulex::RPCGenericType RpcValueGeneric(int v)
{
	mulex::LogDebug("Inside Data: %d", v);
	mulex::RPCGenericType rgt(v);
	mulex::LogDebug("Inside Data: %d", rgt.asType<int>());
	for(auto byte : rgt._data)
	{
		mulex::LogDebug("Inside Byte: %d", byte);
	}
	return rgt;
}

mulex::RPCGenericType RpcValueGenericMultiArg(int v0, mulex::RPCGenericType v1, float v2)
{

	mulex::RPCGenericType rgt(v1.asType<float>() + v2);
	mulex::LogDebug("Inside Data: %f", rgt.asType<float>());
	for(auto byte : rgt._data)
	{
		mulex::LogDebug("Inside Byte: %d", byte);
	}
	return rgt;
}

mulex::RPCGenericType Rpc1MReturn()
{
	std::vector<std::uint8_t> data;
	data.resize(1024 * 1024);
	std::fill(data.begin(), data.end(), 5);
	return data;
}

mulex::RPCGenericType Rpc10MReturn()
{
	std::vector<std::uint8_t> data;
	data.resize(1024 * 1024 * 10);
	std::fill(data.begin(), data.end(), 7);
	return data;
}

int main(int argc, char* argv[])
{
	mulex::RPCServerThread rst;
	while(!rst.ready())
	{
		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}

	mulex::RPCClientThread rct("localhost");

	int echo = rct.call<int, int>(RPC_CALL_RPCECHOVALUE, 42);

	mulex::LogDebug("Got echo: %d", echo);
	std::this_thread::sleep_for(std::chrono::milliseconds(10));

	mulex::string32 echos = rct.call<mulex::string32, mulex::string32>(
		RPC_CALL_RPCECHOSTRING,
		"randomstr"
	);
	mulex::LogDebug("Got echo: %s", echos.c_str());

	mulex::string32 echosm = rct.call<mulex::string32, mulex::string32, int>(
		RPC_CALL_RPCECHOMULTIARG,
		"randomstr",
		75
	);
	mulex::LogDebug("Got echo: %s", echosm.c_str());

	{
		timed_block tb;
		rct.call(RPC_CALL_RPCVALUEVOID, 4);
		rct.call(RPC_CALL_RPCMULTIARGVOID, mulex::string32("randomstr"), 4);
	}

	mulex::RPCGenericType echo2 = rct.call<mulex::RPCGenericType, int>(RPC_CALL_RPCVALUEGENERIC, 42);

	for(auto byte : echo2._data)
	{
		mulex::LogDebug("Byte: %d", byte);
	}
	mulex::LogDebug("Data: %d", echo2.asType<int>());

	mulex::RPCGenericType echo3 = rct.call<mulex::RPCGenericType, int, mulex::RPCGenericType, float>(
		RPC_CALL_RPCVALUEGENERICMULTIARG,
		1,
		mulex::RPCGenericType(2.0),
		3.0f
	);
	for(auto byte : echo3._data)
	{
		mulex::LogDebug("Byte: %d", byte);
	}
	mulex::LogDebug("Data: %f", echo3.asType<float>());

	mulex::RPCGenericType r[2];
	int i = 0;
	for(std::uint16_t call : {RPC_CALL_RPC1MRETURN, RPC_CALL_RPC10MRETURN})
	{
		timed_block tb("", false);
		tb.mstart();
		r[i] = rct.call<mulex::RPCGenericType>(call);
		float ms = tb.mstop();
		float payload_mb = (float)r[i++]._data.size() / (1024 * 1024);
		mulex::LogDebug("Estimated connection speed (Mb/s): %f", payload_mb / (ms / 1000));
	}

	ASSERT_THROW(echo == 42);
	ASSERT_THROW(echos.c_str() == std::string("randomstr"));
	ASSERT_THROW(echosm.c_str() == std::string("randomstr75"));
	ASSERT_THROW(echo2.asType<int>() == 42);
	ASSERT_THROW(echo3.asType<float>() == 3.0f);
	ASSERT_THROW(r[0]._data.size() == 1024 * 1024);
	ASSERT_THROW(r[1]._data.size() == 1024 * 1024 * 10);

	return 0;
}
