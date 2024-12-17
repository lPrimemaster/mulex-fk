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

	using RdbKeyName = mxstring<RDB_MAX_KEY_SIZE>;
	using PdbQuery = mxstring<PDB_MAX_TABLE_NAME_SIZE>;
	using PdbString = mxstring<PDB_MAX_STRING_SIZE>;

	struct RdbKey
	{
		RdbKeyName _name;
	};

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
		BOOL,
		NIL // Avoid clash with NULL
	};

	struct RdbValue
	{
		RdbValueType  _type;
		std::uint64_t _size;
		std::uint64_t _count;
		mutable std::uint8_t  _ptr[];

		template<typename T>
		constexpr inline T as() const
		{
			if constexpr(std::is_pointer_v<T>)
			{
				// Arrays
				if(_count == 0)
				{
					LogError("RdbValue::as() called with pointer type but its value is not an array.");
					return nullptr;
				}
				return reinterpret_cast<T>(_ptr);
			}
			else
			{
				// Non arrays
				if(_count > 0)
				{
					LogError("RdbValue::as() called with non pointer type but its value is an array.");
					return T();
				}
				return *reinterpret_cast<T*>(_ptr);
			}
		}
	};

	enum class RdbEntryFlag : std::uint64_t
	{
		EVENT_MOD_WATCHER = 0x01
	};

	std::uint64_t operator&  (RdbEntryFlag  a, RdbEntryFlag b);
	std::uint64_t operator&  (std::uint64_t a, RdbEntryFlag b);

	std::uint64_t operator|  (RdbEntryFlag  a, RdbEntryFlag b);
	std::uint64_t operator|  (std::uint64_t a, RdbEntryFlag b);

	std::uint64_t operator&= (std::uint64_t a, RdbEntryFlag b);
	std::uint64_t operator|= (std::uint64_t a, RdbEntryFlag b);

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

	std::string RdbMakeWatchEvent(const mulex::RdbKeyName& dir);
	void RdbTriggerEvent(std::uint64_t clientid, const RdbEntry& entry);

	// NOTE: (Cesar) This needs to be called after the Rdb
	void PdbInit();
	void PdbClose();
	std::uint64_t PdbTypeSize(const PdbValueType& type);

	MX_RPC_METHOD bool PdbExecuteQuery(mulex::PdbQuery query);
	MX_RPC_METHOD bool PdbWriteTable(mulex::PdbString table, mulex::RPCGenericType types, mulex::RPCGenericType data);
	MX_RPC_METHOD mulex::RPCGenericType PdbReadTable(mulex::PdbQuery query, mulex::RPCGenericType types);

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
		std::is_same_v<PdbString, std::decay_t<T>>;

	class PdbAccess
	{
	public:
		PdbAccess(const std::string& db = "") : _db(db) {  };

		template<PdbVariable... Vs>
		bool createTable(const std::string& table, const std::initializer_list<std::string>& spec)
		{
			const std::string query = generateSQLQueryCreate(table, spec);
			return executeQueryRemote(query);
		}

		template<PdbVariable... Vs>
		std::function<bool(const std::optional<Vs>&...)> getWriter(const std::string& table, const std::initializer_list<std::string>& names)
		{
			const std::string query = generateSQLQueryInsert(table, names);
			return [this, query](const std::optional<Vs>&... args) {
				std::vector<std::uint8_t> data;
				std::vector<PdbValueType> types;
				data.resize((sizeof(Vs) + ...));
				types.reserve(sizeof...(Vs));
				std::uint8_t* ptr = data.data();
				([&](){
					if(args.has_value())
					{
						std::memcpy(ptr, &args.value(), sizeof(Vs));
						ptr += sizeof(Vs);
						types.push_back(getValueType<Vs>());
					}
					else
					{
						types.push_back(PdbValueType::NIL);
					}
				}(), ...);

				LogTrace("%s", query.c_str());
				return executeInsertRemote(query, types, data);
			};
		}

		template<PdbVariable... Vs>
		std::function<std::vector<std::tuple<Vs...>>(const std::string&)> getReader(const std::string& table, const std::initializer_list<std::string>& names)
		{
			const std::string query = generateSQLQuerySelect(table, names);
			const std::vector<PdbValueType> types = { getValueType<Vs>()... };
			return [this, query, types](const std::string& conditions) -> std::vector<std::tuple<Vs...>> {
				return executeSelectRemote<Vs...>(query, conditions, types);
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
			if constexpr(std::is_same_v<T, bool>) return PdbValueType::BOOL;
			
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
			if constexpr(std::is_same_v<T, bool>) return PdbValueType::BOOL;
			
			LogError("[pdb] PdbGetValueType() FAILED.");
			return PdbValueType::NIL;
		}

		std::string generateSQLQueryCreate(const std::string& table, const std::initializer_list<std::string>& specs);
		std::string generateSQLQueryInsert(const std::string& table, const std::initializer_list<std::string>& names);
		std::string generateSQLQuerySelect(const std::string& table, const std::initializer_list<std::string>& names);
		bool executeQueryRemote(const std::string& query);
		bool executeInsertRemote(const std::string& query, const std::vector<PdbValueType>& types, const std::vector<uint8_t>& data);
		std::vector<std::uint8_t> executeSelectRemoteI(const std::string& query, const std::vector<PdbValueType>& types);

		template<typename T>
		T readBuffer(const std::vector<std::uint8_t>& buffer, std::uint64_t& offset)
		{
			T value;
			std::memcpy(&value, buffer.data() + offset, sizeof(T));
			offset += sizeof(T);
			return value;
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
			std::vector<std::uint8_t> data = executeSelectRemoteI(query + " " + conditions + ";", types);
			return bufferToTupleV<Vs...>(data);
		}
	private:
		std::string _db;
	};
}
