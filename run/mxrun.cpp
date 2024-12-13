#include "../mxrun.h"
#include "../mxrdb.h"
#include "../mxevt.h"

namespace mulex
{
	static const char* RunStatusToString(RunStatus status)
	{
		switch(status)
		{
			case RunStatus::STOPPED:  return "STOPPED";
			case RunStatus::RUNNING:  return "RUNNING";
			case RunStatus::STARTING: return "STARTING";
			case RunStatus::STOPPING: return "STOPPING";
		}
		return "";
	}

	void RunInitVariables()
	{
		LogTrace("[mxrun] Initializing run metadata.");

		// Status MEMO:
		// 0 - Stopped
		// 1 - Running
		// 2 - Starting
		// 3 - Stopping
		RdbNewEntry("/system/run/status", RdbValueType::UINT8, 0);

		// On server init run status is always set to stopped regardless of the saved value
		RdbWriteValueDirect("/system/run/status", std::uint8_t(0));
		
		// Run number incrementing indefinitely 
		RdbNewEntry("/system/run/number", RdbValueType::UINT64, 0);

		// Run last start timestamp
		RdbNewEntry("/system/run/timestamp", RdbValueType::INT64, 0);
	}

	bool RunStart()
	{
		// Check the status
		RunStatus status = static_cast<RunStatus>(RdbReadValueDirect("/system/run/status").asType<std::uint8_t>());

		if(status != RunStatus::STOPPED)
		{
			LogError("[mxrun] Failed to start run. Status is '%s'. Status 'STOPPED' is required.", RunStatusToString(status));
			return false;
		}

		// RdbWriteValueDirect("/system/run/status", std::uint8_t(2));

		// Increment the run
		std::uint64_t runno = RdbReadValueDirect("/system/run/number").asType<std::uint64_t>() + 1;
		RdbWriteValueDirect("/system/run/number", runno);

		// For now we don't do anything else
		
		RdbWriteValueDirect("/system/run/timestamp", SysGetCurrentTime());
		RdbWriteValueDirect("/system/run/status", std::uint8_t(1));

		LogTrace("[mxrun] RunStart() OK.");

		return true;
	}

	void RunStop()
	{
		// Check the status
		RunStatus status = static_cast<RunStatus>(RdbReadValueDirect("/system/run/status").asType<std::uint8_t>());

		if(status != RunStatus::RUNNING)
		{
			LogError("[mxrun] Failed to stop run. Status is '%s'. Status 'RUNNING' is required.", RunStatusToString(status));
			return;
		}

		// RdbWriteValueDirect("/system/run/status", std::uint8_t(3));

		// For now we don't do anything else
		
		RdbWriteValueDirect("/system/run/status", std::uint8_t(0));

		LogTrace("[mxrun] RunStop() OK.");
	}

	void RunReset()
	{
		// Check the status
		RunStatus status = static_cast<RunStatus>(RdbReadValueDirect("/system/run/status").asType<std::uint8_t>());

		if(status != RunStatus::STOPPED)
		{
			LogError("[mxrun] Failed to reset run number. Status is '%s'. Status 'STOPPED' is required.", RunStatusToString(status));
			return;
		}

		RdbWriteValueDirect("/system/run/number", std::uint64_t(0));

		LogTrace("[mxrun] RunReset() OK.");
	}
}
