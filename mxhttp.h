#pragma once
#include <cstdint>

namespace mulex
{
	void HttpStartServer(std::uint16_t port, bool islocal);
	void HttpStopServer();
} // namespace mulex
