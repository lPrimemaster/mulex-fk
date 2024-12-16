#pragma once
#include "mxtypes.h"
#include "mxlogger.h"
#include <shared_mutex>
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
	using PdbValueType = RdbValueType;

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

	MX_RPC_METHOD void PdbWriteTableRow(mulex::PdbQuery table, mulex::RPCGenericType types, mulex::RPCGenericType data);

	template<std::uint64_t S>
	struct CTS
	{
		char data[S];
	};

	template<typename T, CTS N>
	struct PdbTableVariable
	{
		using Type = T;
		static constexpr CTS name = N;
	};

	template<typename> struct is_spec_PdbTableVariable : std::false_type {};
	template<typename T, CTS N> struct is_spec_PdbTableVariable<PdbTableVariable<T, N>> : std::true_type {};
	template<typename T> concept PdbTableVariableType = is_spec_PdbTableVariable<T>::value;

	struct PdbTableDetail {};

	template<PdbTableVariableType ...Ts>
	struct PdbTable : PdbTableDetail
	{
		using Types = std::tuple<typename Ts::Type...>; 
		inline static constexpr auto names = std::tuple<decltype(Ts::name)...>({Ts::name...});
	};

	template<typename T> concept PdbTableType = std::is_base_of_v<PdbTableDetail, T>;

	template<PdbTableType T>
	std::string PdbGenerateSQLQuery(std::string table)
	{
		auto columns = T::names; // Column names
		std::transform(table.begin(), table.end(), table.begin(), ::toupper); // Table name uppercase
		std::string query = "INSERT INTO " + table + " (";
		std::apply([&](auto... col) { ((query += std::string(col.data) + ", "), ...); }, columns); // Column names expansion
		query.pop_back(); query.pop_back(); // Remove last ", "
		query += ") VALUES (";
		std::apply([&](auto... col) { ((static_cast<void>(col), query += "?, "), ...); }, columns); // Column placeholder expansion
		query.pop_back(); query.pop_back(); // Remove last ", "
		query += ")";
		return query;
	}

	template<typename Tuple, typename Ret>
	struct tuple_func_type;

	template<typename Ret, typename... Ts>
	struct tuple_func_type<std::tuple<Ts...>, Ret>
	{
		using type = std::function<Ret(Ts...)>;
	};

	template<typename T>
	constexpr PdbValueType PdbGetValueType(const T&)
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

	template<typename... Args>
	std::pair<std::vector<PdbValueType>, std::vector<std::uint8_t>> PdbSerializeArguments(Args&&... args)
	{
		static constexpr std::uint64_t argsize = (sizeof(Args) + ...);
		std::vector<std::uint8_t> data(argsize);
		std::vector<PdbValueType> type(argsize);

		std::uint64_t offset = 0;
		auto pack = [&](const auto& val) {
			std::memcpy(data.data() + offset, &val, sizeof(val));
			type.push_back(PdbGetValueType(val));
			offset += sizeof(val);
		};

		(pack(args), ...);
		return { type, data };
	}

	template<PdbTableType T>
	typename tuple_func_type<typename T::Types, void>::type PdbTableWriter(const std::string& table)
	{
		return [table](auto&&... args) {
			const static std::string statement = PdbGenerateSQLQuery<T>(table);
			const auto [types, data] = PdbSerializeArguments(args...);
			PdbWriteTableRow(statement, types, data);
		};
	}
	
	inline void writeT()
	{
		auto f = PdbTableWriter<PdbTable<
			PdbTableVariable<int, {"id"}>,
			PdbTableVariable<std::string, {"name"}>,
			PdbTableVariable<std::string, {"post"}>
		>>("LogTable");

		f(1, "", "");
	}

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
}
