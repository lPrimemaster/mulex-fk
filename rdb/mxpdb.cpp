#include <sqlite3.h>
#include "../mxrdb.h"

static sqlite3* _pdb_handle = nullptr;

namespace mulex
{
	static std::string PdbGetLocation(const std::string& cache)
	{
		RdbEntry* pdb = RdbFindEntryByName("/system/pdbloc");
		std::string pdb_loc;
		if(!pdb)
		{
			// Register the pdb path on rdb
			pdb_loc = cache + "/pdb.db";
			pdb = RdbNewEntry("/system/pdbloc", RdbValueType::STRING, pdb_loc.c_str());
			if(!pdb)
			{
				LogError("[pdb] Failed to register pdb path on rdb.");
				return "";
			}
		}
		else
		{
			pdb_loc = RdbReadValueDirect("/system/pdbloc").asType<mxstring<512>>().c_str();
		}

		LogTrace("[pdb] Got pdb location: %s", pdb_loc.c_str());
		return pdb_loc;
	}

	void PdbInit()
	{
		std::string cache = SysGetExperimentHome();
		if(cache.empty())
		{
			LogError("[pdb] pdb requires a named experiment to run.");
			LogError("[pdb] Failed to init pdb.");
			return;
		}

		std::string loc = PdbGetLocation(cache);

		if(loc.empty())
		{
			return;
		}

		if(sqlite3_open(loc.c_str(), &_pdb_handle))
		{
			LogError("[pdb] Failed to open database. Error: %s", sqlite3_errmsg(_pdb_handle));
			return;
		}

		LogDebug("[pdb] Init() OK.");
	}

	void PdbClose()
	{
		LogDebug("[pdb] Closing pdb.");
		if(_pdb_handle)
		{
			sqlite3_close(_pdb_handle);
		}
	}
} // namespace mulex
