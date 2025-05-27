#pragma once
#include <cstdint>

namespace mulex
{
	enum class RunStatus : std::uint8_t
	{
		STOPPED,
		RUNNING,
		STARTING,
		STOPPING
	};

	void RunInitVariables();
	MX_RPC_METHOD MX_PERMISSION("run_control") bool RunStart();
	MX_RPC_METHOD MX_PERMISSION("run_control") void RunStop();
	MX_RPC_METHOD MX_PERMISSION("run_control") void RunReset();
} // namespace mulex
