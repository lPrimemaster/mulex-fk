// Brief  : MxTrx streams realtime trace records to the frontend
// Author : César Godinho
// Date   : 18/04/26

#include "../mxtrace.h"
#include "../mxevt.h"
#include <atomic>
#include <functional>
#include <shared_mutex>
#include <unordered_map>

static constexpr std::int64_t  TRX_STREAM_INTERVAL = 500;
static constexpr std::uint64_t 		TRX_MPSCQ_SIZE = 2048;

static std::atomic<std::uint64_t>    		 _trx_record_id = 0;
static mulex::SysAsyncEventLoop 	 		 _trx_emit_io;
static mulex::SysMPSCQueue<mulex::TrxRecord> _trx_record_queue(TRX_MPSCQ_SIZE, _trx_emit_io);
static std::vector<mulex::TrxRecord> 		 _trx_flush_buffer;
static std::atomic<bool>					 _trx_flush_pending = false;
static std::shared_mutex					 _trx_init_lock;
static bool									 _trx_init = false;

// NOTE: (César) This assumes that the string interning is
// 				 always done on string literals
// 				 Also hack the hash to just be the identity function
// 				 std::hash<std::uint32_t> is apparently the identity function
// 				 but lets be explicit just in case...
static std::unordered_map<std::uint32_t, std::string_view, std::identity> _trx_id_interner_map;
static std::shared_mutex  									  			  _trx_id_interner_lock;

struct TrxInternNewValueEvent
{
	mulex::TrxFuncId _id;
	mulex::string512 _str;
};

namespace mulex
{
	TrxTag operator& (std::uint8_t a, TrxTag b)
	{
		return static_cast<TrxTag>(a & static_cast<std::uint8_t>(b));
	}

	TrxTag operator& (TrxTag a, TrxTag b)
	{
		return static_cast<TrxTag>(static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b));
	}

	TrxTag operator| (TrxTag a, TrxTag b)
	{
		return static_cast<TrxTag>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
	}

	TrxTag operator| (std::uint8_t a, TrxTag b)
	{
		return static_cast<TrxTag>(a | static_cast<std::uint8_t>(b));
	}

	TrxTag operator&= (std::uint8_t a, TrxTag b)
	{
		b = a & b;
		return b;
	}

	TrxTag operator|= (std::uint8_t a, TrxTag b)
	{
		b = a | b;
		return b;
	}

	TrxTag operator~ (TrxTag a)
	{
		return static_cast<TrxTag>(~static_cast<std::uint8_t>(a));
	}

	static inline bool TrxInternMapSafeFindHash(std::uint32_t hash)
	{
		std::shared_lock lock(_trx_id_interner_lock);
		return _trx_id_interner_map.find(hash) != _trx_id_interner_map.end();
	}

	static inline bool TrxInternTryEmplace(std::uint32_t hash, std::string_view str)
	{
		std::unique_lock lock(_trx_id_interner_lock);
		auto [_, inserted] = _trx_id_interner_map.try_emplace(hash, str);
		return inserted;
	}

	static inline void TrxInternEmitValue(std::uint32_t hash, std::string_view str)
	{
		// Emiting new intern table values needs to happen ASAP
		// the clients need to know what the newly received id's stand for
		TrxInternNewValueEvent event {
			._id = hash,
			._str = str
		};
		EvtEmit("mxtrace::intern_newval", reinterpret_cast<std::uint8_t*>(&event), sizeof(TrxInternNewValueEvent));
	}

	// NOTE: (César) Hash collision is "handled" on the display frontend
	TrxFuncId TrxInternStringHash(std::uint32_t hash, std::string_view str)
	{
		if(TrxInternMapSafeFindHash(hash)) [[likely]]
		{
			return hash;
		}
		else [[unlikely]]
		{
			if(TrxInternTryEmplace(hash, str))
			{
				TrxInternEmitValue(hash, str);
			}

			// NOTE: (César) Someone else might have emplaced in the mean time
			// 				 so just return the hash anyways
			return hash;
		}
	}

	TrxRecord TrxGenerateRecord(TrxType type, TrxTag tags, std::uint64_t rid)
	{
		TrxRecord record;

		record._self_cid = SysGetClientId();
		record._self_rid = rid;

		record._timestamp = SysGetCurrentTime();
		record._type = type;
		record._tags = tags;

		record._trigger_cid = 0x00; // TODO: (César)
		record._trigger_rid = 0x00; // TODO: (César)

		// NOTE: (César) Instead of relying on possible NRVO one should construct record here directly
		return record; 
	}

	static std::uint64_t TrxGetNextRecordId()
	{
		return _trx_record_id++;
	}

	static void TrxFlushQueue()
	{
		_trx_record_queue.flush(_trx_flush_buffer);
		if(!_trx_flush_buffer.empty())
		{
			std::vector<std::uint8_t> buffer = SysPackArguments(
					std::uint64_t(_trx_flush_buffer.size()),
					std::vector<uint8_t>(
						reinterpret_cast<uint8_t*>(_trx_flush_buffer.data()),
						reinterpret_cast<uint8_t*>(_trx_flush_buffer.data()) + _trx_flush_buffer.size() * sizeof(TrxRecord)
					)
			);
			EvtEmit("mxtrace::record", buffer.data(), buffer.size());
			_trx_flush_buffer.clear();
		}
	}

	static void TrxScheduleFlush()
	{
		if (!_trx_flush_pending.exchange(true, std::memory_order_acq_rel))
		{
			_trx_emit_io.schedule([](){
				_trx_flush_pending.store(false, std::memory_order_release);
				TrxFlushQueue();
			});
		}
	}

	static void TrxScheduleEmitRecord(TrxRecord&& record)
	{
		// Early out if init failed
		std::shared_lock lock(_trx_init_lock);
		if(!_trx_init) return;

		std::int32_t retries = 0;
		while(!_trx_record_queue.enqueue(record) && retries < 3)
		{
			// Hard backpressure!!
			TrxScheduleFlush();
			std::this_thread::yield();
			mulex::LogWarning("[trx] Hard backpressure detected. Consider a larger MCSP queue buffer.");
			retries++;
		}
		if(_trx_record_queue.shouldFlushNow())
		{
			// Handle soft backpressure by triggering
			// the consumer to emit records ASAP
			// We want to be here
			TrxScheduleFlush();
		}
	}

	void TrxInit()
	{
		EvtRegister("mxtrace::record");
		EvtRegister("mxtrace::intern_newval");
		_trx_flush_buffer.reserve(TRX_MPSCQ_SIZE);

		// Stream only once every X ms
		_trx_emit_io.schedule(TrxFlushQueue, 0, TRX_STREAM_INTERVAL);

		std::unique_lock lock(_trx_init_lock);
		_trx_init = true;
	}

	TrxScopeGuard::TrxScopeGuard(TrxType type, TrxTag tags, TrxFuncId id) : _type(type), _tags(tags), _fid(id), _rid(TrxGetNextRecordId())
	{
		TrxScheduleEmitRecord(TrxGenerateRecord(_type, _tags | TrxTag::TRX_START, _rid));

		// // Trace
		std::shared_lock lock(_trx_id_interner_lock);
		if(_trx_id_interner_map.find(_fid) != _trx_id_interner_map.end())
		{
			LogTrace("[mxtrace] New record: fid_hash <0x%x> | fid <%s> | rid <%llu>.", _fid, _trx_id_interner_map[_fid].data(), _rid);
		}
	}

	TrxScopeGuard::~TrxScopeGuard()
	{
		TrxScheduleEmitRecord(TrxGenerateRecord(_type, _tags | TrxTag::TRX_STOP, _rid));
	}
} // namespace mulex
