#pragma once
#include "mxtypes.h"
#include "mxlogger.h"

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
	MX_RPC_METHOD bool RunStart();
	MX_RPC_METHOD void RunStop();
	MX_RPC_METHOD void RunReset();
} // namespace mulex
