#pragma once
#include <cstdint>
#include "mxrdb.h"
#include "mxtypes.h"
#include "network/rpc.h"

namespace mulex
{
	enum class RunStatus : std::uint8_t
	{
		STOPPED,
		RUNNING,
		STARTING,
		STOPPING
	};

	struct RunLogFileMetadata
	{
		std::uint64_t 	 _runno;
		mulex::FdbHandle _handle;
		mulex::string512 _alias;
	};

	void RunInitVariables();
	MX_RPC_METHOD MX_PERMISSION("run_control") bool RunStart();
	MX_RPC_METHOD MX_PERMISSION("run_control") void RunStop();
	MX_RPC_METHOD MX_PERMISSION("run_reset") void RunReset();

	MX_RPC_METHOD mulex::RPCGenericType RunLogGetRuns(std::uint64_t limit, std::uint64_t page);
	MX_RPC_METHOD mulex::RPCGenericType RunLogGetMeta(std::uint64_t runno);
	MX_RPC_METHOD bool RunLogFile(mulex::RunLogFileMetadata data);
} // namespace mulex
