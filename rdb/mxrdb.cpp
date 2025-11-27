// Brief  : MxRDBv2 this experimental version of the rdb does not move blocks on creation on the pool
// Author : César Godinho
// Date   : 08/12/24

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

#include <tracy/Tracy.hpp>

static std::uint8_t* _rdb_handle = nullptr;
static std::uint64_t _rdb_offset = 0;
static std::uint64_t _rdb_size   = 0;

static std::shared_mutex 					   _rdb_rw_lock;
static std::map<std::string, mulex::RdbEntry*> _rdb_offset_map;

static std::set<std::string> _rdb_watch_dirs;
static std::shared_mutex     _rdb_watch_lock;
static std::map<std::string, std::int64_t> _rdb_watch_last_trigger;

static std::unordered_map<std::string, mulex::RdbEntry> _rdb_map;
static std::vector<std::pair<std::uint64_t, std::uint64_t>> _rdb_free_blocks;

struct RdbStatistics
{
	std::atomic<std::uint32_t> _read_ops;
	std::atomic<std::uint32_t> _write_ops;
	std::atomic<std::uint64_t> _total_keys;
	std::atomic<std::uint64_t> _rdb_allocated;
	std::atomic<std::uint64_t> _rdb_size;
};

static constexpr std::int64_t RDB_STATISTICS_INTERVAL = 5000;
static RdbStatistics                _rdb_statistics;
static std::unique_ptr<std::thread> _rdb_statistics_thread;
static std::atomic<bool> 			_rdb_statistics_flag;

struct RdbHistoryData
{
	mulex::RdbKeyName   _key;
	mulex::RdbValueType _type;
	std::uint64_t 		_size;
	std::int64_t  		_timestamp;
	std::uint8_t  		_data[];
};

static std::uint8_t* _rdb_history_handle = nullptr;
static std::uint64_t _rdb_history_offset = 0;
static std::uint64_t _rdb_history_size = 0;

static std::unique_ptr<mulex::PdbAccessLocal> _rdb_history_access;

static std::shared_mutex _rdb_history_rw_lock;

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

	std::uint64_t operator~ (RdbEntryFlag a)
	{
		return ~static_cast<std::uint64_t>(a);
	}

	static std::uint64_t FindString(const char* data, std::uint64_t idx)
	{
		ZoneScoped;
		while(data[idx] != 0) idx++;
		return idx + 1;
	}

	static void RdbLoadOffsetMap(const char* data, std::uint64_t size)
	{
		ZoneScoped;
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
		ZoneScoped;
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
		ZoneScoped;
		_rdb_statistics._rdb_allocated.store(size);
#ifdef __unix__
		return reinterpret_cast<std::uint8_t*>(std::aligned_alloc(align, size));
#else
		return reinterpret_cast<std::uint8_t*>(_aligned_malloc(size, align));
#endif
	}

	static void RdbAlignedFree(std::uint8_t* ptr)
	{
		ZoneScoped;
#ifdef __unix__
		std::free(ptr);
#else
		_aligned_free(ptr);
#endif
	}

	static void RdbLoadFromFile(const std::string& filename)
	{
		ZoneScoped;
		// Load the existing rdb data
		std::vector<std::uint8_t> data = SysReadBinFile(filename);

		// Extract map size from the raw data
		std::uint64_t mapsize = *reinterpret_cast<std::uint64_t*>(data.data());
		LogTrace("[rdb] Load mapsize: %llu kb.", mapsize / 1024);

		// Extract rdb size from the raw data
		std::uint64_t rdbsize = *reinterpret_cast<std::uint64_t*>(data.data() + sizeof(std::uint64_t));
		std::uint64_t rdbsize_unaligned = rdbsize;
		LogTrace("[rdb] Load rdb true size %llu kb.", rdbsize / 1024);

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
			LogTrace("[rdb] Load rdb aligned size %llu kb.", rdbsize / 1024);
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
		ZoneScoped;
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

	static void RdbStatisticsThread()
	{
		static const std::string root_key = "/system/rdb/statistics/";
		static std::uint32_t initializer32 = 0;
		static std::uint64_t initializer64 = 0;

		RdbNewEntry(root_key + "read", RdbValueType::UINT32, &initializer32);
		RdbNewEntry(root_key + "write", RdbValueType::UINT32, &initializer32);
		RdbNewEntry(root_key + "nkeys", RdbValueType::UINT64, &initializer64);
		RdbNewEntry(root_key + "allocated", RdbValueType::UINT64, &initializer64);
		RdbNewEntry(root_key + "size", RdbValueType::UINT64, &initializer64);

		while(_rdb_statistics_flag.load())
		{
			std::int64_t start = SysGetCurrentTime();

			RdbWriteValueDirect(root_key + "read", _rdb_statistics._read_ops.exchange(0));
			RdbWriteValueDirect(root_key + "write", _rdb_statistics._write_ops.exchange(0));
			RdbWriteValueDirect(root_key + "nkeys", _rdb_statistics._total_keys.load());
			RdbWriteValueDirect(root_key + "allocated", _rdb_statistics._rdb_allocated.load());
			RdbWriteValueDirect(root_key + "size", _rdb_statistics._rdb_size.load());

			// Every 5 seconds
			std::this_thread::sleep_for(std::chrono::milliseconds(RDB_STATISTICS_INTERVAL - ((SysGetCurrentTime() - start))));
		}
	}

	void RdbInit(std::uint64_t size)
	{
		ZoneScoped;
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

		// Register events for rdb
		EvtRegister("mxrdb::keycreated"); // Rdb key created
		EvtRegister("mxrdb::keydeleted"); // Rdb key deleted
	
		_rdb_statistics._rdb_size.store(_rdb_offset);
		_rdb_statistics._total_keys.store(_rdb_offset_map.size());
		_rdb_statistics._read_ops.store(0);
		_rdb_statistics._write_ops.store(0);

		_rdb_statistics_flag.store(true);
		// _rdb_statistics_thread = std::make_unique<std::thread>(RdbStatisticsThread);

		LogDebug("[rdb] Init() OK. Allocated: %llu kb.", _rdb_size / 1024);
	}

	void RdbClose()
	{
		ZoneScoped;
		LogDebug("[rdb] Closing rdb.");

		_rdb_statistics_flag.store(false);
		// _rdb_statistics_thread->join();
		// _rdb_statistics_thread.reset();

		// RdbDumpMetadata("rdb_dump.txt");

		std::unique_lock lock(_rdb_rw_lock);

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

	static std::uint64_t RdbCalculateDataSize(const RdbEntry* entry)
	{
		ZoneScoped;
		return entry->_count > 0 ? entry->_count * entry->_size : entry->_size;
	}

	static std::uint64_t RdbCalculateEntryTotalSize(const RdbEntry* entry)
	{
		ZoneScoped;
		return sizeof(RdbEntry) + RdbCalculateDataSize(entry);
	}

	void RdbInitHistoryBuffer()
	{
		ZoneScoped;
		std::unique_lock lock(_rdb_history_rw_lock);
		constexpr static std::uint64_t RDB_HISTORY_DEF_SIZE = 1024 * 1024 * 100; // 100 kB
		_rdb_history_size = RDB_HISTORY_DEF_SIZE;
		_rdb_history_offset = 0;

		if(RdbNewEntry("/system/rdb/history_buffer_size", RdbValueType::UINT64, &RDB_HISTORY_DEF_SIZE) == nullptr)
		{
			_rdb_history_size = RdbReadValueDirect("/system/rdb/history_buffer_size");
		}

		_rdb_history_handle = RdbAlignedAlloc(1024, _rdb_history_size);

		if(!_rdb_history_handle)
		{
			LogError("[rdb] Failed to create rdb history buffer.");
			LogError("[rdb] Allocation failed with size: %llu kb.", _rdb_history_size / 1024);
			_rdb_history_size = 0;
			return;
		}

		_rdb_history_access = std::make_unique<PdbAccessLocal>();

		_rdb_history_access->createTable("history", {
			"id INTEGER PRIMARY KEY AUTOINCREMENT",
			"keyname TEXT NOT NULL",
			"timestamp BIGINT NOT NULL",
			"type INTEGER NOT NULL",
			"data BLOB NOT NULL"
		});

		LogDebug("[rdb] RdbInitHistoryBuffer() OK.");
	}

	void RdbCloseHistoryBuffer()
	{
		ZoneScoped;
		std::unique_lock lock(_rdb_history_rw_lock);
		if(_rdb_history_handle)
		{
			RdbHistoryFlushUnlocked();
			_rdb_history_size = 0;
			_rdb_history_offset = 0;
			RdbAlignedFree(_rdb_history_handle);
		}

		_rdb_history_access.reset();

		LogDebug("[rdb] RdbCloseHistoryBuffer() OK.");
	}

	void RdbHistoryFlush()
	{
		ZoneScoped;
		std::unique_lock lock(_rdb_history_rw_lock);
		RdbHistoryFlushUnlocked();
	}

	void RdbHistoryFlushUnlocked()
	{
		ZoneScoped;
		static auto history_writer = _rdb_history_access->getWriter<
			std::int32_t,
			PdbString,
			std::int64_t,
			std::uint8_t,
			std::vector<std::uint8_t>
		>("history", {"id", "keyname", "timestamp", "type", "data"});

		std::uint64_t iterator = 0;
		static const std::vector<PdbValueType> types = {
			PdbValueType::INT32,
			PdbValueType::STRING,
			PdbValueType::INT64,
			PdbValueType::UINT8,
			PdbValueType::BINARY };
		while(iterator < _rdb_history_offset)
		{
			RdbHistoryData* data = reinterpret_cast<RdbHistoryData*>(_rdb_history_handle + iterator);
			history_writer(
				std::nullopt,
				data->_key,
				data->_timestamp,
				static_cast<std::uint8_t>(data->_type),
				std::vector<std::uint8_t>(data->_data, data->_data + data->_size)
			);
			iterator += (sizeof(RdbHistoryData) + data->_size);
		}
		
		_rdb_history_offset = 0; // Reset the history memory cache
	}

	void RdbHistoryAdd(RdbEntry* entry, const RdbKeyName& key)
	{
		ZoneScoped;
		// Entry must be locked at this point
		// Read lock is sufficient
		std::unique_lock lock(_rdb_history_rw_lock);
		std::uint64_t entry_data_size = RdbCalculateDataSize(entry);

		if(_rdb_history_offset + sizeof(RdbHistoryData) + entry_data_size > _rdb_history_size)
		{
			RdbHistoryFlushUnlocked();
		}

		RdbHistoryData data;
		data._timestamp = entry->_tmodified;
		data._key = key;
		data._type = entry->_type;
		data._size = entry_data_size;
		std::memcpy(_rdb_history_handle + _rdb_history_offset, &data, sizeof(RdbHistoryData));
		std::memcpy(_rdb_history_handle + _rdb_history_offset + sizeof(RdbHistoryData), entry->_ptr, entry_data_size);
		_rdb_history_offset += (sizeof(RdbHistoryData) + entry_data_size);
	}

	static bool RdbGrow()
	{
		ZoneScoped;
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
		ZoneScoped;
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

	static void RdbEmitEvtCondition(const std::string& swatch, const RdbKeyName& key, const RdbEntry* entry)
	{
		ZoneScoped;
		std::string event_name = RdbMakeWatchEvent(swatch);
		std::vector<std::uint8_t> evt_buffer;
		std::uint64_t entry_data_size = RdbCalculateDataSize(entry);
		evt_buffer.resize(sizeof(RdbKeyName) + sizeof(std::uint64_t) + entry_data_size);
		std::uint64_t offset = EvtDataAppend(0, &evt_buffer, key);
		offset = EvtDataAppend(offset, &evt_buffer, entry_data_size);
		offset = EvtDataAppend(offset, &evt_buffer, entry->_ptr, entry_data_size);
		// LogTrace("Emit watch event <%s> from swatch: <%s>.", event_name.c_str(), swatch.c_str());

		if(!EvtEmit(event_name, evt_buffer.data(), evt_buffer.size()))
		{
			// This watch is not subscribed by anyone and was not triggered for 5 seconds, remove it
			// Defer RdbUnwatch due to unique_lock
			if(SysGetCurrentTime() - _rdb_watch_last_trigger[swatch] > 5000)
			{
				std::thread([=]() {
					RdbUnwatch(swatch);
					LogTrace("[rdb] swatch <%s> was dangling for more than 5 seconds. Removed.", swatch.c_str());
				}).detach();
			}
		}
		else
		{
			// Event triggered OK, set last trigger
			_rdb_watch_last_trigger[swatch] = SysGetCurrentTime();
		}
	}

	// TODO: (Cesar) Move this code to on creation and add a flag for scanning
	//				 That would make this a lot faster
	static void RdbEmitWatchMatchCondition(const RdbKeyName& key, const RdbEntry* entry)
	{
		ZoneScoped;
		std::shared_lock<std::shared_mutex> lock_watch(_rdb_watch_lock);
		for(const auto& watch : _rdb_watch_dirs)
		{
			if(SysMatchPattern(watch, key.c_str()))
			{
				RdbEmitEvtCondition(watch, key, entry);
				LogTrace("[rdb] Match: <%s> -> <%s>.", watch.c_str(), key.c_str());
			}
		}
	}

	static std::uint64_t RdbCalculateEntryOffset(RdbEntry* entry)
	{
		ZoneScoped;
		return (reinterpret_cast<std::uint8_t*>(entry) - _rdb_handle);
	}

	RdbEntry* RdbAllocate(std::uint64_t size)
	{
		ZoneScoped;
		const std::uint64_t total_size = sizeof(RdbEntry) + size;

		// Check for free blocks
		for(auto it = _rdb_free_blocks.begin(); it != _rdb_free_blocks.end(); it++)
		{
			auto& [offset, free_size] = *it;

			if(free_size >= total_size)
			{
				offset += total_size;
				free_size -= total_size;
				_rdb_offset += total_size;

				if(free_size == 0)
				{
					_rdb_free_blocks.erase(it);
				}

				return new (_rdb_handle + offset) RdbEntry();
			}
		}

		// No free blocks so we put it at the end
		
		// No space left
		if(_rdb_offset + total_size > _rdb_size)
		{
			if(!RdbGrow())
			{
				return nullptr;
			}
			return RdbAllocate(size);
		}

		// Enough space at the end OK.
		std::uint64_t offset = _rdb_offset;
		_rdb_offset += total_size;
		return new (_rdb_handle + offset) RdbEntry();
	}

	void RdbFree(RdbEntry* entry)
	{
		ZoneScoped;
		entry->~RdbEntry();
		std::uint64_t free_offset = RdbCalculateEntryOffset(entry);
		std::uint64_t free_size = RdbCalculateEntryTotalSize(entry);
		_rdb_free_blocks.emplace_back(free_offset, free_size);

		// if(free_offset + free_size == _rdb_offset)
		// {
		// 	_rdb_offset -= free_size;
		// }
		// else
		// {
		// 	_rdb_free_blocks.emplace_back(free_offset, free_size);
		// }
	}

	RdbEntry* RdbNewEntry(const RdbKeyName& key, const RdbValueType& type, const void* data, std::uint64_t count)
	{
		ZoneScoped;
		std::unique_lock lock(_rdb_rw_lock);

		if(RdbFindEntryByNameUnlocked(key))
		{
			LogTrace("[rdb] Cannot create already existing key.");
			return nullptr;
		}

		// Lock database map and handle for creation
		const std::uint64_t data_size = RdbTypeSize(type);
		const std::uint64_t data_total_size_bytes = count > 0 ? count * data_size : data_size;

		RdbEntry* entry = RdbAllocate(data_total_size_bytes);

		if(entry == nullptr)
		{
			LogError("[rdb] RdbAllocate failed.");
			return nullptr;
		}

		// Setup entry related statistics
		entry->_tcreated = SysGetCurrentTime();
		entry->_tmodified = entry->_tcreated;

		entry->_count = count;
		entry->_size = data_size;
		entry->_type = type;
		entry->_flags = 0;
		
		// + checks
		if(data != nullptr)
		{
			std::memcpy(entry->_ptr, data, data_total_size_bytes);
		}
		else
		{
			std::memset(entry->_ptr, 0, data_total_size_bytes);
		}

		_rdb_offset_map.emplace(key.c_str(), entry);

		RdbEmitWatchMatchCondition(key, entry);
		EvtEmit("mxrdb::keycreated", reinterpret_cast<const std::uint8_t*>(key.c_str()), sizeof(RdbKeyName));
		_rdb_statistics._write_ops.fetch_add(1);
		_rdb_statistics._total_keys.fetch_add(1);

		LogTrace("[rdb] Created new key: <%s>.", key.c_str());
		return entry;
	}

	bool RdbDeleteEntry(const RdbKeyName& key)
	{
		ZoneScoped;
		// Lock database map and handle for deletion
		// This also locks W/R access to any key
		std::unique_lock lock(_rdb_rw_lock);

		RdbEntry* entry = RdbFindEntryByNameUnlocked(key);
		if(!entry)
		{
			LogError("[rdb] Cannot delete unknown key: <%s>.", key.c_str());
			return false;
		}

		// RdbEmitWatchMatchCondition(key, entry);

		_rdb_offset_map.erase(key.c_str());
		RdbFree(entry);

		EvtEmit("mxrdb::keydeleted", reinterpret_cast<const std::uint8_t*>(key.c_str()), sizeof(RdbKeyName));
		_rdb_statistics._write_ops.fetch_add(1);
		_rdb_statistics._total_keys.fetch_sub(1);

		LogTrace("[rdb] Deleted entry <%s>.", key.c_str());
		return true;
	}

	RdbEntry* RdbFindEntryByNameUnlocked(const RdbKeyName& key)
	{
		ZoneScoped;
		auto it = _rdb_offset_map.find(key.c_str());
		if(it == _rdb_offset_map.end())
		{
			LogTrace("[rdb] Trying to access unknown rdb key: <%s>.", key.c_str());
			return nullptr;
		}
		
		return it->second;
	}

	RdbEntry* RdbFindEntryByName(const RdbKeyName& key)
	{
		ZoneScoped;
		std::shared_lock lock(_rdb_rw_lock);
		return RdbFindEntryByNameUnlocked(key);
	}

	void RdbDumpMetadata(const std::string& filename)
	{
		ZoneScoped;
		std::ofstream output(filename);
		if(!output.is_open())
		{
			LogError("[rdb] Failed to open file <%s> for RDB metadata dump.", filename.c_str());
			return;
		}

		LogDebug("[rdb] Dumping RDB metadata to file <%s>.", filename.c_str());

		std::unique_lock lock(_rdb_rw_lock);
		for(const auto& it : _rdb_offset_map)
		{
	  		const RdbEntry* const entry = it.second;
			output << "Key: " << it.first << '\n';
			output << "\t" << "Value type: " << static_cast<int>(entry->_type) << '\n';
			output << "\t" << "Value ptr: " << entry + sizeof(RdbEntry) << '\n';
			output << "\t" << "Value size: " << entry->_size << '\n';
			output << "\t" << "Value count: " << entry->_count << '\n';
			output << "\t" << "Entry ptr: " << entry << '\n';
			output << "\t" << "Entry size: " << RdbCalculateEntryTotalSize(entry) << '\n';
			output << std::endl;
		}
	}

	RPCGenericType RdbReadValueDirect(RdbKeyName keyname)
	{
		ZoneScoped;
		std::shared_lock lock_ops(_rdb_rw_lock);

		const RdbEntry* entry = RdbFindEntryByNameUnlocked(keyname);
		if(!entry)
		{
			return std::vector<std::uint8_t>();
		}

		std::shared_lock lock_entry(entry->_rw_lock);

		// Entry is read locked
		std::uint64_t size = RdbCalculateDataSize(entry);
		std::vector<std::uint8_t> buffer(size);
		std::memcpy(buffer.data(), entry->_ptr, size);

		_rdb_statistics._read_ops.fetch_add(1);

		return RPCGenericType::FromData(buffer);
	}

	bool RdbValueExists(RdbKeyName keyname)
	{
		ZoneScoped;
		std::shared_lock lock(_rdb_rw_lock);
		return RdbFindEntryByNameUnlocked(keyname) != nullptr;
	}

	RPCGenericType RdbReadKeyMetadata(RdbKeyName keyname)
	{
		ZoneScoped;
		std::shared_lock lock_ops(_rdb_rw_lock);

		const RdbEntry* entry = RdbFindEntryByNameUnlocked(keyname);
		if(!entry)
		{
			return std::vector<std::uint8_t>();
		}

		_rdb_statistics._read_ops.fetch_add(1);

		std::shared_lock lock_entry(entry->_rw_lock);
		return entry->_type;
	}

	void RdbWriteValueDirect(mulex::RdbKeyName keyname, RPCGenericType data)
	{
		ZoneScoped;
		std::shared_lock lock_ops(_rdb_rw_lock);

		RdbEntry* entry = RdbFindEntryByNameUnlocked(keyname);
		if(!entry)
		{
			return;
		}

		std::unique_lock lock_entry(entry->_rw_lock);

		// Entry is read/write locked
		if(data.getSize() != RdbCalculateDataSize(entry))
		{
			LogError("[rdb] Cannot write rdb value. Data type or length differs. <%s>", keyname.c_str());
			LogError("[rdb] Expected <%llu>. Got <%llu>.", RdbCalculateDataSize(entry), data.getSize());
		}
		else
		{
			entry->_tmodified = SysGetCurrentTime();
			std::memcpy(entry->_ptr, data.getData(), data.getSize());

			if(entry->_flags & RdbEntryFlag::HISTORY_ENABLED)
			{
				RdbHistoryAdd(entry, keyname);
			}
			
			RdbEmitWatchMatchCondition(keyname, entry);
			_rdb_statistics._write_ops.fetch_add(1);
		}
	}

	bool RdbCreateValueDirect(mulex::RdbKeyName keyname, mulex::RdbValueType type, std::uint64_t count, mulex::RPCGenericType data)
	{
		ZoneScoped;
		return (RdbNewEntry(keyname, type, data.getData(), count) != nullptr);
	}

	void RdbDeleteValueDirect(mulex::RdbKeyName keyname)
	{
		ZoneScoped;
		RdbDeleteEntry(keyname);
	}

	std::string RdbMakeWatchEvent(const mulex::RdbKeyName& dir)
	{
		ZoneScoped;
		return "mxevt::rdbw-" + SysI64ToHexString(SysStringHash64(dir.c_str()));
	}

	string32 RdbWatch(mulex::RdbKeyName dir)
	{
		ZoneScoped;
		std::string event_name = RdbMakeWatchEvent(dir);
		EvtRegister(event_name);

		std::unique_lock<std::shared_mutex> lock(_rdb_watch_lock); // RW lock
		_rdb_watch_dirs.insert(dir.c_str());
		_rdb_watch_last_trigger[dir.c_str()] = SysGetCurrentTime();
		return event_name;
	}

	string32 RdbUnwatch(mulex::RdbKeyName dir)
	{
		ZoneScoped;
		std::unique_lock<std::shared_mutex> lock(_rdb_watch_lock); // RW lock
		auto wit = _rdb_watch_dirs.find(dir.c_str());
		if(wit == _rdb_watch_dirs.end())
		{
			LogError("[rdb] Failed to unwatch dir <%s>.", dir.c_str());
			return "";
		}
		_rdb_watch_dirs.erase(wit);
		_rdb_watch_last_trigger.erase(dir.c_str());
		return RdbMakeWatchEvent(dir);
	}

	mulex::RPCGenericType RdbListKeys()
	{
		ZoneScoped;
		std::shared_lock lock(_rdb_rw_lock);
		std::vector<RdbKeyName> keys;
		keys.reserve(_rdb_offset_map.size());
		for(const auto& key : _rdb_offset_map)
		{
			keys.push_back(key.first);
		}
		return keys;
	}

	mulex::RPCGenericType RdbListKeyTypes()
	{
		ZoneScoped;
		std::shared_lock lock(_rdb_rw_lock);
		std::vector<std::uint8_t> types;
		types.reserve(_rdb_offset_map.size());
		for(const auto& key : _rdb_offset_map)
		{
			types.push_back(static_cast<std::underlying_type_t<RdbValueType>>(key.second->_type));
		}
		return types;
	}

	static std::vector<RdbKeyName> RdbFindSubkeys(std::string prefix)
	{
		ZoneScoped;
		std::vector<RdbKeyName> subkeys;

		if(prefix.back() != '/')
		{
			LogError("[rdb] RdbFindSubkeys needs a prefix (subkey ending with '/').");
			return subkeys;
		}

		const auto it_low = _rdb_offset_map.lower_bound(prefix);

		prefix.back()++;
		const auto it_high = _rdb_offset_map.lower_bound(prefix);

		for(auto it = it_low; it != it_high; it++)
		{
			subkeys.push_back(it->first);
		}
		return subkeys;
	}

	mulex::RPCGenericType RdbListSubkeys(mulex::RdbKeyName dir)
	{
		ZoneScoped;
		std::shared_lock lock(_rdb_rw_lock);

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
			if(SysMatchPattern(sdir, key.first))
			{
				keys.push_back(key.first);
			}
		}

		return keys;
	}

	std::uint8_t RdbGetKeyType(mulex::RdbKeyName key)
	{
		ZoneScoped;

		std::shared_lock lock_ops(_rdb_rw_lock);
		const RdbEntry* entry = RdbFindEntryByNameUnlocked(key);

		if(!entry)
		{
			return 0xFF;
		}

		std::shared_lock lock_entry(entry->_rw_lock);
		return static_cast<std::uint8_t>(entry->_type);
	}

	static inline std::uint64_t RdbSetEntryFlag(std::uint64_t state, RdbEntryFlag flag, bool active)
	{
		ZoneScoped;
		return (active) ? (state | flag) : (state & (~flag));
	}

	bool RdbToggleHistory(mulex::RdbKeyName keyname, bool active)
	{
		ZoneScoped;
		std::shared_lock lock_ops(_rdb_rw_lock);

		RdbEntry* entry = RdbFindEntryByNameUnlocked(keyname);
		if(!entry)
		{
			return false;
		}

		std::unique_lock lock_entry(entry->_rw_lock);
		entry->_flags = RdbSetEntryFlag(entry->_flags, RdbEntryFlag::HISTORY_ENABLED, active);
		return true;
	}

	static std::vector<std::uint8_t> RdbReadHistoryLimit(const RdbKeyName& keyname, std::uint64_t count)
	{
		ZoneScoped;
		static auto history_reader = _rdb_history_access->getReaderRaw<
			std::int32_t,
			PdbString,
			std::int64_t,
			std::uint8_t,
			std::vector<std::uint8_t>
		>("history", {"id", "keyname", "timestamp", "type", "data"});

		std::string condition = "WHERE keyname = '" + std::string(keyname.c_str()) + "' ORDER BY timestamp";
		if(count > 0)
		{
			condition += " LIMIT " + std::to_string(count);
		}

		const std::vector<std::uint8_t> data = history_reader(condition);
		LogTrace("[rdb] RdbReadHistoryLimit() OK. Read %llu bytes. For key <%s>.", data.size(), keyname.c_str());
		return data;
	}

	// NOTE: (Cesar) This function is pretty I/O intensive
	mulex::RPCGenericType RdbGetHistory(mulex::RdbKeyName keyname, std::uint64_t count)
	{
		ZoneScoped;
		std::vector<uint8_t> output;
		std::shared_lock lock_ops(_rdb_rw_lock);

		RdbEntry* entry = RdbFindEntryByNameUnlocked(keyname);
		if(!entry)
		{
			return output;
		}

		std::shared_lock lock_entry(entry->_rw_lock);
		if(!(entry->_flags & RdbEntryFlag::HISTORY_ENABLED))
		{
			return output;
		}

		// Flush before read
		RdbHistoryFlush();

		return RdbReadHistoryLimit(keyname, count);
	}

	void RdbProxyValue::writeEntry()
	{
		ZoneScoped;
		std::optional<const Experiment*> exp = SysGetConnectedExperiment();
		if(exp.has_value())
		{
			exp.value()->_rpc_client->call(RPC_CALL_MULEX_RDBWRITEVALUEDIRECT, mulex::RdbKeyName(_key), _genvalue);
		}
	}

	void RdbProxyValue::readEntry()
	{
		ZoneScoped;
		std::optional<const Experiment*> exp = SysGetConnectedExperiment();
		if(exp.has_value())
		{
			_genvalue = exp.value()->_rpc_client->call<RPCGenericType, RdbKeyName>(RPC_CALL_MULEX_RDBREADVALUEDIRECT, RdbKeyName(_key));
		}
	}

	bool RdbProxyValue::exists() const
	{
		ZoneScoped;
		std::optional<const Experiment*> exp = SysGetConnectedExperiment();
		if(exp.has_value())
		{
			return exp.value()->_rpc_client->call<bool>(RPC_CALL_MULEX_RDBVALUEEXISTS, RdbKeyName(_key));
		}
		return false;
	}

	bool RdbProxyValue::create(RdbValueType type, RPCGenericType value, std::uint64_t count)
	{
		ZoneScoped;
		std::optional<const Experiment*> exp = SysGetConnectedExperiment();
		if(exp.has_value())
		{
			return exp.value()->_rpc_client->call<bool>(RPC_CALL_MULEX_RDBCREATEVALUEDIRECT, RdbKeyName(_key), type, count, value);
		}
		return false;
	}

	bool RdbProxyValue::erase()
	{
		ZoneScoped;
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
		ZoneScoped;
		std::optional<const Experiment*> exp = SysGetConnectedExperiment();
		if(!exp.has_value())
		{
			return;
		}
		_swatch_event = exp.value()->_rpc_client->call<mulex::string32>(RPC_CALL_MULEX_RDBWATCH, RdbKeyName(_key)).c_str();
		exp.value()->_evt_client->subscribe(_swatch_event, [=](const std::uint8_t* data, std::uint64_t len, const std::uint8_t* userdata) {
			RdbKeyName key = reinterpret_cast<const char*>(data);
			RPCGenericType value = RPCGenericType::FromData(
				data + sizeof(RdbKeyName) + sizeof(std::uint64_t),
				*reinterpret_cast<const std::uint64_t*>(data + sizeof(RdbKeyName))
			);
			callback(key, value);
		});
	}

	void RdbProxyValue::unwatch()
	{
		// TODO: (César): One should fix the event sub/unsub when the same
		// 				  backends / actors are using it multiple times
		ZoneScoped;
		std::optional<const Experiment*> exp = SysGetConnectedExperiment();
		if(!exp.has_value() || _swatch_event.empty())
		{
			return;
		}
		exp.value()->_evt_client->unsubscribe(_swatch_event);
	}

	bool RdbProxyValue::history(bool status)
	{
		ZoneScoped;
		std::optional<const Experiment*> exp = SysGetConnectedExperiment();
		if(!exp.has_value())
		{
			return false;
		}
		return exp.value()->_rpc_client->call<bool>(RPC_CALL_MULEX_RDBTOGGLEHISTORY, RdbKeyName(_key), status);
	}

	RdbValueType RdbProxyValue::type() const
	{
		ZoneScoped;
		std::optional<const Experiment*> exp = SysGetConnectedExperiment();
		if(exp.has_value())
		{
			return static_cast<RdbValueType>(exp.value()->_rpc_client->call<std::uint8_t>(RPC_CALL_MULEX_RDBGETKEYTYPE, RdbKeyName(_key)));
		}
		return static_cast<RdbValueType>(0);
	}
}
