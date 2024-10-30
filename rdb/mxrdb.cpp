#include "../mxrdb.h"
#include "../mxlogger.h"
#include <cmath>
#include <cstdlib>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <fstream>
#include <rpcspec.inl>

static std::uint8_t* _rdb_handle = nullptr;
static std::uint64_t _rdb_offset = 0;
static std::uint64_t _rdb_size   = 0;

static std::shared_mutex 					   _rdb_rw_lock;
static std::map<std::string, mulex::RdbEntry*> _rdb_offset_map;

namespace mulex
{
	void RdbInit(std::uint64_t size)
	{
		// Make sure size is a multiple of 1024 and aligned
		if(size % 1024 != 0)
		{
			size = 1024 * ((size / 1024) + 1);
		}

#ifdef __unix__
		_rdb_handle = reinterpret_cast<std::uint8_t*>(std::aligned_alloc(1024, size));
#else
		_rdb_handle = reinterpret_cast<std::uint8_t*>(_aligned_malloc(size, 1024));
#endif
		_rdb_size = size;

		if(!_rdb_handle)
		{
			LogError("[rdb] Failed to create rdb.");
			LogError("[rdb] Allocation failed with size: %llu kb.", size / 1024);
			_rdb_size = 0;
			return;
		}

		LogDebug("[rdb] Init() OK. Allocated: %llu kb.", size / 1024);
	}

	void RdbClose()
	{
		std::unique_lock lock(_rdb_rw_lock);
		LogDebug("[rdb] Closing rdb.");
		if(_rdb_handle)
		{
			_rdb_size = 0;
			_rdb_offset = 0;
			_rdb_offset_map.clear();
#ifdef __unix__
			std::free(_rdb_handle);
#else
			_aligned_free(_rdb_handle);
#endif
		}
	}

	static std::uint64_t RdbCalculateDataSize(const RdbValue& value)
	{
		return value._count > 0 ? value._count * value._size : value._size;
	}

	static std::uint64_t RdbCalculateEntryTotalSize(const RdbEntry& entry)
	{
		return sizeof(RdbEntry) + RdbCalculateDataSize(entry._value);
	}

	static bool RdbCheckSizeAndGrowIfNeeded(std::uint64_t data_size)
	{
		std::uint64_t available_size = _rdb_size - _rdb_offset;

		if(available_size >= data_size + sizeof(RdbEntry))
		{
			return true;
		}
		
		// Need to grow the rdb linear memory
		// TODO: (Cesar) Implement this

		return false;
	}
	
	static std::uint64_t RdbTypeSize(const RdbValueType& type)
	{
		switch (type)
		{
			case RdbValueType::INT8: return sizeof(std::int8_t);
			case RdbValueType::INT16: return sizeof(std::int16_t);
			case RdbValueType::INT32: return sizeof(std::int32_t);
			case RdbValueType::INT64: return sizeof(std::int64_t);
			case RdbValueType::UINT8: return sizeof(std::uint8_t);
			case RdbValueType::UINT16: return sizeof(std::uint16_t);
			case RdbValueType::UINT32: return sizeof(std::uint32_t);
			case RdbValueType::UINT64: return sizeof(std::uint64_t);
			case RdbValueType::FLOAT32: return sizeof(float);
			case RdbValueType::FLOAT64: return sizeof(double);
			case RdbValueType::STRING: return RDB_MAX_STRING_SIZE; // HACK: For now store the max size
		}
		return 0;
	}

	static std::uint64_t RdbFindEmplaceOffset(const RdbKeyName& key)
	{
		auto lb = _rdb_offset_map.lower_bound(key.c_str());
		if(lb != _rdb_offset_map.cbegin())
		{
			// Emplace at the end of some entry
			RdbEntry* const entry = (--lb)->second;
			return (reinterpret_cast<std::uint8_t*>(entry) - _rdb_handle) + RdbCalculateEntryTotalSize(*entry);
		}
		else
		{
			// Emplace at the beginning (no left neighbour)
			return 0;
		}
	}

	static void RdbIncrementMapKeysAddrAfter(const RdbKeyName& key, std::uint64_t entry_size)
	{
		LogTrace("[rdb] Incrementing all keys after [%s]", key.c_str());
		for(auto it = _rdb_offset_map.upper_bound(key.c_str()); it != _rdb_offset_map.end(); it++)
		{
			it->second = reinterpret_cast<RdbEntry*>(reinterpret_cast<std::uint8_t*>(it->second) + entry_size);
		}
	}

	static void RdbDecrementMapKeysAddrAfter(const RdbKeyName& key, std::uint64_t entry_size)
	{
		LogTrace("[rdb] Decrementing all keys after [%s]", key.c_str());
		for(auto it = _rdb_offset_map.upper_bound(key.c_str()); it != _rdb_offset_map.end(); it++)
		{
			it->second = reinterpret_cast<RdbEntry*>(reinterpret_cast<std::uint8_t*>(it->second) - entry_size);
		}
	}

	RdbEntry* RdbNewEntry(const RdbKeyName& key, const RdbValueType& type, void* data, std::uint64_t count)
	{
		// NOTE: (Cesar) Needs to be before the lock
		if(RdbFindEntryByName(key))
		{
			LogError("[rdb] Cannot create already existing key");
			return nullptr;
		}

		// Lock database map and handle for creation
		// HACK: (Cesar) For simplicity this also
		//				 blocks W/R on the rdb
		//				 However this does not
		//				 invalidate RdbEntry pointers
		//				 Well it might due to realloc...
		std::unique_lock lock(_rdb_rw_lock);
		const std::uint64_t data_size = RdbTypeSize(type);
		const std::uint64_t data_total_size_bytes = count > 0 ? count * data_size : data_size;

		// Check if we have enough memory to add the new key
		// NOTE: (Cesar) This might invalidate the rdb handle
		if(!RdbCheckSizeAndGrowIfNeeded(data_total_size_bytes))
		{
			LogError("[rdb] Failed to allocate more space on rdb for new key");
			return nullptr;
		}

		// NOTE: (Cesar) _rdb_handle could have changed due to
		// 				 possible realloc above

		// Find emplace offset
		std::uint64_t eoff = RdbFindEmplaceOffset(key);
		std::uint8_t* ptr = _rdb_handle + eoff;

		// Move rdb to accommodate the new entry
		// Unless we are at the end block
		if(eoff < _rdb_offset)
		{
			std::uint64_t entry_total_size = sizeof(RdbEntry) + data_total_size_bytes;
			std::uint8_t* next = ptr + entry_total_size;
			std::memmove(next, ptr, _rdb_offset - eoff);
			RdbIncrementMapKeysAddrAfter(key, entry_total_size);
		}

		// Placement new and assign members
		RdbEntry* entry = new(ptr) RdbEntry();

		entry->_key._name = key;
		entry->_value._count = count;
		entry->_value._size = data_size;
		entry->_value._type = type;
		// NOTE: (Cesar) entry->_value._ptr is automatically populated ahead

		// Now copy the actual value data
		ptr += sizeof(RdbEntry);
		std::memcpy(ptr, data, data_total_size_bytes);

		// Finally put the key on the map
		_rdb_offset_map.emplace(key.c_str(), entry);

		_rdb_offset += sizeof(RdbEntry) + data_total_size_bytes;
		LogTrace("[rdb] Created new key: %s", key.c_str());
		return entry;
	}

	bool RdbDeleteEntry(const RdbKeyName& key)
	{
		RdbEntry* entry = RdbFindEntryByName(key);
		if(!entry)
		{
			LogError("[rdb] Cannot delete unknown key.");
			return false;
		}

		// Lock database map and handle for deletion
		// This also locks W/R access to any key
		std::unique_lock lock(_rdb_rw_lock);

		// If we are at the end just move the offset
		const std::uint64_t entry_total_size_bytes = RdbCalculateEntryTotalSize(*entry);
		if(_rdb_handle + _rdb_offset - entry_total_size_bytes ==
			reinterpret_cast<const std::uint8_t*>(entry)
		)
		{
			_rdb_offset -= entry_total_size_bytes;
		}
		else
		{
			// We are not at the end so will need to move the memory
			// This will however invalidate the memory
			const std::uint8_t* entry_end = reinterpret_cast<const std::uint8_t*>(entry) + entry_total_size_bytes;
			std::memmove(entry, entry_end, (_rdb_handle + _rdb_offset) - entry_end);
			_rdb_offset -= entry_total_size_bytes;

			// Left size of the key map is unchanged on the RDB since it is lexicographically sorted by key
			_rdb_offset_map.erase(key.c_str());
			RdbDecrementMapKeysAddrAfter(key, entry_total_size_bytes);
		}

		return true;
	}

	RdbEntry* RdbFindEntryByName(const RdbKeyName& key)
	{
		// Reading is ok to share the lock
		std::shared_lock lock(_rdb_rw_lock);

		auto it = _rdb_offset_map.find(key.c_str());
		if(it == _rdb_offset_map.end())
		{
			LogTrace("[rdb] Trying to access unknown rdb key: %s", key.c_str());
			return nullptr;
		}
		
		return it->second;
	}

	void RdbDumpMetadata(const std::string& filename)
	{
		std::ofstream output(filename);
		if(!output.is_open())
		{
			LogError("[rdb] Failed to open file <%s> for RDB metadata dump.", filename.c_str());
			return;
		}

		LogDebug("[rdb] Dumping RDB metadata to file <%s>.", filename.c_str());

		std::shared_lock lock(_rdb_rw_lock);
		for(const auto& it : _rdb_offset_map)
		{
	  		const RdbEntry* const entry = it.second;
			output << "Key: " << entry->_key._name.c_str() << std::endl;
			output << "\t" << "Value type: " << static_cast<int>(entry->_value._type) << std::endl;
			output << "\t" << "Value ptr: " << entry + sizeof(RdbEntry) << std::endl;
			output << "\t" << "Value size: " << entry->_value._size << std::endl;
			output << "\t" << "Value count: " << entry->_value._count << std::endl;
			output << "\t" << "Entry ptr: " << entry << std::endl;
			output << "\t" << "Entry size: " << RdbCalculateEntryTotalSize(*entry) << std::endl;
			output << std::endl;
		}
	}

	void RdbLockEntryRead(const RdbEntry& entry)
	{
		_rdb_rw_lock.lock_shared();
		entry._rw_lock.lock_shared();
	}

	void RdbLockEntryReadWrite(const RdbEntry& entry)
	{
		_rdb_rw_lock.lock_shared();
		entry._rw_lock.lock();
	}

	void RdbUnlockEntryRead(const RdbEntry& entry)
	{
		_rdb_rw_lock.unlock_shared();
		entry._rw_lock.unlock_shared();
	}

	void RdbUnlockEntryReadWrite(const RdbEntry& entry)
	{
		_rdb_rw_lock.unlock_shared();
		entry._rw_lock.unlock();
	}

	RPCGenericType RdbReadValueDirect(RdbKeyName keyname)
	{
		const RdbEntry* entry = RdbFindEntryByName(keyname);
		if(!entry)
		{
			return std::vector<std::uint8_t>();
		}

		RdbLockEntryRead(*entry);

		// Entry is read locked
		std::uint64_t size = RdbCalculateDataSize(entry->_value);
		std::vector<std::uint8_t> buffer(size);
		std::memcpy(buffer.data(), entry->_value._ptr, size);

		RdbUnlockEntryRead(*entry);

		return RPCGenericType::FromData(buffer);
	}

	void RdbWriteValueDirect(mulex::RdbKeyName keyname, RPCGenericType data)
	{
		RdbEntry* entry = RdbFindEntryByName(keyname);
		if(!entry)
		{
			return;
		}

		RdbLockEntryReadWrite(*entry);

		// Entry is read/write locked
		if(data._data.size() != RdbCalculateDataSize(entry->_value))
		{
			LogError("Cannot write rdb value. Data type or length differs.");
		}
		else
		{
			std::memcpy(entry->_value._ptr, data._data.data(), data._data.size());
		}

		RdbUnlockEntryReadWrite(*entry);
	}

	void RdbCreateValueDirect(mulex::RdbKeyName keyname, mulex::RdbValueType type, std::uint64_t count, mulex::RPCGenericType data)
	{
		RdbNewEntry(keyname, type, data._data.data(), count);
	}

	void RdbDeleteValueDirect(mulex::RdbKeyName keyname)
	{
		RdbDeleteEntry(keyname);
	}

	void RdbProxyValue::writeEntry()
	{
		std::optional<const Experiment*> exp = SysGetConnectedExperiment();
		if(exp.has_value())
		{
			exp.value()->_rpc_client->call(RPC_CALL_MULEX_RDBWRITEVALUEDIRECT, mulex::RdbKeyName(_key), _genvalue);
		}
	}

	void RdbProxyValue::readEntry()
	{
		std::optional<const Experiment*> exp = SysGetConnectedExperiment();
		if(exp.has_value())
		{
			_genvalue = exp.value()->_rpc_client->call<RPCGenericType, RdbKeyName>(RPC_CALL_MULEX_RDBREADVALUEDIRECT, RdbKeyName(_key));
		}
	}

	void RdbAccess::create(const std::string& key, RdbValueType type, RPCGenericType value, std::uint64_t count)
	{
		std::optional<const Experiment*> exp = SysGetConnectedExperiment();
		if(exp.has_value())
		{
			exp.value()->_rpc_client->call(RPC_CALL_MULEX_RDBCREATEVALUEDIRECT, RdbKeyName(key), type, count, value);
		}
	}

	void RdbAccess::erase(const std::string& key)
	{
		std::optional<const Experiment*> exp = SysGetConnectedExperiment();
		if(exp.has_value())
		{
			exp.value()->_rpc_client->call(RPC_CALL_MULEX_RDBDELETEVALUEDIRECT, RdbKeyName(key));
		}
	}
}
