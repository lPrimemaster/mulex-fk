#pragma once
#include <cstdint>
#include <string>
#include <optional>

#ifdef _WIN32
#include <windows.h>
#endif

namespace mulex
{
	static constexpr std::uint16_t REX_PORT = 5703;
	static constexpr std::int64_t REX_TIMEOUT = 10000;

#ifdef __linux__
	using RexPid = pid_t;
	using RexLockHandle = int;
#else
	using RexPid = DWORD;
	using RexLockHandle = HANDLE;
#endif

	enum class RexCommandStatus : std::uint8_t
	{
		BACKEND_START_OK,
		BACKEND_START_FAILED,
		BACKEND_STOP_OK,
		BACKEND_STOP_FAILED,
		NO_SUCH_BACKEND,
		NO_SUCH_HOST,
		NO_SUCH_COMMAND,
		COMMAND_TX_ERROR,
		COMMAND_TX_TIMEOUT
	};

	enum class RexOperation : std::uint8_t
	{
		BACKEND_START,
		BACKEND_STOP
	};

	struct RexCommand
	{
		std::uint64_t _backend;
		RexOperation _op;
	};

	struct RexClientInfo
	{
		std::uint64_t _cid;
		std::string   _bwd;
		std::string   _bin_path;
		std::string   _srv_host;
	};

	RexLockHandle RexAcquireLock();
	bool RexWriteLockFile();
	bool RexInterruptDaemon();
	void RexReleaseLock(RexLockHandle h);

	bool RexServerInit();
	void RexServerLoop();
	void RexServerStop();

	RexCommandStatus RexServerExecuteCommand(const RexCommand& command);

	bool RexUpdateHostsFile(std::uint64_t cid, const std::string& cltaddr);
	std::optional<std::string> RexFindBackendHost(std::uint64_t cid);

	bool RexCreateClientListFile();
	bool RexCreateClientInfo(std::uint64_t cid, const std::string& absbwd, const std::string& binpath, const std::string& srvaddr);
	bool RexUpdateClientInfo(std::uint64_t cid, const std::string& absbwd, const std::string& binpath, const std::string& srvaddr);
	bool RexDeleteClientInfo(std::uint64_t cid);
	std::optional<RexClientInfo> RexGetClientInfo(std::uint64_t cid);

	RexCommandStatus RexStartBackend(const RexClientInfo& cinfo);
	RexCommandStatus RexStopBackend(const RexClientInfo& cinfo);

	MX_RPC_METHOD mulex::RexCommandStatus RexSendStartCommand(std::uint64_t backend);
	MX_RPC_METHOD mulex::RexCommandStatus RexSendStopCommand(std::uint64_t backend);
}
