#include "../mxrdb.h"
#include "../mxlogger.h"
#include "../mxevt.h"
#include <cmath>
#include <cstdlib>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <set>
#include <rpcspec.inl>

static std::uint8_t* _rdb_handle = nullptr;
static std::uint64_t _rdb_offset = 0;
static std::uint64_t _rdb_size   = 0;

static std::shared_mutex 					   _rdb_rw_lock;
static std::map<std::string, mulex::RdbEntry*> _rdb_offset_map;

static std::set<std::string> _rdb_watch_dirs;
static std::shared_mutex     _rdb_watch_lock;

namespace mulex
{
	std::uint64_t operator& (std::uint64_t a, RdbEntryFlag b)
	{
		return a & static_cast<std::uint64_t>(b);
	}

	std::uint64_t operator& (RdbEntryFlag a, RdbEntryFlag b)
	{
		return static_cast<std::uint64_t>(a) & static_cast<std::uint64_t>(b);
	}

	std::uint64_t operator| (RdbEntryFlag a, RdbEntryFlag b)
	{
		return static_cast<std::uint64_t>(a) | static_cast<std::uint64_t>(b);
	}

	std::uint64_t operator| (std::uint64_t a, RdbEntryFlag b)
	{
		return a | static_cast<std::uint64_t>(b);
	}

	std::uint64_t operator&= (std::uint64_t a, RdbEntryFlag b)
	{
		a = a & b;
		return a;
	}

	std::uint64_t operator|= (std::uint64_t a, RdbEntryFlag b)
	{
		a = a | b;
		return a;
	}

	static std::uint64_t FindString(const char* data, std::uint64_t idx)
	{
		while(data[idx] != 0) idx++;
		return idx + 1;
	}

	static void RdbLoadOffsetMap(const char* data, std::uint64_t size)
	{
		std::uint64_t idx = 0;
		while(idx < size)
		{
			std::uint64_t nidx = FindString(data, idx);
			std::uint64_t offset = *reinterpret_cast<const std::uint64_t*>(data + nidx);
			LogTrace("[rdb] Loading entry: %s <%llu>", data + idx, offset);
			_rdb_offset_map.emplace(std::string(data + idx), reinterpret_cast<RdbEntry*>(_rdb_handle + offset));
			idx = nidx + sizeof(std::uint64_t);
		}
	}

	static std::vector<std::uint8_t> RdbWriteOffsetMap()
	{
		std::vector<std::uint8_t> data;
		std::uint64_t offset = 0;
		for(const auto& it : _rdb_offset_map)
		{
			std::uint64_t ssize = it.first.size() + 1;
			data.resize(data.size() + ssize + sizeof(std::uint64_t));
			std::uint8_t* ptr = data.data() + offset;
			std::memcpy(ptr, it.first.c_str(), ssize);

			std::uint64_t entry_offset = reinterpret_cast<std::uint8_t*>(it.second) - _rdb_handle;
			std::memcpy(ptr + ssize, &entry_offset, sizeof(std::uint64_t));
			offset += (ssize + sizeof(std::uint64_t));
		}
		return data;
	}

	static std::uint8_t* RdbAlignedAlloc(std::uint64_t align, std::uint64_t size)
	{
#ifdef __unix__
		return reinterpret_cast<std::uint8_t*>(std::aligned_alloc(align, size));
#else
		return reinterpret_cast<std::uint8_t*>(_aligned_malloc(size, align));
#endif
	}

	static void RdbAlignedFree(std::uint8_t* ptr)
	{
#ifdef __unix__
		std::free(ptr);
#else
		_aligned_free(ptr);
#endif
	}

	static void RdbLoadFromFile(const std::string& filename)
	{
		// Load the existing rdb data
		std::vector<std::uint8_t> data = SysReadBinFile(filename);

		// Extract map size from the raw data
		std::uint64_t mapsize = *reinterpret_cast<std::uint64_t*>(data.data());
		LogTrace("[rdb] Load mapsize %llu", mapsize);

		// Extract rdb size from the raw data
		std::uint64_t rdbsize = *reinterpret_cast<std::uint64_t*>(data.data() + sizeof(std::uint64_t));
		std::uint64_t rdbsize_unaligned = rdbsize;
		LogTrace("[rdb] Load rdb true size %llu", rdbsize);

		if(rdbsize == 0)
		{
			_rdb_size = 0;
			_rdb_offset = 0;
			return;
		}

		// Make sure size is a multiple of 1024 and aligned
		if(rdbsize % 1024 != 0)
		{
			rdbsize = 1024 * ((rdbsize / 1024) + 1);
			LogTrace("[rdb] Load rdb aligned size %llu", rdbsize);
		}

		_rdb_handle = RdbAlignedAlloc(1024, rdbsize);
		_rdb_size = rdbsize;
		_rdb_offset = rdbsize_unaligned;

		// Copy the data and set the map offsets
		RdbLoadOffsetMap(reinterpret_cast<char*>(data.data() + 2 * sizeof(std::uint64_t)), mapsize);

		std::memcpy(_rdb_handle, data.data() + mapsize + 2 * sizeof(std::uint64_t), rdbsize_unaligned);
	}

	static void RdbSaveToFile(const std::string& filename)
	{
		std::vector<std::uint8_t> data_offset = RdbWriteOffsetMap();
		std::uint64_t mapsize = data_offset.size();
		std::vector<std::uint8_t> output;
		output.resize(mapsize + _rdb_offset + 2 * sizeof(std::uint64_t));

		std::memcpy(output.data(), &mapsize, sizeof(std::uint64_t));
		std::memcpy(output.data() + sizeof(std::uint64_t), &_rdb_offset, sizeof(std::uint64_t));
		std::memcpy(output.data() + 2 * sizeof(std::uint64_t), data_offset.data(), mapsize);
		std::memcpy(output.data() + mapsize + 2 * sizeof(std::uint64_t), _rdb_handle, _rdb_offset);

		SysWriteBinFile(filename, output);
	}

	void RdbInit(std::uint64_t size)
	{
		std::unique_lock lock(_rdb_rw_lock);

		// Make sure size is a multiple of 1024 and aligned
		if(size % 1024 != 0)
		{
			size = 1024 * ((size / 1024) + 1);
		}

		std::string exphome = SysGetExperimentHome();
		if(!std::filesystem::is_regular_file(exphome + "/rdb.bin"))
		{
			LogWarning("[rdb] Could not find rdb cache under experiment home dir.");
			LogWarning("[rdb] Initializing empty rdb.");

			_rdb_handle = RdbAlignedAlloc(1024, size);
			_rdb_size = size;
		}
		else
		{
			RdbLoadFromFile(exphome + "/rdb.bin");
			if(_rdb_size == 0)
			{
				LogTrace("[rdb] File <rdb.bin> is empty. Allocating default space for rdb.");
				_rdb_handle = RdbAlignedAlloc(1024, size);
				_rdb_size = size;
			}
		}


		if(!_rdb_handle)
		{
			LogError("[rdb] Failed to create rdb.");
			LogError("[rdb] Allocation failed with size: %llu kb.", _rdb_size / 1024);
			_rdb_size = 0;
			return;
		}

		LogDebug("[rdb] Init() OK. Allocated: %llu kb.", _rdb_size / 1024);
	}

	void RdbClose()
	{
		std::unique_lock lock(_rdb_rw_lock);
		LogDebug("[rdb] Closing rdb.");

		if(_rdb_handle)
		{
			std::string exphome = SysGetExperimentHome();
			if(!exphome.empty())
			{
				LogDebug("[rdb] Found experiment home. Saving rdb data.");
				RdbSaveToFile(exphome + "/rdb.bin");
			}

			_rdb_size = 0;
			_rdb_offset = 0;
			_rdb_offset_map.clear();

			RdbAlignedFree(_rdb_handle);
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
		// Realloc won't align, so we copy the memory
		// RDB Already locked at this point
		std::uint8_t* temp_handle = RdbAlignedAlloc(1024, _rdb_size * 2);
		
		if(!temp_handle)
		{
			LogError("[rdb] Failed to allocate more space. Current <%llu> kB. Tried <%llu> kB.", _rdb_size / 1024, (_rdb_size * 2) / 1024);
			return false;
		}

		_rdb_size *= 2;
		
		std::memcpy(temp_handle, _rdb_handle, _rdb_offset);

		// Update the offsets based on the new handle
		for(auto it = _rdb_offset_map.begin(); it != _rdb_offset_map.end(); it++)
		{
			const std::uint64_t offset = (reinterpret_cast<std::uint8_t*>(it->second) - _rdb_handle);
			it->second = reinterpret_cast<RdbEntry*>(temp_handle + offset);
		}

		RdbAlignedFree(_rdb_handle);
		_rdb_handle = temp_handle;

		LogDebug("[rdb] RdbCheckSizeAndGrowIfNeeded() OK.");
		LogDebug("[rdb] New size <%llu> kB.", _rdb_size / 1024);

		return true;
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
			case RdbValueType::BOOL: return sizeof(bool);
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

	static void RdbEmitEvtCondition(const std::string& swatch, const RdbKeyName& key, const RdbEntry* entry)
	{
		std::string event_name = RdbMakeWatchEvent(swatch);
		std::vector<std::uint8_t> evt_buffer;
		std::uint64_t entry_data_size = RdbCalculateDataSize(entry->_value);
		evt_buffer.resize(sizeof(RdbKeyName) + sizeof(std::uint64_t) + entry_data_size);
		std::uint64_t offset = EvtDataAppend(0, &evt_buffer, key);
		offset = EvtDataAppend(offset, &evt_buffer, entry_data_size);
		offset = EvtDataAppend(offset, &evt_buffer, entry->_value._ptr, entry_data_size);
		LogTrace("Emit watch event <%s> from swatch: <%s>.", event_name.c_str(), swatch.c_str());
		EvtEmit(event_name, evt_buffer.data(), evt_buffer.size());
	}

	// TODO: (Cesar) Move this code to on creation and add a flag for scanning
	//				 That would make this a lot faster
	static void RdbEmitWatchMatchCondition(const RdbKeyName& key, const RdbEntry* entry)
	{
		std::shared_lock<std::shared_mutex> lock_watch(_rdb_watch_lock);
		for(const auto& watch : _rdb_watch_dirs)
		{
			if(SysMatchPattern(watch, key.c_str()))
			{
				RdbEmitEvtCondition(watch, key, entry);
				LogTrace("Match: <%s> -> <%s>.", watch.c_str(), key.c_str());
			}
		}
	}

	RdbEntry* RdbNewEntry(const RdbKeyName& key, const RdbValueType& type, const void* data, std::uint64_t count)
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
			LogError("[rdb] Failed to allocate more space on rdb for new key.");
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

		// Setup entry related statistics
		entry->_tcreated = SysGetCurrentTime();
		entry->_tmodified = entry->_tcreated;

		entry->_key._name = key;
		entry->_value._count = count;
		entry->_value._size = data_size;
		entry->_value._type = type;
		// NOTE: (Cesar) entry->_value._ptr is automatically populated ahead

		// Now copy the actual value data
		ptr += sizeof(RdbEntry);
		if(data != nullptr)
		{
			std::memcpy(ptr, data, data_total_size_bytes);
		}
		else
		{
			std::memset(ptr, 0, data_total_size_bytes);
		}
		

		// Finally put the key on the map
		_rdb_offset_map.emplace(key.c_str(), entry);

		_rdb_offset += sizeof(RdbEntry) + data_total_size_bytes;
		LogTrace("[rdb] Created new key: %s", key.c_str());

		RdbEmitWatchMatchCondition(key, entry);

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

		RdbEmitWatchMatchCondition(key, entry);

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

		LogTrace("[rdb] Deleted entry <%s>.", key.c_str());
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

	void RdbTriggerEvent(std::uint64_t clientid, const RdbEntry& entry)
	{
		
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

	bool RdbValueExists(RdbKeyName keyname)
	{
		const RdbEntry* entry = RdbFindEntryByName(keyname);
		return entry != nullptr;
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
			LogError("[rdb] Cannot write rdb value. Data type or length differs.");
		}
		else
		{
			entry->_tmodified = SysGetCurrentTime();
			std::memcpy(entry->_value._ptr, data.getData(), data._data.size());

			if(entry->_flags & RdbEntryFlag::EVENT_MOD_WATCHER)
			{
				// NOTE: (Cesar) : cid is 0 for system modifications
				const std::uint64_t cid = SysGetClientId();

				// Sends the modified data to all subscribers
				// RdbTriggerEvent(cid, *entry);
			}
			
			RdbEmitWatchMatchCondition(keyname, entry);
		}

		RdbUnlockEntryReadWrite(*entry);
	}

	bool RdbCreateValueDirect(mulex::RdbKeyName keyname, mulex::RdbValueType type, std::uint64_t count, mulex::RPCGenericType data)
	{
		return (RdbNewEntry(keyname, type, data.getData(), count) != nullptr);
	}

	void RdbDeleteValueDirect(mulex::RdbKeyName keyname)
	{
		RdbDeleteEntry(keyname);
	}

	std::string RdbMakeWatchEvent(const mulex::RdbKeyName& dir)
	{
		return "mxevt::rdbw-" + SysI64ToHexString(SysStringHash64(dir.c_str()));
	}

	string32 RdbWatch(mulex::RdbKeyName dir)
	{
		std::string event_name = RdbMakeWatchEvent(dir);
		EvtRegister(event_name);
		if(!EvtSubscribe(event_name))
		{
			LogError("[rdb] Failed to watch dir <%s>.", dir.c_str());
			return "";
		}

		// TODO: (Cesar) Watch dirs should remove unsused dirs when a client disconnects
		std::unique_lock<std::shared_mutex> lock(_rdb_watch_lock); // RW lock
		_rdb_watch_dirs.insert(dir.c_str());
		return event_name;
	}

	mulex::RPCGenericType RdbListKeys()
	{
		std::vector<RdbKeyName> keys;
		keys.reserve(_rdb_offset_map.size());
		for(const auto& key : _rdb_offset_map)
		{
			RdbLockEntryRead(*key.second);
			keys.push_back(key.second->_key._name);
			RdbUnlockEntryRead(*key.second);
		}
		return keys;
	}

	static std::vector<RdbKeyName> RdbFindSubkeys(std::string prefix)
	{
		std::vector<RdbKeyName> subkeys;

		if(prefix.back() != '/')
		{
			LogError("[rdb] RdbFindSubkeys needs a prefix (subkey ending with '/').");
			return subkeys;
		}

		const auto it_low = _rdb_offset_map.lower_bound(prefix);

		prefix.back()++;
		const auto it_high = _rdb_offset_map.lower_bound(prefix);

		// TODO: (Cesar) Iterator aritmetic
		// subkeys.reserve(it_high - it_low);

		for(auto it = it_low; it != it_high; it++)
		{
			subkeys.push_back(it->second->_key._name);
		}
		return subkeys;
	}

	mulex::RPCGenericType RdbListSubkeys(mulex::RdbKeyName dir)
	{
		std::string sdir = dir.c_str();
		if(sdir.find('*') == std::string::npos)
		{
			// This is just a prefix
			return RdbFindSubkeys(sdir);
		}

		std::vector<RdbKeyName> keys;

		// This is a prefix with the kleene star operator
		for(const auto& key : _rdb_offset_map)
		{
			RdbLockEntryRead(*key.second);

			const RdbKeyName& name = key.second->_key._name;
			if(SysMatchPattern(sdir, name.c_str()))
			{
				keys.push_back(key.second->_key._name);
			}

			RdbUnlockEntryRead(*key.second);
		}
		return keys;
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

	bool RdbProxyValue::exists()
	{
		std::optional<const Experiment*> exp = SysGetConnectedExperiment();
		if(exp.has_value())
		{
			return exp.value()->_rpc_client->call<bool>(RPC_CALL_MULEX_RDBVALUEEXISTS, RdbKeyName(_key));
		}
		return false;
	}

	bool RdbProxyValue::create(RdbValueType type, RPCGenericType value, std::uint64_t count)
	{
		std::optional<const Experiment*> exp = SysGetConnectedExperiment();
		if(exp.has_value())
		{
			return exp.value()->_rpc_client->call<bool>(RPC_CALL_MULEX_RDBCREATEVALUEDIRECT, RdbKeyName(_key), type, count, value);
		}
		return false;
	}

	bool RdbProxyValue::erase()
	{
		std::optional<const Experiment*> exp = SysGetConnectedExperiment();
		if(exp.has_value())
		{
			exp.value()->_rpc_client->call(RPC_CALL_MULEX_RDBDELETEVALUEDIRECT, RdbKeyName(_key));
			return true;
		}
		return false;
	}

	void RdbProxyValue::watch(std::function<void(const RdbKeyName& key, const RPCGenericType& value)> callback)
	{
		std::optional<const Experiment*> exp = SysGetConnectedExperiment();
		if(!exp.has_value())
		{
			return;
		}
		mulex::string32 event_name = exp.value()->_rpc_client->call<mulex::string32>(RPC_CALL_MULEX_RDBWATCH, RdbKeyName(_key));
		exp.value()->_evt_client->subscribe(event_name.c_str(), [=](const std::uint8_t* data, std::uint64_t len, const std::uint8_t* userdata) {
			RdbKeyName key = reinterpret_cast<const char*>(data);
			RPCGenericType value = RPCGenericType::FromData(
				data + sizeof(RdbKeyName) + sizeof(std::uint64_t),
				*reinterpret_cast<const std::uint64_t*>(data + sizeof(RdbKeyName))
			);
			callback(key, value);
		});
	}
}
