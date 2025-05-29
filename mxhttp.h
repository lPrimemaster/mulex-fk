#pragma once
#include "mxtypes.h"
#include "network/rpc.h"
#include <cstdint>

namespace mulex
{
	void HttpStartServer(std::uint16_t port, bool islocal);
	void HttpStopServer();

	MX_RPC_METHOD mulex::RPCGenericType HttpGetClients();

	bool HttpRegisterUserPlugin(const std::string& plugin, std::int64_t timestamp);
	bool HttpUpdateUserPlugin(const std::string& plugin, std::int64_t timestamp);
	void HttpRemoveUserPlugin(const std::string& plugin);

	std::string HttpJWSIssue(const std::string& sub, const std::string& iss, std::int64_t exp);
	std::pair<bool, std::string> HttpJWSVerify(const std::string& token);

	void HttpInitUsersPdb();
	std::optional<std::pair<std::string, std::string>> HttpGetUserCredentials(const std::string& username);
} // namespace mulex
