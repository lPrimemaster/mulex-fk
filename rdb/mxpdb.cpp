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

	std::uint64_t PdbTypeSize(const PdbValueType& type)
	{
		switch (type)
		{
			case PdbValueType::INT8: return sizeof(std::int8_t);
			case PdbValueType::INT16: return sizeof(std::int16_t);
			case PdbValueType::INT32: return sizeof(std::int32_t);
			case PdbValueType::INT64: return sizeof(std::int64_t);
			case PdbValueType::UINT8: return sizeof(std::uint8_t);
			case PdbValueType::UINT16: return sizeof(std::uint16_t);
			case PdbValueType::UINT32: return sizeof(std::uint32_t);
			case PdbValueType::UINT64: return sizeof(std::uint64_t);
			case PdbValueType::FLOAT32: return sizeof(float);
			case PdbValueType::FLOAT64: return sizeof(double);
			case PdbValueType::STRING: return PDB_MAX_STRING_SIZE; // HACK: For now store the max size
			case PdbValueType::BOOL: return sizeof(bool);
		}
		return 0;
	}

	static bool PdbTableBind(const PdbValueType& type, const std::uint8_t* ptr, int col, sqlite3_stmt* stmt)
	{
		switch (type)
		{
			case PdbValueType::INT8:  
			case PdbValueType::UINT8:
			case PdbValueType::BOOL:
				return sqlite3_bind_int(stmt, col, *reinterpret_cast<const std::int8_t*>(ptr)) == SQLITE_OK;
			case PdbValueType::INT16:
			case PdbValueType::UINT16:
				return sqlite3_bind_int(stmt, col, *reinterpret_cast<const std::int16_t*>(ptr)) == SQLITE_OK;
			case PdbValueType::INT32:
			case PdbValueType::UINT32:
				return sqlite3_bind_int(stmt, col, *reinterpret_cast<const std::int32_t*>(ptr)) == SQLITE_OK;
			case PdbValueType::INT64:
			case PdbValueType::UINT64:
				return sqlite3_bind_int(stmt, col, *reinterpret_cast<const std::int64_t*>(ptr)) == SQLITE_OK;
			case PdbValueType::FLOAT32:
				return sqlite3_bind_double(stmt, col, *reinterpret_cast<const float*>(ptr)) == SQLITE_OK;
			case PdbValueType::FLOAT64:
				return sqlite3_bind_double(stmt, col, *reinterpret_cast<const double*>(ptr)) == SQLITE_OK;
			case PdbValueType::STRING:
				return sqlite3_bind_text(stmt, col, reinterpret_cast<const PdbString*>(ptr)->c_str(), -1, SQLITE_STATIC) == SQLITE_OK;
			default:
				return false;
		}
	}

	void PdbWriteTableRow(mulex::PdbQuery query, mulex::RPCGenericType types, mulex::RPCGenericType data)
	{
		sqlite3_stmt* stmt;
		if(sqlite3_prepare_v2(_pdb_handle, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
		{
			LogError("[pdb] Error creating query.");
			LogError("[pdb] %s.", sqlite3_errmsg(_pdb_handle));
			sqlite3_finalize(stmt);
			return;
		}

		std::vector<PdbValueType> vars = types;
		std::vector<std::uint8_t> vdata = data;
		std::uint64_t offset = 0;
		for(int i = 0; i < static_cast<int>(vars.size()); i++)
		{
			if(!PdbTableBind(vars[i], vdata.data() + offset, i, stmt))
			{
				LogError("[pdb] Failed to bind value. Type might be wrong. Or data is unexpected.");
				sqlite3_finalize(stmt);
				return;
			}
			offset += PdbTypeSize(vars[i]);
		}

		if(sqlite3_step(stmt) != SQLITE_DONE)
		{
			LogError("[pdb] Failed to execute statement.");
			LogError("[pdb] %s.", sqlite3_errmsg(_pdb_handle));
			sqlite3_finalize(stmt);
			return;
		}

		sqlite3_finalize(stmt);
		LogTrace("[pdb] PdbWriteTableRow() OK.");
	}
} // namespace mulex
