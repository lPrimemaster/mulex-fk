#include "../mxsystem.h"
#include "../mxlogger.h"
#include "mxrexs.h"
#include <functional>
#include <minwinbase.h>
#include <thread>
#include <winbase.h>

using namespace mulex;

static std::function<void()> _rex_service_init;
static std::function<void()> _rex_service_loop;
static std::function<void()> _rex_service_cleanup;

#ifdef _WIN32
#include <windows.h>
#include <tchar.h>
#include <strsafe.h>

#define SVCNAME TEXT("mxrexs")

static SERVICE_STATUS _service_status = {};
static SERVICE_STATUS_HANDLE _status_handle = nullptr;
static HANDLE _service_stop_evt = nullptr;

#endif

static bool RexStartBackgroundDaemon(const char* self)
{
#ifdef __linux__
	// On Linux just make this process a daemon and continue
	(void)self;
	if(!SysDaemonize())
	{
		LogError("[mxrexs] Daemonize failed or not available on this system.");
		LogDebug("[mxrexs] Aborting execution.");
		return false;
	}

	LogMessage("[mxrexs] Rexs daemon started.");
	return true;
#else
	// On Windows spawn this process in the background without arguments
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	ZeroMemory(&pi, sizeof(pi));

	if(!CreateProcess(
		NULL, const_cast<char*>(self), NULL, NULL, FALSE,
		CREATE_NO_WINDOW | DETACHED_PROCESS,
		NULL, NULL, &si, &pi
	))
	{
		LogError("[mxrexs] Failed to start rexs daemon.");
		return false;
	}

	LogMessage("[mxrexs] Rexs daemon started.");
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return true;
#endif
}

static inline void RexInlineServiceInitFunction(const std::function<void()>& f)
{
	_rex_service_init = f;
}

static inline void RexInlineServiceLoopFunction(const std::function<void()>& f)
{
	_rex_service_loop = f;
}

static inline void RexInlineServiceCleanupFunction(const std::function<void()>& f)
{
	_rex_service_cleanup = f;
}

static void RexRunService()
{
	// This is daemonized already just run as normal
	if(!RexWriteLockFile())
	{
		return;
	}
	RexLockHandle lock = RexAcquireLock();

#ifdef __linux__
	if(lock == -1)
#else
	if(lock == INVALID_HANDLE_VALUE)
#endif
	{
		LogError("[mxrexs] Failed to acquire file lock.");
		LogDebug("[mxrexs] Aborting execution.");
		return;
	}

	_rex_service_init();

	const std::atomic<bool>* stop = SysSetupExitSignal();

	while(!*stop)
	{
		_rex_service_loop();
	}

	_rex_service_cleanup();

	LogMessage("[mxrexs] ctrl-C detected. Exiting...");

	RexReleaseLock(lock);
}

enum class RexCLICommand
{
	START,
	STOP,
	UNDEFINED
};

int main(int argc, char* argv[])
{
	RexCLICommand cmd = RexCLICommand::UNDEFINED;
	SysAddArgument("start", 0, false, [&](const std::string&){
		if(cmd == RexCLICommand::STOP)
		{
			LogError("[mxrexs] Cannot specify --start and --stop at the same time.");
			::exit(0);
		}

		cmd = RexCLICommand::START;
	}, "Starts the mxrexs background service.");

	SysAddArgument("stop", 0, false, [&](const std::string&){
		if(cmd == RexCLICommand::START)
		{
			LogError("[mxrexs] Cannot specify --start and --stop at the same time.");
			::exit(0);
		}

		cmd = RexCLICommand::STOP;
	}, "Stops the mxrexs background service.");

	if(!SysParseArguments(argc, argv))
	{
		LogError("[mxrexs] Failed to parse arguments.");
		return 0;
	}

	// All arguments OK at this point
	LogTrace("[mxrexs] Arguments OK.");

	if(cmd == RexCLICommand::START)
	{
		if(!RexStartBackgroundDaemon(argv[0]))
		{
			return 0;
		}
#ifdef _WIN32
		// No fork on Windows so the child process starts over
		return 0;
#endif
	}
	else if(cmd == RexCLICommand::STOP)
	{
		if(!RexInterruptDaemon())
		{
			LogDebug("[mxrexs] Aborting execution.");
			return 0;
		}
		LogMessage("[mxrexs] Rexs daemon stopped.");
		return 0;
	}
	else
	{
		LogWarning("[mxrexs] The service is running on this shell.");
		LogWarning("[mxrexs] Setting up background via '--start' is preferred.");
	}

	RexInlineServiceInitFunction([]() {
		LogDebug("[mxrexs] Service Init.");
		if(!RexServerInit())
		{
			LogError("[mxrexs] Failed to init rexs server.");
		}
	});

	RexInlineServiceLoopFunction([]() {
		RexServerLoop();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	});

	RexInlineServiceCleanupFunction([]() {
		LogDebug("[mxrexs] Service Stop.");
		RexServerStop();
	});

	RexRunService();
	return 0;
}
