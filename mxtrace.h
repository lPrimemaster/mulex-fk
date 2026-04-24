#pragma once
#include "network/rpc.h"
#include <cstdint>

// NOTE: (César) name must be known at compile-time
#define TrxTargetTags(group, name, tags) \
	static constexpr const char* __trx_fname { TrxStaticStringAssert(group ":" name) }; \
	const TrxScopeGuard __trx_target(tags, SysFastHashConstEval(__trx_fname), __trx_fname);

#define TrxTarget(group, name) \
	static constexpr const char* __trx_fname { TrxStaticStringAssert(group ":" name) }; \
	const TrxScopeGuard __trx_target(TrxTag::NONE, SysFastHashConstEval(__trx_fname), __trx_fname);

namespace mulex
{
	// NOTE: (César) Mental notes on tracing
	// - [x] Tags for filtering
	// - [x] Time ordering (relatively easy)
	// - [ ] Causality (complex) figure out best way to implement
	// - [x] Passing the record data around threads
	// - [ ] Passing the record data around backends
	// - [x] Tracing should have a relatively low impact
	// - [x] String interning

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

		TrxTag        _tags;
		std::uint8_t  _padding[3];
		TrxFuncId	  _fid;
	};

	// Use RAII to manage trx record transfers
	// NOTE: (César) There are improvements possible here
	// 				 However, this runs with ctor/dtor
	// 				 P90 timings of ~200 ns on my system
	// 				 which is more than enough for us
	class TrxScopeGuard
	{
	public:
		[[nodiscard]] TrxScopeGuard(TrxTag tags, TrxFuncId id, const char* fname);
		~TrxScopeGuard();

		// No copy
		TrxScopeGuard(const TrxScopeGuard&) = delete;
		TrxScopeGuard& operator=(const TrxScopeGuard&) = delete;

	private:
		TrxTag    	  	 _tags;
		TrxFuncId 	  	 _fid;
		std::uint64_t 	 _rid;
		const char*		 _fname;
	};

	template<std::size_t N>
	consteval auto& TrxStaticStringAssert(char const (&str)[N])
	{
		// We use string128 for passing data around in events
		static_assert(N < 128, "TrxStaticStringAssert: cannot use descriptors with more than 128 chars.");
		return str;
	}

	void TrxInit();
	void TrxClose();

	MX_RPC_METHOD mulex::RPCGenericType TrxGetInternedMap();
} // namespace mulex
