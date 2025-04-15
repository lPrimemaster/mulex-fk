#pragma once
#include "network/rpc.h"
#include <cstdint>

namespace mulex
{
	void HttpStartServer(std::uint16_t port, bool islocal, const std::string& ssl_path);
	void HttpStopServer();

	MX_RPC_METHOD mulex::RPCGenericType HttpGetClients();
	bool HttpRegisterUserPlugin(const std::string& plugin);
	void HttpRemoveUserPlugin(const std::string& plugin);
} // namespace mulex
