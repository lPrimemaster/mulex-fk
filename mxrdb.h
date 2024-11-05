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
		STRING
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

	struct RdbEntry
	{
		// Entry locking
		mutable std::shared_mutex _rw_lock;

		// Entry statistics
		std::int64_t _tcreated;
		std::int64_t _tmodified;

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
	MX_RPC_METHOD void RdbCreateValueDirect(mulex::RdbKeyName keyname, mulex::RdbValueType type, std::uint64_t count, mulex::RPCGenericType data);
	MX_RPC_METHOD void RdbDeleteValueDirect(mulex::RdbKeyName keyname);

	void RdbLockEntryRead(const RdbEntry& entry);
	void RdbLockEntryReadWrite(const RdbEntry& entry);
	void RdbUnlockEntryRead(const RdbEntry& entry);
	void RdbUnlockEntryReadWrite(const RdbEntry& entry);

	// NOTE: (Cesar) This needs to be called after the Rdb
	void PdbInit();
	void PdbClose();

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

		void create(const std::string& key, RdbValueType type, RPCGenericType value, std::uint64_t count = 0);
		void erase(const std::string& key);

	private:
		std::string _rootkey;
	};
}
