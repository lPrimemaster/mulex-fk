// Brief  : MxTrx streams realtime trace records to the frontend
// Author : César Godinho
// Date   : 18/04/26

#include "../mxtrace.h"
#include "../mxevt.h"
#include <atomic>
#include <functional>
#include <shared_mutex>
#include <tracy/Tracy.hpp>
#include <unordered_map>

static constexpr std::int64_t  TRX_STREAM_INTERVAL = 500;
static constexpr std::uint64_t 		TRX_MPSCQ_SIZE = 1024;

// NOTE: (César)
// 56B Alignment (8)
struct alignas(8) TrxRecordWithMeta
{
	mulex::TrxRecord _record;
	const char*		 _meta;
};

static std::atomic<std::uint64_t>    		  _trx_record_id = 0;
static mulex::SysAsyncEventLoop 	 		  _trx_emit_io;
static mulex::SysMPSCQueue<TrxRecordWithMeta> _trx_record_queue(TRX_MPSCQ_SIZE, _trx_emit_io);
static std::vector<TrxRecordWithMeta> 		  _trx_flush_buffer;
static std::vector<mulex::TrxRecord>		  _trx_emit_buffer;
static std::atomic<bool>					  _trx_flush_pending = false;

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
	mulex::string128 _str;
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
		ZoneScoped;
		std::shared_lock lock(_trx_id_interner_lock);
		return _trx_id_interner_map.find(hash) != _trx_id_interner_map.end();
	}

	static inline bool TrxInternTryEmplace(std::uint32_t hash, std::string_view str)
	{
		ZoneScoped;
		std::unique_lock lock(_trx_id_interner_lock);
		auto [_, inserted] = _trx_id_interner_map.try_emplace(hash, str);
		return inserted;
	}

	static inline void TrxInternEmitValue(std::uint32_t hash, std::string_view str)
	{
		ZoneScoped;
		// Emiting new intern table values needs to happen ASAP
		// the clients need to know what the newly received id's stand for
		TrxInternNewValueEvent event {
			._id = hash,
			._str = str
		};
		EvtEmit("mxtrace::intern_newval", reinterpret_cast<std::uint8_t*>(&event), sizeof(TrxInternNewValueEvent));
	}

	// NOTE: (César) Hash collision is "handled" on the display frontend
	static void TrxInternStringHash(std::uint32_t hash, std::string_view str)
	{
		ZoneScoped;
		if(!TrxInternMapSafeFindHash(hash)) [[unlikely]]
		{
			if(TrxInternTryEmplace(hash, str))
			{
				TrxInternEmitValue(hash, str);
			}
			else
			{
				LogError("[mxtrace] TrxInternStringHash: Failed to intern <%s>. Hash collision.", str.data());
			}
		}
	}

	static TrxRecordWithMeta TrxGenerateRecordWithMeta(TrxTag tags, std::uint64_t rid, TrxFuncId fid, const char* str)
	{
		ZoneScoped;
		return TrxRecordWithMeta {
			._record = {
				._self_rid = rid,
				._self_cid = SysGetClientId(),

				._trigger_rid = 0x00, // TODO: (César)
				._trigger_cid = 0x00, // TODO: (César)

				._timestamp = SysGetCurrentTime(),
				._tags = tags,
				._fid  = fid
			},
			._meta = str
		};
	}

	static std::uint64_t TrxGetNextRecordId()
	{
		return _trx_record_id++;
	}

	static inline std::vector<std::uint8_t> TrxInternRecordsAndGetBuffer(const std::vector<TrxRecordWithMeta>& ibuffer)
	{
		_trx_emit_buffer.clear();
		for(const auto& irecord : ibuffer)
		{
			TrxInternStringHash(irecord._record._fid, irecord._meta);
			_trx_emit_buffer.push_back(irecord._record);
		}

		return SysPackArguments(
				std::uint64_t(_trx_emit_buffer.size()),
				std::vector<uint8_t>(
					reinterpret_cast<uint8_t*>(_trx_emit_buffer.data()),
					reinterpret_cast<uint8_t*>(_trx_emit_buffer.data() + _trx_emit_buffer.size())// * sizeof(TrxRecord)
				)
		);
	}

	static void TrxFlushQueue()
	{
		ZoneScoped;
		_trx_record_queue.flush(_trx_flush_buffer);
		if(!_trx_flush_buffer.empty())
		{
			std::vector<std::uint8_t> buffer = TrxInternRecordsAndGetBuffer(_trx_flush_buffer);
			EvtEmit("mxtrace::record", buffer.data(), buffer.size());
			_trx_flush_buffer.clear();
		}
	}

	static void TrxScheduleFlush()
	{
		ZoneScoped;
		if (!_trx_flush_pending.exchange(true, std::memory_order_acq_rel))
		{
			_trx_emit_io.schedule([](){
				_trx_flush_pending.store(false, std::memory_order_release);
				TrxFlushQueue();
			});
		}
	}

	static void TrxScheduleEmitRecord(TrxRecordWithMeta&& record)
	{
		ZoneScoped;

		std::int32_t retries = 0;
		while(!_trx_record_queue.enqueue(record) && retries < 3)
		{
			// Hard backpressure!!
			TrxScheduleFlush();
			std::this_thread::yield();
			mulex::LogWarning("[mxtrace] Hard backpressure detected. Consider a larger MCSP queue buffer.");
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
		ZoneScoped;
		EvtRegister("mxtrace::record");
		EvtRegister("mxtrace::intern_newval");
		_trx_flush_buffer.reserve(TRX_MPSCQ_SIZE);
		_trx_emit_buffer.reserve(TRX_MPSCQ_SIZE);

		// Stream only once every X ms
		_trx_emit_io.schedule(TrxFlushQueue, 0, TRX_STREAM_INTERVAL);
	}

	mulex::RPCGenericType TrxGetInternedMap()
	{
		std::shared_lock lock(_trx_id_interner_lock);

		std::uint64_t size = _trx_id_interner_map.size() * (sizeof(TrxInternNewValueEvent));
		std::vector<std::uint8_t> buffer;
		buffer.reserve(size);

		for(const auto& [fid, str] : _trx_id_interner_map)
		{
			SysPackArguments(buffer, TrxInternNewValueEvent {
				._id = fid,
				._str = str
			});
		}

		return buffer;
	}

	TrxScopeGuard::TrxScopeGuard(TrxTag tags, TrxFuncId id, const char* fname) : _tags(tags), _fid(id), _fname(fname), _rid(0)
	{
		ZoneScoped;
		TrxScheduleEmitRecord(TrxGenerateRecordWithMeta(_tags | TrxTag::TRX_START, _rid, _fid, _fname));
	}

	TrxScopeGuard::~TrxScopeGuard()
	{
		ZoneScoped;
		TrxScheduleEmitRecord(TrxGenerateRecordWithMeta(_tags | TrxTag::TRX_STOP, _rid, _fid, _fname));
	}
} // namespace mulex
