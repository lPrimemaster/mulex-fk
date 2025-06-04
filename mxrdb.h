#pragma once
#include "mxtypes.h"
#include "mxlogger.h"
#include <shared_mutex>
#include <tuple>
#include <utility>
#include "network/rpc.h"

namespace mulex
{
	static constexpr std::uint64_t RDB_MAX_KEY_SIZE = 512;
	static constexpr std::uint64_t RDB_MAX_STRING_SIZE = 512;
	static constexpr std::uint64_t PDB_MAX_TABLE_NAME_SIZE = 512;
	static constexpr std::uint64_t PDB_MAX_STRING_SIZE = 512;
	static constexpr std::uint64_t FDB_HANDLE_SIZE = 37;

	using RdbKeyName = mxstring<RDB_MAX_KEY_SIZE>;
	using PdbQuery = mxstring<PDB_MAX_TABLE_NAME_SIZE>;
	using PdbString = mxstring<PDB_MAX_STRING_SIZE>;
	using FdbHandle = mxstring<FDB_HANDLE_SIZE>;
	using FdbPath = PdbString;

	enum class RdbValueType : std::uint8_t
	{
		INT8,
		INT16,
		INT32,
		INT64,
		UINT8,
		UINT16,
		UINT32,
		UINT64,
		FLOAT32,
		FLOAT64,
		STRING,
		BOOL
	};

	// Because of the RPC layer this needs to have dynamic types
	enum class PdbValueType : std::uint8_t
	{
		INT8,
		INT16,
		INT32,
		INT64,
		UINT8,
		UINT16,
		UINT32,
		UINT64,
		FLOAT32,
		FLOAT64,
		STRING,
		CSTRING,
		BOOL,
		BINARY,
		NIL // Avoid clash with NULL
	};

	enum class RdbEntryFlag : std::uint64_t
	{
		EVENT_MOD_WATCHER = (1 << 0),
		HISTORY_ENABLED   = (1 << 1)
	};

	std::uint64_t operator&  (RdbEntryFlag  a, RdbEntryFlag b);
	std::uint64_t operator&  (std::uint64_t a, RdbEntryFlag b);

	std::uint64_t operator|  (RdbEntryFlag  a, RdbEntryFlag b);
	std::uint64_t operator|  (std::uint64_t a, RdbEntryFlag b);

	std::uint64_t operator&= (std::uint64_t a, RdbEntryFlag b);
	std::uint64_t operator|= (std::uint64_t a, RdbEntryFlag b);

	std::uint64_t operator~  (RdbEntryFlag  a);

	struct RdbEntry
	{
		// Entry locking
		mutable std::shared_mutex _rw_lock;

		// Entry statistics
		std::int64_t _tcreated;
		std::int64_t _tmodified;

		// Entry metadata
		std::uint64_t _flags;
		RdbValueType  _type;
		std::uint64_t _size;
		std::uint64_t _count;

		// Entry data
		// std::uint8_t _padding[16];
		mutable std::uint8_t _ptr[];

		template<typename T>
		constexpr inline T as() const
		{
			if constexpr(std::is_pointer_v<T>)
			{
				// Arrays
				if(_count == 0)
				{
					LogError("RdbEntry::as() called with pointer type but its value is not an array.");
					return nullptr;
				}
				return reinterpret_cast<T>(_ptr);
			}
			else
			{
				// Non arrays
				if(_count > 0)
				{
					LogError("RdbEntry::as() called with non pointer type but its value is an array.");
					return T();
				}
				return *reinterpret_cast<T*>(_ptr);
			}
		}
	};

	void RdbInit(std::uint64_t size);
	void RdbClose();

	void RdbInitHistoryBuffer();
	void RdbCloseHistoryBuffer();
	void RdbHistoryFlush();
	void RdbHistoryFlushUnlocked();
	void RdbHistoryAdd(RdbEntry* entry, const RdbKeyName& key);

	RdbEntry* RdbAllocate(std::uint64_t size);
	void RdbFree(RdbEntry* entry);

	// TODO: (Cesar) Implement this
	bool RdbImportFromSQL(const std::string& filename);
	void RdbDumpMetadata(const std::string& filename);

	RdbEntry* RdbNewEntry(const RdbKeyName& key, const RdbValueType& type, const void* data, std::uint64_t count = 0);
	bool RdbDeleteEntry(const RdbKeyName& key);
	RdbEntry* RdbFindEntryByName(const RdbKeyName& key);
	RdbEntry* RdbFindEntryByNameUnlocked(const RdbKeyName& key);

	MX_RPC_METHOD mulex::RPCGenericType RdbReadValueDirect(mulex::RdbKeyName keyname);
	MX_RPC_METHOD void RdbWriteValueDirect(mulex::RdbKeyName keyname, mulex::RPCGenericType data);
	MX_RPC_METHOD bool RdbCreateValueDirect(mulex::RdbKeyName keyname, mulex::RdbValueType type, std::uint64_t count, mulex::RPCGenericType data);
	MX_RPC_METHOD void RdbDeleteValueDirect(mulex::RdbKeyName keyname);
	MX_RPC_METHOD bool RdbValueExists(mulex::RdbKeyName keyname);
	MX_RPC_METHOD mulex::RPCGenericType RdbReadKeyMetadata(mulex::RdbKeyName keyname);
	MX_RPC_METHOD mulex::string32 RdbWatch(mulex::RdbKeyName dir);
	MX_RPC_METHOD mulex::string32 RdbUnwatch(mulex::RdbKeyName dir);
	MX_RPC_METHOD mulex::RPCGenericType RdbListKeys();
	MX_RPC_METHOD mulex::RPCGenericType RdbListKeyTypes();
	MX_RPC_METHOD mulex::RPCGenericType RdbListSubkeys(mulex::RdbKeyName dir);
	MX_RPC_METHOD unsigned char RdbGetKeyType(mulex::RdbKeyName key);
	MX_RPC_METHOD bool RdbToggleHistory(mulex::RdbKeyName keyname, bool active);
	MX_RPC_METHOD mulex::RPCGenericType RdbGetHistory(mulex::RdbKeyName keyname, std::uint64_t count);

	std::string RdbMakeWatchEvent(const mulex::RdbKeyName& dir);
	void RdbTriggerEvent(std::uint64_t clientid, const RdbEntry& entry);

	// NOTE: (Cesar) This needs to be called after the Rdb
	void PdbInit();
	void PdbClose();
	std::uint64_t PdbTypeSize(const PdbValueType& type);
	void PdbPushBufferBytes(const std::uint8_t* value, std::uint64_t size, std::vector<std::uint8_t>& buffer);

	std::string PdbGenerateSQLQueryCreate(const std::string& table, const std::initializer_list<std::string>& specs);
	std::string PdbGenerateSQLQueryInsert(const std::string& table, const std::initializer_list<std::string>& names);
	std::string PdbGenerateSQLQuerySelect(const std::string& table, const std::initializer_list<std::string>& names);

	bool PdbTableExists(const std::string& table);
	void PdbSetupUserDatabase();
	std::string PdbGetUserRole(const std::string& username);
	std::int32_t PdbGetUserRoleId(const std::string& username);
	std::int32_t PdbGetRoleId(const std::string& role);
	std::int32_t PdbGetUserId(const std::string& username);
	bool PdbExecuteQueryUnrestricted(const std::string& query); // For very large local queries

	MX_RPC_METHOD bool PdbExecuteQuery(mulex::PdbQuery query);
	MX_RPC_METHOD bool PdbWriteTable(mulex::PdbString table, mulex::RPCGenericType types, mulex::RPCGenericType data);
	MX_RPC_METHOD mulex::RPCGenericType PdbReadTable(mulex::PdbQuery query, mulex::RPCGenericType types);

	// RPCs for user database
	MX_RPC_METHOD MX_PERMISSION("create_user") bool PdbUserCreate(mulex::PdbString username, mulex::PdbString password, mulex::PdbString role);
	MX_RPC_METHOD MX_PERMISSION("delete_user") bool PdbUserDelete(mulex::PdbString username);
	MX_RPC_METHOD 							   bool PdbUserChangePassword(mulex::PdbString oldpass, mulex::PdbString newpass);
	MX_RPC_METHOD 							   bool PdbUserChangeAvatar(mulex::PdbString handle);
	MX_RPC_METHOD mulex::FdbPath PdbUserGetAvatarPath();

	void FdbInit();
	void FdbClose();

	FdbPath FdbGetFilePath(const FdbHandle& handle);
	bool FdbCheckHandle(const FdbHandle& handle);

	// Chunked upload is preferred for big files (>10MB)
	// This avoids the need to have huge data buffers allocated
	MX_RPC_METHOD MX_PERMISSION("upload_files") mulex::FdbHandle FdbChunkedUploadStart(mulex::string32 mimetype);
	MX_RPC_METHOD MX_PERMISSION("upload_files") bool FdbChunkedUploadSend(mulex::PdbString handle, mulex::RPCGenericType chunk);
	MX_RPC_METHOD MX_PERMISSION("upload_files") bool FdbChunkedUploadEnd(mulex::PdbString handle);

	// NOTE: (Cesar) FdbUploadFile is limited by the SysRecvThread buffer size (~10MB filesize)
	MX_RPC_METHOD MX_PERMISSION("upload_files") mulex::FdbHandle FdbUploadFile(mulex::RPCGenericType data, mulex::string32 mimetype);
	MX_RPC_METHOD MX_PERMISSION("delete_files") bool FdbDeleteFile(mulex::PdbString handle);
	MX_RPC_METHOD mulex::FdbPath FdbGetHandleRelativePath(mulex::PdbString handle);

	// NOTE: (Cesar) Limited to 128 permissions
	//				 Enlarge if required
	//				 Benchmarked and is faster than std::bitset<128>
	//				 with random access
	//				 At least on "my system" eh
	class PdbPermissions
	{
	public:
		constexpr PdbPermissions(std::uint64_t hi, std::uint64_t lo) : lo(lo), hi(hi) {  }
		PdbPermissions() = default;

		constexpr inline bool test(std::uint64_t hi, std::uint64_t lo) const
		{
			return (hi & this->hi) == hi && (lo & this->lo) == lo;
		}

		inline void set(int i)
		{
			if(i < 64) lo |= (1ULL << i);
			else hi |= (1ULL << (i - 64));
		}

		inline bool test(int i) const
		{
			return i < 64 ? (lo & (1ULL << i)) : (hi & (1ULL << (i - 64)));
		}

	private:
		std::uint64_t lo = 0;
		std::uint64_t hi = 0;
	};

	class RdbProxyValue
	{
	public:
		RdbProxyValue(const std::string& key) : _key(key) {  }

		template<typename T>
		RdbProxyValue operator=(const T& t)
		{
			_genvalue = t;
			// Send t to the rdb
			writeEntry();
			return *this;
		}

		template<typename T>
		T* asPointer()
		{
			// Read to from the rdb
			readEntry();
			return _genvalue.asPointer<T>();
		}

		template<typename T>
		operator T()
		{
			// Read t from the rdb
			readEntry();
			return _genvalue.asType<T>();
		}

		template<typename T>
		operator std::vector<T>()
		{
			// Read t from the rdb
			readEntry();
			return _genvalue.asVectorType<T>();
		}

		inline void flush()
		{
			writeEntry();
		}

		bool exists();
		bool create(RdbValueType type, RPCGenericType value, std::uint64_t count = 0);
		bool erase();
		void watch(std::function<void(const RdbKeyName& key, const RPCGenericType& value)> callback);
		void unwatch();
		bool history(bool status);

	private:
		void writeEntry();
		void readEntry();
		
	private:
		RPCGenericType _genvalue;
		std::string _key;
	};

	class RdbAccess
	{
	public:
		RdbAccess(const std::string& rootkey = "") : _rootkey(rootkey) {  }

		inline RdbProxyValue operator[](const std::string& key)
		{
			return RdbProxyValue(_rootkey + key);
		}

	private:
		std::string _rootkey;
	};

	template<typename T> concept PdbVariable = 
		std::is_same_v<std::uint8_t , std::decay_t<T>> ||
		std::is_same_v<std::uint16_t, std::decay_t<T>> ||
		std::is_same_v<std::uint32_t, std::decay_t<T>> ||
		std::is_same_v<std::uint64_t, std::decay_t<T>> ||
		std::is_same_v<std::int8_t , std::decay_t<T>> ||
		std::is_same_v<std::int16_t, std::decay_t<T>> ||
		std::is_same_v<std::int32_t, std::decay_t<T>> ||
		std::is_same_v<std::int64_t, std::decay_t<T>> ||
		std::is_same_v<float , std::decay_t<T>> ||
		std::is_same_v<double, std::decay_t<T>> ||
		std::is_same_v<bool  , std::decay_t<T>> ||
		std::is_same_v<PdbString, std::decay_t<T>> ||
		std::is_same_v<std::string, std::decay_t<T>> ||
		std::is_same_v<std::vector<std::uint8_t>, std::decay_t<T>>;

	template<typename Policy>
	class PdbAccess
	{
	public:
		PdbAccess(const std::string& db = "") : _db(db) {  };

		bool createTable(const std::string& table, const std::initializer_list<std::string>& spec)
		{
			const std::string query = PdbGenerateSQLQueryCreate(table, spec);
			return Policy::executeQueryRemote(query);
			// return executeQueryRemote(query);
		}

		template<PdbVariable... Vs>
		std::function<bool(const std::optional<Vs>&...)> getWriter(const std::string& table, const std::initializer_list<std::string>& names)
		{
			const std::string query = PdbGenerateSQLQueryInsert(table, names);
			return [this, query](const std::optional<Vs>&... args) {
				std::vector<std::uint8_t> data;
				std::vector<PdbValueType> types;
				data.reserve((sizeof(Vs) + ...));
				types.reserve(sizeof...(Vs));
				([&](){
					if(args.has_value())
					{
						if constexpr(std::is_same_v<Vs, std::vector<std::uint8_t>>)
						{
							std::uint64_t size = args.value().size();
							PdbPushBufferBytes(reinterpret_cast<const std::uint8_t*>(&size), sizeof(std::uint64_t), data);
							PdbPushBufferBytes(reinterpret_cast<const std::uint8_t*>(args.value().data()), args.value().size(), data);
						}
						else if constexpr(std::is_same_v<Vs, std::string>)
						{
							const std::string& value = args.value();
							std::uint64_t size = value.size();
							PdbPushBufferBytes(reinterpret_cast<const std::uint8_t*>(value.c_str()), size + 1, data);
						}
						else
						{
							PdbPushBufferBytes(reinterpret_cast<const std::uint8_t*>(&args.value()), sizeof(Vs), data);
						}
						types.push_back(getValueType<Vs>());
					}
					else
					{
						types.push_back(PdbValueType::NIL);
					}
				}(), ...);

				return Policy::executeInsertRemote(query, types, data);
			};
		}

		template<PdbVariable... Vs>
		std::function<std::vector<std::tuple<Vs...>>(const std::string&)> getReader(const std::string& table, const std::initializer_list<std::string>& names)
		{
			const std::string query = PdbGenerateSQLQuerySelect(table, names);
			const std::vector<PdbValueType> types = { getValueType<Vs>()... };
			return [this, query, types](const std::string& conditions) -> std::vector<std::tuple<Vs...>> {
				return executeSelectRemote<Vs...>(query, conditions, types);
			};
		}

		template<PdbVariable... Vs>
		std::function<std::vector<std::uint8_t>(const std::string&)> getReaderRaw(const std::string& table, const std::initializer_list<std::string>& names)
		{
			const std::string query = PdbGenerateSQLQuerySelect(table, names);
			const std::vector<PdbValueType> types = { getValueType<Vs>()... };
			return [this, query, types](const std::string& conditions) -> std::vector<std::uint8_t> {
				return Policy::executeSelectRemoteI(query + " " + conditions + ";", types);
			};
		}

	private:
		template<typename T>
		constexpr PdbValueType getValueType(const T&)
		{
			if constexpr(std::is_same_v<T, std::int8_t>) return PdbValueType::INT8;
			if constexpr(std::is_same_v<T, std::uint8_t>) return PdbValueType::UINT8;
			if constexpr(std::is_same_v<T, std::int16_t>) return PdbValueType::INT16;
			if constexpr(std::is_same_v<T, std::uint16_t>) return PdbValueType::UINT16;
			if constexpr(std::is_same_v<T, std::int32_t>) return PdbValueType::INT32;
			if constexpr(std::is_same_v<T, std::uint32_t>) return PdbValueType::UINT32;
			if constexpr(std::is_same_v<T, std::int64_t>) return PdbValueType::INT64;
			if constexpr(std::is_same_v<T, std::uint64_t>) return PdbValueType::UINT64;
			if constexpr(std::is_same_v<T, float>) return PdbValueType::FLOAT32;
			if constexpr(std::is_same_v<T, double>) return PdbValueType::FLOAT64;
			if constexpr(std::is_same_v<T, PdbString>) return PdbValueType::STRING;
			if constexpr(std::is_same_v<T, std::string>) return PdbValueType::CSTRING;
			if constexpr(std::is_same_v<T, bool>) return PdbValueType::BOOL;
			if constexpr(std::is_same_v<T, std::vector<std::uint8_t>>) return PdbValueType::BINARY;
			
			LogError("[pdb] PdbGetValueType() FAILED.");
			return PdbValueType::INT8;
		}

		template<typename T>
		constexpr PdbValueType getValueType()
		{
			if constexpr(std::is_same_v<T, std::int8_t>) return PdbValueType::INT8;
			if constexpr(std::is_same_v<T, std::uint8_t>) return PdbValueType::UINT8;
			if constexpr(std::is_same_v<T, std::int16_t>) return PdbValueType::INT16;
			if constexpr(std::is_same_v<T, std::uint16_t>) return PdbValueType::UINT16;
			if constexpr(std::is_same_v<T, std::int32_t>) return PdbValueType::INT32;
			if constexpr(std::is_same_v<T, std::uint32_t>) return PdbValueType::UINT32;
			if constexpr(std::is_same_v<T, std::int64_t>) return PdbValueType::INT64;
			if constexpr(std::is_same_v<T, std::uint64_t>) return PdbValueType::UINT64;
			if constexpr(std::is_same_v<T, float>) return PdbValueType::FLOAT32;
			if constexpr(std::is_same_v<T, double>) return PdbValueType::FLOAT64;
			if constexpr(std::is_same_v<T, PdbString>) return PdbValueType::STRING;
			if constexpr(std::is_same_v<T, std::string>) return PdbValueType::CSTRING;
			if constexpr(std::is_same_v<T, bool>) return PdbValueType::BOOL;
			if constexpr(std::is_same_v<T, std::vector<std::uint8_t>>) return PdbValueType::BINARY;
			
			LogError("[pdb] PdbGetValueType() FAILED.");
			return PdbValueType::NIL;
		}

		template<typename T>
		T readBuffer(const std::vector<std::uint8_t>& buffer, std::uint64_t& offset)
		{
			if constexpr(std::is_same_v<T, std::vector<std::uint8_t>>)
			{
				std::int32_t size = *reinterpret_cast<const std::int32_t*>(buffer.data() + offset);
				std::vector<std::uint8_t> value(size);
				std::memcpy(value.data(), buffer.data() + offset + sizeof(std::int32_t), size);
				offset += (sizeof(std::int32_t) + size);
				return value;
			}
			else if constexpr(std::is_same_v<T, std::string>)
			{
				const char* cstr = reinterpret_cast<const char*>(buffer.data() + offset);
				const std::uint64_t size = std::strlen(cstr);
				std::string value;
				value.resize(size);
				std::memcpy(value.data(), cstr, size);
				offset += (size + 1);
				return value;
			}
			else
			{
				T value;
				std::memcpy(&value, buffer.data() + offset, sizeof(T));
				offset += sizeof(T);
				return value;
			}
		}

		template<typename T, std::uint64_t... Ti>
		std::vector<T> bufferToTupleVI(const std::vector<std::uint8_t>& buffer, std::index_sequence<Ti...>)
		{
			std::uint64_t offset = 0;
			std::vector<T> output;
			while(offset < buffer.size())
			{
				output.push_back(T{readBuffer<std::tuple_element_t<Ti, T>>(buffer, offset)...});
			}
			return output;
		}

		template<typename... Ts>
		std::vector<std::tuple<Ts...>> bufferToTupleV(const std::vector<std::uint8_t>& buffer)
		{
			return bufferToTupleVI<std::tuple<Ts...>>(buffer, std::index_sequence_for<Ts...>{});
		}

		template<PdbVariable... Vs>
		std::vector<std::tuple<Vs...>> executeSelectRemote(const std::string& query, const std::string& conditions, const std::vector<PdbValueType>& types)
		{
			std::vector<std::uint8_t> data = Policy::executeSelectRemoteI(query + " " + conditions + ";", types);
			return bufferToTupleV<Vs...>(data);
		}
	private:
		std::string _db;
	};

	class PdbAccessPolicyRemote
	{
	public:
		static bool executeQueryRemote(const std::string& query);
		static bool executeInsertRemote(const std::string& query, const std::vector<PdbValueType>& types, const std::vector<uint8_t>& data);
		static std::vector<std::uint8_t> executeSelectRemoteI(const std::string& query, const std::vector<PdbValueType>& types);
	};

	class PdbAccessPolicyLocal
	{
	public:
		static bool executeQueryRemote(const std::string& query);
		static bool executeInsertRemote(const std::string& query, const std::vector<PdbValueType>& types, const std::vector<uint8_t>& data);
		static std::vector<std::uint8_t> executeSelectRemoteI(const std::string& query, const std::vector<PdbValueType>& types);
	};

	using PdbAccessRemote = PdbAccess<PdbAccessPolicyRemote>;
	using PdbAccessLocal  = PdbAccess<PdbAccessPolicyLocal>;
}
