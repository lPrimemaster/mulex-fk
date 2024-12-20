#pragma once
#include "network/rpc.h"
#include <cstdint>

namespace mulex
{
	void HttpStartServer(std::uint16_t port, bool islocal);
	void HttpStopServer();

	MX_RPC_METHOD mulex::RPCGenericType HttpGetClients();
} // namespace mulex
