#pragma once
#include "../mxtypes.h"
#include "../network/rpc.h"

MX_RPC_METHOD int RpcEchoValue(int v);
MX_RPC_METHOD mulex::string32 RpcEchoString(mulex::string32 v);
MX_RPC_METHOD mulex::string32 RpcEchoMultiArg(mulex::string32 v0, int v1);

MX_RPC_METHOD void RpcValueVoid(int v);
MX_RPC_METHOD void RpcMultiArgVoid(mulex::string32 v0, int v1);

MX_RPC_METHOD mulex::RPCGenericType RpcValueGeneric(int v);
MX_RPC_METHOD mulex::RPCGenericType RpcValueGenericMultiArg(int v0, mulex::RPCGenericType v1, float v2);

MX_RPC_METHOD mulex::RPCGenericType Rpc1MReturn();
MX_RPC_METHOD mulex::RPCGenericType Rpc10MReturn();
