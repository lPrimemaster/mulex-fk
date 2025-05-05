#include "mxrexs.h"
#include "../mxsystem.h"
#include "../mxlogger.h"
#include <cerrno>
#include <fstream>

#ifdef __linux__
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#endif

namespace mulex
{

	static std::string RexGetLockFilePath()
	{
		std::string cache_dir(SysGetCacheDir());
		if(cache_dir.empty())
		{
			return "";
		}

		return cache_dir + "/rexs.lock";
	}

#ifdef __linux__
	static bool RexPidIsRunning(pid_t pid)
	{
		std::string proc_file = "/proc/" + std::to_string(pid);
		struct stat st;
		return stat(proc_file.c_str(), &st) == 0;
	}

	static int RexGetDaemonPid()
	{
		const std::string lock_path = RexGetLockFilePath();

		if(lock_path.empty())
		{
			LogError("[mxrexs] RexGetLockFilePath() failed.");
			LogDebug("[mxrexs] Aborting execution.");
			return -1;
		}

		std::ifstream lock_file(lock_path);
		if(lock_file)
		{
			pid_t pid;
			lock_file >> pid;

			if(RexPidIsRunning(pid))
			{
				return pid;
			}
		}

		return -1;
	}
#endif

	bool RexWriteLockFile()
	{
#ifdef __linux__
		const std::string lock_path = RexGetLockFilePath();

		if(lock_path.empty())
		{
			LogError("[mxrexs] RexGetLockFilePath() failed.");
			LogDebug("[mxrexs] Aborting execution.");
			return false;
		}

		{
			std::ifstream lock_file(lock_path);
			if(lock_file)
			{
				pid_t pid;
				lock_file >> pid;

				if(RexPidIsRunning(pid))
				{
					LogError("[mxrexs] Daemon already running [pid = %d].", pid);
					LogDebug("[mxrexs] Aborting execution.");
					return false;
				}

				LogDebug("[mxrexs] Stale lock file. Overwriting...");
			}
		}
		
		std::ofstream lock_file(lock_path, std::ios::trunc);
		lock_file << getpid() << "\n";

		LogTrace("[mxrexs] RexWriteLockFile() OK.");
		return true;
#else
		LogError("[mxrexs] Windows service does not require a lock file.");
		return false;
#endif
	}

	int RexAcquireLock()
	{
#ifdef __linux__
		int fd = ::open(RexGetLockFilePath().c_str(), O_RDWR, 0666);
		if(fd == -1)
		{
			LogError("[mxrexs] Failed to open lock file.");
			return -1;
		}

		if(::flock(fd, LOCK_EX | LOCK_NB) == -1)
		{
			LogError("[mxrexs] Already running. Cannot acquire lock.");
			return -1;
		}

		return fd;
#else
		LogError("[mxrexs] Windows service does not require locking.");
		return false;
#endif
	}

	void RexReleaseLock(int fd)
	{
#ifdef __linux__
		::close(fd);
		LogTrace("[mxrexs] RexReleaseLock() OK.");
#endif
	}

	bool RexInterruptDaemon()
	{
#ifdef __linux__
		pid_t pid = RexGetDaemonPid();
		if(pid == -1)
		{
			LogError("[mxrexs] Could not find mxrexs daemon PID. Maybe not running?");
			return false;
		}
		
		if(::kill(pid, SIGINT) == 0)
		{
			LogTrace("[mxrexs] Sent SIGINT to daemon <%d>.", pid);
			return true;
		}

		LogError("[mxrexs] Failed to interrupt daemon. Code = %d.", errno);
		return false;
#endif
		return false;
	}
}
