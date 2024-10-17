#include "../mxsystem.h"
#include "../mxlogger.h"
#include <cmath>
#include <cstdlib>
#include <map>
#include <mutex>
#include <shared_mutex>

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

			_rdb_handle = reinterpret_cast<std::uint8_t*>(std::aligned_alloc(1024, size));
			_rdb_size = size;

			if(!_rdb_handle)
			{
				LogError("Failed to create rdb.");
				LogError("Allocation failed with size: %llu kb.", size / 1024);
				_rdb_size = 0;
			}
		}
	}

	void RdbClose()
	{
		if(_rdb_handle)
		{
			_rdb_size = 0;
			_rdb_offset = 0;
			_rdb_offset_map.clear();
			delete _rdb_handle;
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
	}

	RdbEntry* RdbNewEntry(const RdbKeyName& key, const RdbValueType& type, void* data, std::uint64_t count)
	{
		// NOTE: (Cesar) Needs to be before the lock
		if(RdbFindEntryByName(key))
		{
			LogError("Cannot create already existing key");
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
			LogError("Failed to allocate more space on rdb for new key.");
			return nullptr;
		}

		// NOTE: (Cesar) This needs to be here
		//		 		 _rdb_handle could have changed due to
		// 				 possible realloc above
		std::uint8_t* ptr = _rdb_handle + _rdb_offset;
		
		// Placement new and assign members
		RdbEntry* entry = new(ptr) RdbEntry();
		_rdb_offset += sizeof(RdbEntry);

		entry->_key._name = key;
		entry->_value._count = count;
		entry->_value._size = data_size;
		entry->_value._type = type;
		// NOTE: (Cesar) entry->_value._ptr is automatically populated ahead
	
		// TODO: (Cesar) This needs to be changed so
		// 				 that it places keys in alphabetical order

		// Now copy the actual value data
		ptr = _rdb_handle + _rdb_offset;
		std::memcpy(ptr, data, data_total_size_bytes);
		_rdb_offset += data_total_size_bytes;
		return entry;
	}

	bool RdbDeleteEntry(const RdbKeyName& key)
	{
		RdbEntry* entry = RdbFindEntryByName(key);
		if(!entry)
		{
			LogError("Cannot delete unknown key.");
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
			return true;
		}

		// We are not at the end so will need to move the memory
		// This will however invalidate the memory
		const std::uint8_t* entry_end = reinterpret_cast<const std::uint8_t*>(entry) + entry_total_size_bytes;
		std::memmove(entry, entry_end, (_rdb_handle + _rdb_offset) - entry_end);
		_rdb_offset -= entry_total_size_bytes;
		return true;
	}

	RdbEntry* RdbFindEntryByName(const RdbKeyName& key)
	{
		// Reading is ok to share the lock
		std::shared_lock lock(_rdb_rw_lock);

		auto it = _rdb_offset_map.find(key.c_str());
		if(it == _rdb_offset_map.end())
		{
			LogTrace("Trying to access unknown rdb key: %s", key.c_str());
			return nullptr;
		}
		
		return it->second;
	}

	std::vector<std::uint8_t> RdbReadValueDirect(RdbKeyName keyname)
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

		RdbLockEntryRead(*entry);

		return buffer;
	}

	void RdbWriteValueDirect(mulex::RdbKeyName keyname, std::vector<std::uint8_t> data)
	{
		RdbEntry* entry = RdbFindEntryByName(keyname);
		if(!entry)
		{
			return;
		}

		RdbLockEntryReadWrite(*entry);

		// Entry is read/write locked
		if(data.size() != RdbCalculateDataSize(entry->_value))
		{
			LogError("Cannot write rdb value. Data type or length differs.");
		}
		else
		{
			std::memcpy(entry->_value._ptr, data.data(), data.size());
		}

		RdbUnlockEntryReadWrite(*entry);
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


	RdbAccess::RdbAccess(const std::string& rootkey)
	{
		
	}
}
