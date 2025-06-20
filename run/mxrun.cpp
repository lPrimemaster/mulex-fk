#include "../mxrun.h"
#include "../mxrdb.h"
#include <cstdint>

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

	static void RunInitTables()
	{
		const std::string rlquery = 
		"CREATE TABLE IF NOT EXISTS runlog ("
			"id INTEGER PRIMARY KEY,"
			"name TEXT DEFAULT NULL,"
			"description TEXT DEFAULT NULL,"
			"started_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
			"stopped_at DATETIME DEFAULT NULL"
		");";

		const std::string rflquery = 
		"CREATE TABLE IF NOT EXISTS runlogxref ("
			"id INTEGER PRIMARY KEY AUTOINCREMENT,"
			"handle TEXT NOT NULL,"
			"run_id INTEGER NOT NULL,"
			"alias TEXT NOT NULL,"
			"client_id INTEGER NOT NULL,"
			"FOREIGN KEY (handle) REFERENCES storage_index(id),"
			"FOREIGN KEY (run_id) REFERENCES runlog(id)"
		");";

		if(!PdbExecuteQuery(rlquery) || !PdbExecuteQuery(rflquery))
		{
			LogError("[mxrun] Failed to initialize run tables.");
		}
	}

	static bool RunRegisterStart(std::uint64_t runno)
	{
		static const std::string query = "INSERT OR REPLACE INTO runlog (id) VALUES (?)";
		static const std::vector<PdbValueType> types = { PdbValueType::UINT64 };
		return PdbWriteTable(query, types, SysPackArguments(runno));
	}

	static bool RunRegisterStop(std::uint64_t runno)
	{
		const std::string query = "UPDATE runlog SET stopped_at = CURRENT_TIMESTAMP WHERE id = " + std::to_string(runno) + ";";
		return PdbExecuteQuery(query);
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

		// Init database tables
		RunInitTables();
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

		RunRegisterStart(runno);
		
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

		std::uint64_t runno = RdbReadValueDirect("/system/run/number").asType<std::uint64_t>();
		RunRegisterStop(runno);
		
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

		// Run reset also invalidates all of the files cache
		// one should proceed with caution
		
		// Delete run files
		LogWarning("[mxrun] RunReset as been called. Invalidating all run log files from the storage_index.");
		static PdbAccessLocal accessor;
		static auto reader = accessor.getReader<PdbString>("runlogxref", { "handle" });
		const auto handles = reader("");
		for(const auto& handle : handles)
		{
			const PdbString& hstr = std::get<0>(handle);
			LogDebug("[mxrun] Delete log file: %s", hstr.c_str());
			FdbDeleteFile(hstr);
		}

		// Delete run tables
		LogWarning("[mxrun] Invalidating run database.");
		PdbExecuteQuery("DELETE FROM runlog;");
		PdbExecuteQuery("DELETE FROM sqlite_sequence WHERE name = 'runlogxref';");
		PdbExecuteQuery("DELETE FROM runlogxref;");

		LogTrace("[mxrun] RunReset() OK.");
	}

	mulex::RPCGenericType RunLogGetRuns(std::uint64_t limit, std::uint64_t page)
	{
		static PdbAccessLocal accessor;
		static auto reader = accessor.getReaderRaw<std::uint64_t, PdbString, PdbString, PdbString>("runlog", { "id", "name", "started_at", "stopped_at" });
		return reader("ORDER BY id DESC LIMIT " + std::to_string(limit) + " OFFSET (" + std::to_string(limit) + " * " + std::to_string(page) + ")");
	}

	mulex::RPCGenericType RunLogGetMeta(std::uint64_t runno)
	{
		static PdbAccessLocal accessor;
		static auto reader_rf = accessor.getReaderRaw<PdbString, PdbString, std::uint64_t>("runlogxref", { "handle", "alias", "client_id" });

		const std::string query =
		"SELECT runlogxref.handle, runlogxref.alias, runlogxref.client_id, storage_index.created_at FROM runlogxref "
		"JOIN storage_index ON runlogxref.handle = storage_index.id "
		"WHERE run_id = " + std::to_string(runno) + ";";

		static const std::vector<PdbValueType> types = {
			PdbValueType::STRING,
			PdbValueType::STRING,
			PdbValueType::UINT64,
			PdbValueType::STRING
		};

		return PdbReadTable(query, types);
	}

	bool RunLogFile(RunLogFileMetadata data)
	{
		static PdbAccessLocal accessor;
		static auto writer = accessor.getWriter<PdbString, std::uint64_t, PdbString, std::uint64_t>("runlogxref", { "handle", "run_id", "alias", "client_id" });

		std::uint64_t cid = GetCurrentCallerId();
		if(!writer(data._handle, data._runno, data._alias, cid))
		{
			LogError("[mxrun] Failed to log run file.");
			return false;
		}

		LogTrace("[mxrun] RunLogFile() OK.");
		return true;
	}
}
