#include <sqlite3.h>
#include "../mxrdb.h"

static sqlite3* _pdb_handle = nullptr;

namespace mulex
{
	void PdbInit()
	{
		std::string_view cache = SysGetCacheDir();
		if(cache.empty())
		{
			LogError("[pdb] Failed to init PDB.");
			return;
		}

		// if(sqlite3_open(std::string(cache) + std::string("/pdb.db")))
	}

	void PdbClose()
	{
	}
} // namespace mulex
