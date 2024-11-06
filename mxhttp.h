#pragma once
#include <cstdint>

namespace mulex
{
	void HttpStartServer(std::uint16_t port);
	void HttpStopServer();
} // namespace mulex
