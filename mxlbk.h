#include "mxrdb.h"
#include "network/rpc.h"

namespace mulex
{
	void LbkInit();
	void LbkClose();

	MX_RPC_METHOD MX_PERMISSION("write_entry") bool LbkPostCreate(mulex::PdbString title, mulex::RPCGenericType body, mulex::RPCGenericType meta);
	MX_RPC_METHOD MX_PERMISSION("read_entry") mulex::RPCGenericType LbkPostRead(std::int32_t id);
	MX_RPC_METHOD mulex::RPCGenericType LbkGetEntriesPageSearch(mulex::PdbString query, std::uint64_t limit, std::uint64_t page);
	MX_RPC_METHOD mulex::RPCGenericType LbkGetEntriesPage(std::uint64_t limit, std::uint64_t page);
	MX_RPC_METHOD std::int64_t LbkGetNumEntriesWithCondition(mulex::PdbString query);
} // namespace mulex
