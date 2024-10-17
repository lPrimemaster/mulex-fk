#pragma once
#include "../mxtypes.h"

MX_RPC_METHOD int RpcEchoValue(int v);
MX_RPC_METHOD mulex::string32 RpcEchoString(mulex::string32 v);
MX_RPC_METHOD mulex::string32 RpcEchoMultiArg(mulex::string32 v0, int v1);

MX_RPC_METHOD void RpcValueVoid(int v);
MX_RPC_METHOD void RpcMultiArgVoid(mulex::string32 v0, int v1);
