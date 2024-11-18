#pragma once
#include "mxtypes.h"
#include "mxlogger.h"
#include <shared_mutex>
#include "network/rpc.h"

namespace mulex
{
	static constexpr std::uint64_t RDB_MAX_KEY_SIZE = 512;
	static constexpr std::uint64_t RDB_MAX_STRING_SIZE = 512;

	using RdbKeyName = mxstring<RDB_MAX_KEY_SIZE>;

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

		// Entry data
		RdbKey   _key;
		RdbValue _value; // NOTE: Must be last
	};

	void RdbInit(std::uint64_t size);
	void RdbClose();

	// TODO: (Cesar) Implement this
	bool RdbImportFromSQL(const std::string& filename);
	void RdbDumpMetadata(const std::string& filename);

	RdbEntry* RdbNewEntry(const RdbKeyName& key, const RdbValueType& type, const void* data, std::uint64_t count = 0);
	bool RdbDeleteEntry(const RdbKeyName& key);
	RdbEntry* RdbFindEntryByName(const RdbKeyName& key);

	MX_RPC_METHOD mulex::RPCGenericType RdbReadValueDirect(mulex::RdbKeyName keyname);
	MX_RPC_METHOD void RdbWriteValueDirect(mulex::RdbKeyName keyname, mulex::RPCGenericType data);
	MX_RPC_METHOD bool RdbCreateValueDirect(mulex::RdbKeyName keyname, mulex::RdbValueType type, std::uint64_t count, mulex::RPCGenericType data);
	MX_RPC_METHOD void RdbDeleteValueDirect(mulex::RdbKeyName keyname);
	MX_RPC_METHOD bool RdbValueExists(mulex::RdbKeyName keyname);
	MX_RPC_METHOD mulex::string32 RdbWatch(mulex::RdbKeyName dir);

	std::string RdbMakeWatchEvent(const mulex::RdbKeyName& dir);
	void RdbTriggerEvent(std::uint64_t clientid, const RdbEntry& entry);

	void RdbLockEntryRead(const RdbEntry& entry);
	void RdbLockEntryReadWrite(const RdbEntry& entry);
	void RdbUnlockEntryRead(const RdbEntry& entry);
	void RdbUnlockEntryReadWrite(const RdbEntry& entry);

	// NOTE: (Cesar) This needs to be called after the Rdb
	void PdbInit();
	void PdbClose();

	// TODO: (Cesar)
	template<typename Table>
	void PdbWriteTable(const Table& data);

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
