#include "../mxsystem.h"
#include "../mxlogger.h"
#include "mxrexs.h"
#include <functional>
#include <thread>

using namespace mulex;

static std::function<void()> _rex_service_init;
static std::function<void()> _rex_service_loop;

#ifdef _WIN32
#include <windows.h>
#include <tchar.h>

#define SVCNAME TEXT("mxrexs")

static SERVICE_STATUS _service_status = {};
static SERVICE_STATUS_HANDLE _status_handle = nullptr;
static HANDLE _service_stop_evt = nullptr;

// Log error on Windows event log
void RexReportEvent(LPTSTR func)
{
	HANDLE source;
	LPCTSTR strs[2];
	TCHAR buffer[80];

	source = RegisterEventSource(NULL, SVCNAME);

	if(source != NULL)
	{
		StringCchPrintf(buffer, 80, TEXT("%s failed with %d"), func, GetLastError());

		strs[0] = SVCNAME;
		strs[1] = buffer;

		ReportEvent(
			source,
			EVENTLOG_ERROR_TYPE,
			0,
			SVC_ERROR,
			NULL,
			2,
			0,
			strs,
			NULL
		);

		DeregisterEventSource(source);
	}
}

void WINAPI ServiceCtrlHandler(DWORD ctrlcode)
{
	switch(ctrlcode)
	{
		case SERVICE_CONTROL_STOP:
		{
			_service_status.dwCurrentState = SERVICE_STOP_PENDING;
			SetServiceStatus(_status_handle, &_service_status);
			SetEvent(_service_stop_evt);
			break;
		}
		default:
		{
			break;
		}
	}
}

void WINAPI ServiceMain(DWORD argc, LPTSTR* argv)
{
	_status_handle = RegisterServiceCtrlHandler(SVCNAME, ServiceCtrlHandler);

	if(!_status_handle)
	{
		RexReportEvent(TEXT("RegisterServiceCtrlHandler"));
		return;
	}

	_service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	_service_status.dwServiceSpecifigExitCode = 0;
	_service_status.dwControlsAccepted = SERVICE_ACCEPT_STOP;
	_service_status.dwCurrentState = SERVICE_START_PENDING;
	SetServiceStatus(_status_handle, &_service_status);

	_service_stop_evt = CreateEvent(NULL, TRUE, FALSE, NULL);
	if(!_service_stop_evt)
	{
		_service_status.dwCurrentState = SERVICE_STOPPED;
		SetServiceStatus(_status_handle, &_service_status);
		return;
	}

	_service_status.dwCurrentState = SERVICE_RUNNING;
	SetServiceStatus(_status_handle, &_service_status);

	// Service init
	_rex_service_init();

	while(WaitForSingleObject(_service_stop_evt, INFINITE) != WAIT_OBJECT_0)
	{
		// Service is running
		_rex_service_loop();
	}

	_service_status.dwCurrentState = SERVICE_STOPPED;
	SetServiceStatus(_status_handle, &_service_status);
}

bool ServiceInstall()
{
	SC_HANDLE hscm;
	SC_HANDLE hservice;
	TCHAR path[MAX_PATH];

	if(!GetModuleFileName(NULL, path, MAX_PATH))
	{
		LogError("[mxrexs] Cannot install service (%d).", GetLastError());
		return false;
	}

	TCHAR qpath[MAX_PATH];
	StringCbPrintf(qpath, MAX_PATH, TEXT("\"%s\""), path);

	hscm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if(!hscm)
	{
		LogError("[mxrexs] OpenSCManager failed (%d).", GetLastError());
		return false;
	}

	hservice = CreateService(
		hscm,
		SVCNAME,
		SVCNAME,
		SERVICE_ALL_ACCESS,
		SERVICE_WIN32_OWN_PROCESS,
		SERVICE_AUTO_START,
		SERVICE_ERROR_NORMAL,
		qpath,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL
	);

	if(!hservice)
	{
		DWORD err = GetLastError();
		if(err == ERROR_SERVICE_EXISTS)
		{
			LogError("[mxrexs] CreateService failed (%d).", err);
			CloseServiceHandle(hscm);
			return false;
		}

		// Service already exists
		// Silently ignore
		LogTrace("[mxrexs] Service already exists. Ignoring...");
	}

	CloseServiceHandle(hservice);
	CloseServiceHandle(hscm);
	return true;
}

#endif

static bool RexStartBackgroundDaemon()
{
#ifdef __linux__
	// On linux just make this process a daemon and continue
	LogMessage("[mxrexs] Rexs daemon started.");
	if(!SysDaemonize())
	{
		LogError("[mxrexs] Daemonize failed or not available on this system.");
		LogDebug("[mxrexs] Aborting execution.");
		return false;
	}

	return true;
#else
	if(!ServiceInstall())
	{
		LogError("[mxrexs] ServiceInstall failed.");
		LogDebug("[mxrexs] Aborting execution.");
		return false;
	}
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

static void RexRunService()
{
#ifdef __linux__
	// This is daemonized already just run as normal
	if(!RexWriteLockFile())
	{
		return;
	}
	int lock = RexAcquireLock();

	if(lock == -1)
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

	LogMessage("[mxrexs] ctrl-C detected. Exiting...");

	RexReleaseLock(lock);

#else
	// On Windows start the service
	SERVICE_TABLE_ENTRY dispatch_table[] = {
		{ SVCNAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain },
		{ NULL, NULL }
	};

	if(!StartServiceCtrlDispatcher(dispatch_table))
	{
		RexReportEvent(TEXT("StartServiceCtrlDispatcher"));
	}
#endif
}

enum class RexCommand
{
	START,
	STOP,
	UNDEFINED
};

int main(int argc, char* argv[])
{
	RexCommand cmd = RexCommand::UNDEFINED;
	SysAddArgument("start", 0, false, [&](const std::string&){
		if(cmd == RexCommand::STOP)
		{
			LogError("[mxrexs] Cannot specify --start and --stop at the same time.");
			::exit(0);
		}

		cmd = RexCommand::START;
	}, "Starts the mxrexs background service.");

	SysAddArgument("stop", 0, false, [&](const std::string&){
		if(cmd == RexCommand::START)
		{
			LogError("[mxrexs] Cannot specify --start and --stop at the same time.");
			::exit(0);
		}

		cmd = RexCommand::STOP;
	}, "Stops the mxrexs background service.");

	if(!SysParseArguments(argc, argv))
	{
		LogError("[mxrexs] Failed to parse arguments.");
		return 0;
	}

	// All arguments OK at this point
	LogTrace("[mxrexs] Arguments OK.");

	if(cmd == RexCommand::START)
	{
		if(!RexStartBackgroundDaemon())
		{
			return 0;
		}
	}
	else if(cmd == RexCommand::STOP)
	{
#ifdef __linux__
		if(!RexInterruptDaemon())
		{
			LogDebug("[mxrexs] Aborting execution.");
			return 0;
		}
		LogMessage("[mxrexs] Rexs daemon stopped.");
		return 0;
#else
		// TODO: (Cesar) Windows service stop
		LogMessage("[mxrexs] Rexs service stopped.");
		return 0;
#endif
	}
	else
	{
#ifdef __linux__
		LogWarning("[mxrexs] The service is running on this shell.");
		LogWarning("[mxrexs] Setting up background via '--start' is preferred.");
#endif
		// Otherwise the service runs via scm on windows
	}

	RexInlineServiceInitFunction([]() {
		LogDebug("Init");
	});

	RexInlineServiceLoopFunction([]() {
		LogDebug("Loop");
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	});

	RexRunService();
	return 0;
}
