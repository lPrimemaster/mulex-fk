#include <cstdint>
#include <string>

namespace mulex
{
	static constexpr std::uint16_t REX_PORT = 5703;

	class RexServerThread
	{
	};

	bool RexWriteLockFile();
	int RexAcquireLock();
	void RexReleaseLock(int fd);
	bool RexInterruptDaemon();

	bool RexCreateClientListFile();
	bool RexCreateClientInfo(std::int64_t cid, const std::string& absbwd, const std::string& srvaddr);
	bool RexUpdateClientInfo(std::int64_t cid, const std::string& absbwd, const std::string& srvaddr);
	bool RexDeleteClientInfo(std::int64_t cid);
}
