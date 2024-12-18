#include <numeric>

#include <rpcspec.inl>
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
			case PdbValueType::BINARY: return 0; // Calculated at insert/select time
			case PdbValueType::NIL: return 0;
		}
		return 0;
	}
	
	void PdbPushBufferBytes(const std::uint8_t* value, std::uint64_t size, std::vector<std::uint8_t>& buffer)
	{
		for(std::uint64_t i = 0; i < size; i++)
		{
			buffer.push_back(*(value + i));
		}
	}

	static void PdbTableGet(const PdbValueType& type, std::vector<std::uint8_t>& buffer, int col, sqlite3_stmt* stmt)
	{
		switch (type)
		{
			case PdbValueType::INT8:
			case PdbValueType::UINT8:
			case PdbValueType::BOOL:
			case PdbValueType::INT16:
			case PdbValueType::UINT16:
			case PdbValueType::INT32:
			case PdbValueType::UINT32:
			{
				const std::int32_t i32 = sqlite3_column_int(stmt, col);
				PdbPushBufferBytes(reinterpret_cast<const std::uint8_t*>(&i32), PdbTypeSize(type), buffer);
				break;
			}

			case PdbValueType::INT64:
			case PdbValueType::UINT64:
			{
				const std::int32_t i64 = sqlite3_column_int64(stmt, col);
				PdbPushBufferBytes(reinterpret_cast<const std::uint8_t*>(&i64), PdbTypeSize(type), buffer);
				break;
			}

			case PdbValueType::FLOAT32:
			{
				const float f = sqlite3_column_double(stmt, col);
				PdbPushBufferBytes(reinterpret_cast<const std::uint8_t*>(&f), PdbTypeSize(type), buffer);
				break;
			}

			case PdbValueType::FLOAT64:
			{
				const double f = sqlite3_column_double(stmt, col);
				PdbPushBufferBytes(reinterpret_cast<const std::uint8_t*>(&f), PdbTypeSize(type), buffer);
				break;
			}

			case PdbValueType::STRING:
			{
				PdbString s = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
				PdbPushBufferBytes(reinterpret_cast<const std::uint8_t*>(s.c_str()), PdbTypeSize(type), buffer);
				break;
			}

			case PdbValueType::BINARY:
			{
				const std::uint8_t* p = reinterpret_cast<const std::uint8_t*>(sqlite3_column_blob(stmt, col));
				std::int32_t size = sqlite3_column_bytes(stmt, col);
				PdbPushBufferBytes(reinterpret_cast<const std::uint8_t*>(&size), sizeof(std::int32_t), buffer);
				PdbPushBufferBytes(p, size, buffer);
			}

			case PdbValueType::NIL:
			{
				LogError("[pdb] Failed to get value. Can not get a NIL value.");
				break;
			}
		}
	}

	static bool PdbTableBind(const PdbValueType& type, const std::uint8_t* ptr, int col, sqlite3_stmt* stmt, std::uint64_t& offset)
	{
		switch (type)
		{
			case PdbValueType::INT8:  
			case PdbValueType::UINT8:
			case PdbValueType::BOOL:
			{
				offset += PdbTypeSize(type);
				return sqlite3_bind_int(stmt, col, *reinterpret_cast<const std::int8_t*>(ptr)) == SQLITE_OK;
			}
			case PdbValueType::INT16:
			case PdbValueType::UINT16:
			{
				offset += PdbTypeSize(type);
				return sqlite3_bind_int(stmt, col, *reinterpret_cast<const std::int16_t*>(ptr)) == SQLITE_OK;
			}
			case PdbValueType::INT32:
			case PdbValueType::UINT32:
			{
				offset += PdbTypeSize(type);
				return sqlite3_bind_int(stmt, col, *reinterpret_cast<const std::int32_t*>(ptr)) == SQLITE_OK;
			}
			case PdbValueType::INT64:
			case PdbValueType::UINT64:
			{
				offset += PdbTypeSize(type);
				return sqlite3_bind_int64(stmt, col, *reinterpret_cast<const std::int64_t*>(ptr)) == SQLITE_OK;
			}
			case PdbValueType::FLOAT32:
			{
				offset += PdbTypeSize(type);
				return sqlite3_bind_double(stmt, col, *reinterpret_cast<const float*>(ptr)) == SQLITE_OK;
			}
			case PdbValueType::FLOAT64:
			{
				offset += PdbTypeSize(type);
				return sqlite3_bind_double(stmt, col, *reinterpret_cast<const double*>(ptr)) == SQLITE_OK;
			}
			case PdbValueType::STRING:
			{
				offset += PdbTypeSize(type);
				return sqlite3_bind_text(stmt, col, reinterpret_cast<const PdbString*>(ptr)->c_str(), -1, SQLITE_STATIC) == SQLITE_OK;
			}
			case PdbValueType::BINARY:
			{
				// Increment here where we know the size of the data
				const std::uint64_t size = *reinterpret_cast<const std::uint64_t*>(ptr);
				offset += size;
				return sqlite3_bind_blob(stmt, col, ptr + sizeof(std::uint64_t), size, SQLITE_STATIC) == SQLITE_OK;
			}
			case PdbValueType::NIL:
				return sqlite3_bind_null(stmt, col) == SQLITE_OK;
			default:
				return false;
		}
	}

	bool PdbWriteTable(mulex::PdbQuery query, mulex::RPCGenericType types, mulex::RPCGenericType data)
	{
		sqlite3_stmt* stmt;
		if(sqlite3_prepare_v2(_pdb_handle, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
		{
			LogError("[pdb] Error creating query.");
			LogError("[pdb] %s.", sqlite3_errmsg(_pdb_handle));
			sqlite3_finalize(stmt);
			return false;
		}

		std::vector<PdbValueType> vars = types;
		std::vector<std::uint8_t> vdata = data;
		std::uint64_t offset = 0;
		for(int i = 0; i < static_cast<int>(vars.size()); i++)
		{
			if(!PdbTableBind(vars[i], vdata.data() + offset, i + 1, stmt, offset))
			{
				LogError("[pdb] Failed to bind value. Type might be wrong. Or data is unexpected.");
				LogError("[pdb] %s.", sqlite3_errmsg(_pdb_handle));
				sqlite3_finalize(stmt);
				return false;
			}
		}

		if(sqlite3_step(stmt) != SQLITE_DONE)
		{
			LogError("[pdb] Failed to execute statement.");
			LogError("[pdb] %s.", sqlite3_errmsg(_pdb_handle));
			sqlite3_finalize(stmt);
			return false;
		}

		sqlite3_finalize(stmt);
		LogTrace("[pdb] PdbWriteTable() OK.");
		return true;
	}

	static std::uint64_t PdbCalculateTypesSize(const std::vector<PdbValueType>& types)
	{
		std::uint64_t size = 0;
		for(const auto& t : types)
		{
			size += PdbTypeSize(t);
		}
		return size;
	}

	static std::vector<std::uint8_t> PdbFlattenList(const std::vector<std::vector<std::uint8_t>>& list)
	{
		std::vector<std::uint8_t> data;
		std::uint64_t tsize = std::accumulate(list.begin(), list.end(), 0, [](std::uint64_t s, const std::vector<std::uint8_t>& v) { return s + v.size(); });
		data.clear();
		data.reserve(tsize);

		for(const auto& v : list)
		{
			data.insert(data.end(), v.begin(), v.end());
		}

		return data;
	}

	mulex::RPCGenericType PdbReadTable(mulex::PdbQuery query, mulex::RPCGenericType types)
	{
		sqlite3_stmt* stmt;
		if(sqlite3_prepare_v2(_pdb_handle, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
		{
			LogError("[pdb] Error creating query.");
			LogError("[pdb] %s.", sqlite3_errmsg(_pdb_handle));
			sqlite3_finalize(stmt);
			return {};
		}

		std::vector<PdbValueType> vars = types;
		std::uint64_t size = PdbCalculateTypesSize(vars);
		std::vector<std::uint8_t> data;
		data.reserve(size);
		std::vector<std::vector<std::uint8_t>> list;
		while(sqlite3_step(stmt) == SQLITE_ROW)
		{
			for(int i = 0; i < static_cast<int>(vars.size()); i++)
			{
				PdbTableGet(vars[i], data, i, stmt);
			}
			list.push_back(data);
		}

		return PdbFlattenList(list);
	}

	static std::string PdbTypeName(const PdbValueType& type)
	{
		switch (type)
		{
			case PdbValueType::INT8:
			case PdbValueType::UINT8:
			case PdbValueType::BOOL:
			case PdbValueType::INT16:
			case PdbValueType::UINT16:
			case PdbValueType::INT32:
			case PdbValueType::UINT32:
			case PdbValueType::INT64:
			case PdbValueType::UINT64:
				return "INTEGER";
			case PdbValueType::FLOAT32:
			case PdbValueType::FLOAT64:
				return "REAL";
			case PdbValueType::STRING:
				return "TEXT";
			case PdbValueType::BINARY:
				return "BLOB";
			case PdbValueType::NIL:
				return "NULL";
		}
		return "";
	}

	bool PdbExecuteQuery(mulex::PdbQuery query)
	{
		char* err;
		if(sqlite3_exec(_pdb_handle, query.c_str(), nullptr, nullptr, &err) != SQLITE_OK)
		{
			LogError("[pdb] Failed to execute query <%s>.", query.c_str());
			sqlite3_free(err);
			return false;
		}
		return true;
	}

	std::string PdbGenerateSQLQueryCreate(const std::string& table, const std::initializer_list<std::string>& specs)
	{
		std::string query = "CREATE TABLE IF NOT EXISTS " + table + " (";
		// std::apply([&](auto&&... val){ ((query += val, query += ","), ...); }, spec);
		for(const auto& spec : specs)
		{
			query += spec;
			query += ",";
		}
		query.pop_back();
		query += ");";
		return query;
	}

	std::string PdbGenerateSQLQueryInsert(const std::string& table, const std::initializer_list<std::string>& names)
	{
		std::string query = "INSERT INTO " + table + " (";
		for(const auto& name : names)
		{
			query += name;
			query += ",";
		}
		// ([&]() { query += names; query += ","; }, ...);
		query.pop_back();
		query += ") VALUES (";
		for(const auto& name : names)
		{
			query += "?,";
		}
		// ([&]() { static_cast<void>(names), query += "?,"; }, ...);
		query.pop_back();
		query += ");";
		return query;
	}

	std::string PdbGenerateSQLQuerySelect(const std::string& table, const std::initializer_list<std::string>& names)
	{
		std::string query = "SELECT ";
		for(const auto& name : names)
		{
			query += name;
			query += ", ";
		}
		// ([&]() { query += names; query += ","; }, ...);
		query.pop_back();
		query.pop_back();
		query += " FROM " + table;
		return query;
	}

	bool PdbAccessPolicyRemote::executeQueryRemote(const std::string& query)
	{
		std::optional<const Experiment*> exp = SysGetConnectedExperiment();
		if(exp.has_value())
		{
			return exp.value()->_rpc_client->call<bool>(RPC_CALL_MULEX_PDBEXECUTEQUERY, PdbQuery(query));
		}
		return false;
	}

	bool PdbAccessPolicyRemote::executeInsertRemote(const std::string& query, const std::vector<PdbValueType>& types, const std::vector<uint8_t>& data)
	{
		std::optional<const Experiment*> exp = SysGetConnectedExperiment();
		if(exp.has_value())
		{
			return exp.value()->_rpc_client->call<bool>(RPC_CALL_MULEX_PDBWRITETABLE, PdbQuery(query), RPCGenericType(types), RPCGenericType(data));
		}
		return false;
	}

	std::vector<std::uint8_t> PdbAccessPolicyRemote::executeSelectRemoteI(const std::string& query, const std::vector<PdbValueType>& types)
	{
		std::optional<const Experiment*> exp = SysGetConnectedExperiment();
		if(exp.has_value())
		{
			return exp.value()->_rpc_client->call<RPCGenericType>(RPC_CALL_MULEX_PDBREADTABLE, PdbQuery(query), RPCGenericType(types));
		}
		return {};
	}

	bool PdbAccessPolicyLocal::executeQueryRemote(const std::string& query)
	{
		return PdbExecuteQuery(query);
	}

	bool PdbAccessPolicyLocal::executeInsertRemote(const std::string& query, const std::vector<PdbValueType>& types, const std::vector<uint8_t>& data)
	{
		return PdbWriteTable(PdbQuery(query), types, data);
	}

	std::vector<std::uint8_t> PdbAccessPolicyLocal::executeSelectRemoteI(const std::string& query, const std::vector<PdbValueType>& types)
	{
		return PdbReadTable(PdbQuery(query), types);
	}
} // namespace mulex
