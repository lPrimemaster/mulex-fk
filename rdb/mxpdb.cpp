#include <numeric>

#include <rpcspec.inl>
#include <sqlite3.h>
#include "../mxrdb.h"
#include "mxres.h"

#include <tracy/Tracy.hpp>

static sqlite3* _pdb_handle = nullptr;

namespace mulex
{
	static std::string PdbGetLocation(const std::string& cache)
	{
		ZoneScoped;
		RdbEntry* pdb = RdbFindEntryByName("/system/pdbloc");
		std::string pdb_loc;
		if(!pdb)
		{
			// Register the pdb path on rdb
			pdb_loc = cache + "/pdb.db";
			pdb = RdbNewEntry("/system/pdbloc", RdbValueType::STRING, mxstring<512>(pdb_loc).c_str());
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
		ZoneScoped;
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
		ZoneScoped;
		LogDebug("[pdb] Closing pdb.");
		if(_pdb_handle)
		{
			sqlite3_close(_pdb_handle);
		}
	}

	std::uint64_t PdbTypeSize(const PdbValueType& type)
	{
		ZoneScoped;
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
		ZoneScoped;
		for(std::uint64_t i = 0; i < size; i++)
		{
			buffer.push_back(*(value + i));
		}
	}

	static void PdbTableGet(const PdbValueType& type, std::vector<std::uint8_t>& buffer, int col, sqlite3_stmt* stmt)
	{
		ZoneScoped;
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
				const std::int64_t i64 = sqlite3_column_int64(stmt, col);
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
		ZoneScoped;
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
		ZoneScoped;
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
		return true;
	}

	static std::uint64_t PdbCalculateTypesSize(const std::vector<PdbValueType>& types)
	{
		ZoneScoped;
		std::uint64_t size = 0;
		for(const auto& t : types)
		{
			size += PdbTypeSize(t);
		}
		return size;
	}

	static std::vector<std::uint8_t> PdbFlattenList(const std::vector<std::vector<std::uint8_t>>& list)
	{
		ZoneScoped;
		std::vector<std::uint8_t> data;
		std::uint64_t tsize = std::accumulate(list.begin(), list.end(), 0, [](std::uint64_t s, const std::vector<std::uint8_t>& v) { return s + v.size(); });
		data.reserve(tsize);

		for(const auto& v : list)
		{
			data.insert(data.end(), v.begin(), v.end());
		}

		return data;
	}

	mulex::RPCGenericType PdbReadTable(mulex::PdbQuery query, mulex::RPCGenericType types)
	{
		ZoneScoped;
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
			data.clear();
		}

		sqlite3_finalize(stmt);

		return PdbFlattenList(list);
	}

	bool PdbTableExists(const std::string& table)
	{
		ZoneScoped;
		sqlite3_stmt* stmt;
		bool exists = false;
		const std::string query = "SELECT count(TYPE) FROM sqlite_master WHERE TYPE='table' and name='" + table + "';";
		if(sqlite3_prepare_v2(_pdb_handle, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
		{
			LogError("[pdb] Error creating query.");
			LogError("[pdb] %s.", sqlite3_errmsg(_pdb_handle));
			sqlite3_finalize(stmt);
			return false;
		}

		if(sqlite3_step(stmt) == SQLITE_ROW)
		{
			exists = (sqlite3_column_int(stmt, 0) == 1);
		}

		sqlite3_finalize(stmt);
		return exists;
	}

	void PdbSetupUserDatabase()
	{
		const std::vector<std::uint8_t> data = ResGetResource("user_database.sql");
		const std::string query = std::string(reinterpret_cast<const char*>(data.data()), data.size());
		PdbExecuteQueryUnrestricted(query);
	}

	std::string PdbGetUserRole(const std::string& username)
	{
		const static std::vector<PdbValueType> types = { PdbValueType::STRING };
		auto data = PdbReadTable(
			"SELECT r.name AS role "
			"FROM users u "
			"JOIN roles r ON u.role_id = r.id "
			"WHERE u.username = '" + username + "';", types
		);

		if(data.getSize() == 0)
		{
			LogError("[pdb] No such username %s.", username.c_str());
			return "";
		}

		return data.asType<mulex::PdbString>().c_str();
	}

	std::int32_t PdbGetUserRoleId(const std::string& username)
	{
		const static std::vector<PdbValueType> types = { PdbValueType::INT32 };
		auto data = PdbReadTable(
			"SELECT r.id AS role "
			"FROM users u "
			"JOIN roles r ON u.role_id = r.id "
			"WHERE u.username = '" + username + "';", types
		);

		if(data.getSize() == 0)
		{
			LogError("[pdb] No such username %s.", username.c_str());
			return -1;
		}

		return data.asType<std::int32_t>();
	}

	std::int32_t PdbGetRoleId(const std::string& role)
	{
		const static std::vector<PdbValueType> types = { PdbValueType::INT32 };
		auto data = PdbReadTable(
			"SELECT id "
			"FROM roles "
			"WHERE name = '" + role + "';", types
		);

		if(data.getSize() == 0)
		{
			LogError("[pdb] No such role %s.", role.c_str());
			return -1;
		}

		return data.asType<std::int32_t>();
	}

	std::int32_t PdbGetUserId(const std::string& username)
	{
		const static std::vector<PdbValueType> types = { PdbValueType::INT32 };
		auto data = PdbReadTable(
			"SELECT id "
			"FROM users "
			"WHERE username = '" + username + "';", types
		);

		if(data.getSize() == 0)
		{
			LogError("[pdb] No such user %s.", username.c_str());
			return -1;
		}

		return data.asType<std::int32_t>();
	}

	static std::string PdbTypeName(const PdbValueType& type)
	{
		ZoneScoped;
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

	bool PdbExecuteQueryUnrestricted(const std::string& query)
	{
		ZoneScoped;
		char* err;
		if(sqlite3_exec(_pdb_handle, query.c_str(), nullptr, nullptr, &err) != SQLITE_OK)
		{
			LogError("[pdb] Failed to execute query <%s>.", query.c_str());
			LogError("[pdb] %s.", err);
			sqlite3_free(err);
			return false;
		}
		return true;
	}

	bool PdbExecuteQuery(mulex::PdbQuery query)
	{
		ZoneScoped;
		char* err;
		if(sqlite3_exec(_pdb_handle, query.c_str(), nullptr, nullptr, &err) != SQLITE_OK)
		{
			LogError("[pdb] Failed to execute query <%s>.", query.c_str());
			LogError("[pdb] %s.", err);
			sqlite3_free(err);
			return false;
		}
		return true;
	}

	std::string PdbGenerateSQLQueryCreate(const std::string& table, const std::initializer_list<std::string>& specs)
	{
		ZoneScoped;
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
		ZoneScoped;
		std::string query = "INSERT INTO " + table + " (";
		for(const auto& name : names)
		{
			query += name;
			query += ",";
		}
		// ([&]() { query += names; query += ","; }, ...);
		query.pop_back();
		query += ") VALUES (";
		for([[maybe_unused]] const auto& name : names)
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
		ZoneScoped;
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
		ZoneScoped;
		std::optional<const Experiment*> exp = SysGetConnectedExperiment();
		if(exp.has_value())
		{
			return exp.value()->_rpc_client->call<bool>(RPC_CALL_MULEX_PDBEXECUTEQUERY, PdbQuery(query));
		}
		return false;
	}

	bool PdbAccessPolicyRemote::executeInsertRemote(const std::string& query, const std::vector<PdbValueType>& types, const std::vector<uint8_t>& data)
	{
		ZoneScoped;
		std::optional<const Experiment*> exp = SysGetConnectedExperiment();
		if(exp.has_value())
		{
			return exp.value()->_rpc_client->call<bool>(RPC_CALL_MULEX_PDBWRITETABLE, PdbQuery(query), RPCGenericType(types), RPCGenericType(data));
		}
		return false;
	}

	std::vector<std::uint8_t> PdbAccessPolicyRemote::executeSelectRemoteI(const std::string& query, const std::vector<PdbValueType>& types)
	{
		ZoneScoped;
		std::optional<const Experiment*> exp = SysGetConnectedExperiment();
		if(exp.has_value())
		{
			return exp.value()->_rpc_client->call<RPCGenericType>(RPC_CALL_MULEX_PDBREADTABLE, PdbQuery(query), RPCGenericType(types));
		}
		return {};
	}

	bool PdbAccessPolicyLocal::executeQueryRemote(const std::string& query)
	{
		ZoneScoped;
		return PdbExecuteQuery(query);
	}

	bool PdbAccessPolicyLocal::executeInsertRemote(const std::string& query, const std::vector<PdbValueType>& types, const std::vector<uint8_t>& data)
	{
		ZoneScoped;
		return PdbWriteTable(PdbQuery(query), types, data);
	}

	std::vector<std::uint8_t> PdbAccessPolicyLocal::executeSelectRemoteI(const std::string& query, const std::vector<PdbValueType>& types)
	{
		ZoneScoped;
		return PdbReadTable(PdbQuery(query), types);
	}

	static bool PdbCheckCurrentUserRolePermissionHierarchy(const std::string& username, const std::string& role)
	{
		std::int32_t current_role = PdbGetUserRoleId(username);
		std::int32_t requested_role = PdbGetRoleId(role);

		if(current_role < 0)
		{
			LogError("[pdb] Failed to fetch role from user %s.", username.c_str());
			return false;
		}

		if(requested_role < 0)
		{
			LogError("[pdb] Failed to fetch role %s. No such role.", role.c_str());
			return false;
		}

		return !(current_role > 1 && requested_role <= current_role);
	}

	bool PdbUserCreate(mulex::PdbString username, mulex::PdbString password, mulex::PdbString role)
	{
		std::string current_user = GetCurrentCallerUser();

		// No user, we are on a backend (not on public API)
		// Allow to create any user role

		// Check if current_user can create role
		if(!current_user.empty() && !PdbCheckCurrentUserRolePermissionHierarchy(current_user, role.c_str()))
		{
			LogError("[pdb] User <%s> has no permission to create user with role <%s>.", current_user.c_str(), role.c_str());
			return false;
		}

		std::int32_t requested_role = PdbGetRoleId(role.c_str());

		const static std::vector<PdbValueType> types = {
			PdbValueType::NIL,
			PdbValueType::STRING,
			PdbValueType::STRING,
			PdbValueType::STRING,
			PdbValueType::INT32
		};

		std::string random_salt = SysGenerateSecureRandom256Hex();
		std::string cat_pass_salt = random_salt + password.c_str();
		std::vector<std::uint8_t> buffer(cat_pass_salt.size());
		std::memcpy(buffer.data(), cat_pass_salt.c_str(), cat_pass_salt.size());
		std::string hash = SysSHA256Hex(buffer);

		std::vector<std::uint8_t> data = SysPackArguments(
			username,
			PdbString(random_salt),
			PdbString(hash),
			requested_role
		);

		if(PdbWriteTable("INSERT INTO users (id, username, salt, passhash, role_id) VALUES (?, ?, ?, ?, ?);", types, data))
		{
			LogDebug("[pdb] Created new user <%s> with role <%s>.", username.c_str(), role.c_str());
			return true;
		}
		
		LogError("[pdb] Failed to create new user <%s>. Maybe username exists?", username.c_str());
		return false;
	}

	bool PdbUserDelete(mulex::PdbString username)
	{
		std::string current_user = GetCurrentCallerUser();
		std::string role = PdbGetUserRole(username.c_str());
		if(role.empty())
		{
			LogError("[pdb] Cannot delete user <%s>.", username.c_str());
			return false;
		}

		if(!current_user.empty() && !PdbCheckCurrentUserRolePermissionHierarchy(current_user, role.c_str()))
		{
			LogError("[pdb] User <%s> has no permission to delete user with role <%s>.", current_user.c_str(), role.c_str());
			return false;
		}

		// Username "admin" is reserved and cannot be deleted
		if(std::string(username.c_str()) == "admin")
		{
			LogError("[pdb] Cannot delete restricted user <admin>.");
			return false;
		}

		std::string query = std::string("DELETE FROM users WHERE username = '") + username.c_str() + "';";
		if(PdbExecuteQuery(query))
		{
			LogDebug("[pdb] Deleted user <%s>.", username.c_str());
			return true;
		}

		LogError("[pdb] Failed to delete user <%s>. Maybe user does not exist?", username.c_str());
		return false;
	}

	bool PdbUserChangePassword(mulex::PdbString oldpass, mulex::PdbString newpass)
	{
		std::string current_user = GetCurrentCallerUser();
		if(current_user.empty())
		{
			LogError("[pdb] Only a user can change their password.");
			return false;
		}

		const static std::vector<PdbValueType> types = {
			PdbValueType::STRING,
			PdbValueType::STRING
		};

		std::string query = "SELECT salt, passhash FROM users WHERE username = '" + current_user + "';";
		std::vector<std::uint8_t> data = PdbReadTable(query, types);
		
		if(data.empty())
		{
			LogError("[pdb] Failed to change password. No such user <%s>.", current_user.c_str());
			return false;
		}

		PdbString dbsalt = reinterpret_cast<const char*>(data.data());
		PdbString dbhash = reinterpret_cast<const char*>(data.data() + 512);

		// Calculate hash
		std::string cat_pass_salt = std::string(dbsalt.c_str()) + oldpass.c_str();
		std::vector<std::uint8_t> buffer(cat_pass_salt.size());
		std::memcpy(buffer.data(), cat_pass_salt.c_str(), cat_pass_salt.size());
		std::string hash = SysSHA256Hex(buffer);

		if(hash != dbhash.c_str())
		{
			LogError("[pdb] Failed to change password. Old password incorrect.");
			return false;
		}

		// Calculate new password hash
		cat_pass_salt = std::string(dbsalt.c_str()) + newpass.c_str();
		buffer.resize(cat_pass_salt.size());
		std::memcpy(buffer.data(), cat_pass_salt.c_str(), cat_pass_salt.size());
		hash = SysSHA256Hex(buffer);

		query = "UPDATE users SET passhash = '" + hash + "' WHERE username = '" + current_user + "';";
		if(!PdbExecuteQueryUnrestricted(query))
		{
			LogError("[pdb] Failed to update password for user <%s>.", current_user.c_str());
			return false;
		}

		LogDebug("[pdb] Updated password of user <%s>.", current_user.c_str());
		return true;
	}

	bool PdbUserChangeAvatar(mulex::FdbHandle handle)
	{
		return true;
	}
} // namespace mulex
