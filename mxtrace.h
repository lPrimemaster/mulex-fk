#pragma once
#include <cstdint>
#include <string_view>

#define TrxTarget(type, tags, name) \
	const TrxScopeGuard __trx_target(type, tags, TrxInternStringHash(SysFastHashConstEval(std::string_view(name)), std::string_view(name)));

namespace mulex
{
	// NOTE: (César) Mental notes on tracing
	// - [ ] Tags for filtering
	// - [ ] Time ordering (relatively easy)
	// - [ ] Causality (complex) figure out best way to implement
	// - [ ] Passing the record data around threads
	// - [ ] Passing the record data around backends
	// - [ ] Tracing should have a relatively low impact
	// - [ ] String interning
	
	// Record types
	enum class TrxType : std::uint8_t
	{
		RPC,
		RDB,
		EVT
	};

	using TrxFuncId = std::uint32_t;

	// Record tags as bit flags
	// Compact way of sharing low impact metadata
	enum class TrxTag : std::uint8_t
	{
		NONE	  = 0,		  // To refer to no tags
		TRX_START = (1 << 0), // To make clear if this Record is a start or a stop
		TRX_STOP  = (1 << 1), // To make clear if this Record is a start or a stop
		DEFERRED  = (1 << 2), // This record's referred operation execution was deferred by the caller
		SYSTEM	  = (1 << 3)  // This record was triggered by a system operation
	};

	TrxTag operator&  (TrxTag  	  a, TrxTag b);
	TrxTag operator&  (std::uint8_t a, TrxTag b);

	TrxTag operator|  (TrxTag 	  a, TrxTag b);
	TrxTag operator|  (std::uint8_t a, TrxTag b);

	TrxTag operator&= (std::uint8_t a, TrxTag b);
	TrxTag operator|= (std::uint8_t a, TrxTag b);

	TrxTag operator~  (TrxTag a);

	// NOTE: (César)
	// 48B Alignment (8)
	// If perfomance acts up here, we want to compress cids and use smaller rids
	struct alignas(8) TrxRecord
	{
		std::uint64_t _self_rid;
		std::uint64_t _self_cid;
		std::uint64_t _trigger_rid;
		std::uint64_t _trigger_cid;

		std::int64_t  _timestamp;

		TrxType		  _type;
		TrxTag        _tags;
	};

	// Use RAII to manage trx record transfers
	class TrxScopeGuard
	{
	public:
		[[nodiscard]] TrxScopeGuard(TrxType type, TrxTag tags, TrxFuncId id);
		~TrxScopeGuard();

		// No copy
		TrxScopeGuard(const TrxScopeGuard&) = delete;
		TrxScopeGuard& operator=(const TrxScopeGuard&) = delete;

	private:
		TrxType   	  _type;
		TrxTag    	  _tags;
		TrxFuncId 	  _fid;
		std::uint64_t _rid;
	};

	// String interner to store metadata for each record function
	TrxFuncId TrxInternStringHash(std::uint32_t hash, std::string_view str);

	TrxRecord TrxGenerateRecord(TrxType type, TrxTag tags, std::uint64_t rid);
	void TrxInit();
} // namespace mulex
