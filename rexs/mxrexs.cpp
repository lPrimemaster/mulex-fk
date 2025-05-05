#include "mxrexs.h"
#include "../mxsystem.h"
#include "../mxlogger.h"
#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>

#ifdef __linux__
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#endif

#ifdef __linux__
static int _rex_file_lock_fd;
#else
static HANDLE _rex_file_lock_fd;
#endif

static mulex::Socket _server_socket;

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

	bool RexServerInit()
	{
		ZoneScoped;
		_server_socket = SocketInit();
		SocketBindListen(_server_socket, REX_PORT);
		if(!SocketSetNonBlocking(_server_socket))
		{
			LogError("Failed to set listen socket to non blocking mode.");
			SocketClose(_server_socket);
			return false;
		}
		return true;
	}

	void RexServerStop()
	{
		SocketClose(_server_socket);
	}

	static std::optional<RexCommand> RexServerProcessRequest(const Socket& client)
	{
		static std::uint8_t buffer[sizeof(RexCommand)];
		std::uint64_t rlen, tsize = 0;
		while(tsize < sizeof(RexCommand))
		{
			SocketResult res = SocketRecvBytes(client, &buffer[tsize], sizeof(RexCommand) - tsize, &rlen);
			tsize += rlen;

			if(res == SocketResult::ERROR || res == SocketResult::DISCONNECT)
			{
				LogError("[mxrexs] Could not process request.");
				return std::nullopt;
			}
		}

		RexCommand command;
		std::memcpy(&command, buffer, sizeof(RexCommand));
		return command;
	}

	RexCommandStatus RexServerExecuteCommand(const RexCommand& command)
	{
		switch(command._op)
		{
			case RexOperation::BACKEND_START:
			{
				std::optional<RexClientInfo> cinfo = RexGetClientInfo(command._backend);
				if(cinfo.has_value())
				{
					return RexStartBackend(cinfo.value());
				}
			}
			case RexOperation::BACKEND_STOP:
			{
				std::optional<RexClientInfo> cinfo = RexGetClientInfo(command._backend);
				if(cinfo.has_value())
				{
					return RexStopBackend(cinfo.value());
				}
			}
		}

		return RexCommandStatus::NO_SUCH_COMMAND;
	}

	void RexServerLoop()
	{
		bool would_block;
		Socket client = SocketAccept(_server_socket, &would_block);
		if(would_block)
		{
			return;
		}

		// NOTE: (Cesar)
		// New client connection to this Rex server
		// await comm message with timeout
		if(!client._error)
		{
			std::optional<RexCommand> cmd = RexServerProcessRequest(client);

			if(cmd.has_value())
			{
				RexCommandStatus status = RexServerExecuteCommand(cmd.value());
			}

			SocketClose(client);
		}
	}

	static std::string RexGetClientListFilePath()
	{
		static std::string cache_dir(SysGetCacheDir());
		if(cache_dir.empty())
		{
			return "";
		}

		static std::string rex_path = cache_dir + "/rex.bin";
		return rex_path;
	}

	static std::string RexCreateEntryString(std::uint64_t cid, const std::string& absbwd, const std::string& binpath, const std::string& srvaddr)
	{
		std::ostringstream ss;
		// NOTE: (Cesar) Cids, workdirs and server addresses cannot contain '|' (good spacer here)
		ss << SysI64ToHexString(cid) << "|" << absbwd << "|" << binpath << "|" << srvaddr << "\n";
		return ss.str();
	}

	// NOTE: (Cesar) Multiple processes might want to read/write on the file
	static bool RexLockFile()
	{
#ifdef __linux__
		_rex_file_lock_fd = ::open((RexGetClientListFilePath() + ".lock").c_str(), O_CREAT | O_RDWR, 0666);
		if(::flock(_rex_file_lock_fd, LOCK_EX) == -1)
		{
			return false;
		}
		return true;
#else
		const std::string path = RexGetClientListFilePath();
		_rex_file_lock_fd = CreateFile((path + ".lock").c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		
		OVERLAPPED overlapped = { 0 };
		return LockFileEx(_rex_file_lock_fd, LOCKFILE_EXCLUSIVE_LOCK, 0, MAXDWORD, MAXDWORD, &overlapped) == TRUE;
#endif
	}

	static bool RexUnlockFile()
	{
#ifdef __linux__
		::flock(_rex_file_lock_fd, LOCK_UN);
		::close(_rex_file_lock_fd);
		return true;
#else
		OVERLAPPED overlapped = { 0 };
		return UnlockFileEx(_rex_file_lock_fd, 0, MAXDWORD, MAXDWORD, &overlapped) == TRUE;
#endif
	}

	static bool RexLineHasCid(std::uint64_t cid, const std::string& line)
	{
		auto idx = line.find_first_of('|');
		std::uint64_t fcid = std::stoull(line.substr(0, idx), 0, 16);
		return fcid == cid;
	}

	static void RexWriteBufferToFile(std::ostringstream& ss, const std::string& path)
	{
		std::ofstream file(path, std::ios::trunc);

		if(!file)
		{
			LogError("[mxrexs] Failed to open <rex.bin> file.");
			return;
		}
		file << ss.str();
	}

	struct RexLockGuard
	{
		RexLockGuard()
		{
			if(!RexLockFile())
			{
				LogError("[mxrexs] RexLockGuard failed to lock file.");
				return;
			}

			_locked = true;
		}

		~RexLockGuard()
		{
			if(_locked && !RexUnlockFile())
			{
				LogError("[mxrexs] RexLockGuard failed to unlock file.");
			}
		}

		bool _locked = false;
	};

	bool RexCreateClientListFile()
	{
		RexLockGuard lock;
		std::string rex_path = RexGetClientListFilePath();
		if(!std::filesystem::exists(rex_path))
		{
			std::ofstream file(rex_path);
			if(!file)
			{
				LogError("[mxrexs] Failed to create <rex.bin> file.");
				return false;
			}
		}

		LogTrace("[mxrexs] RexCreateClientListFile() OK.");
		return true;
	}

	bool RexCreateClientInfo(std::uint64_t cid, const std::string& absbwd, const std::string& binpath, const std::string& srvaddr)
	{
		RexLockGuard lock;
		std::string path = RexGetClientListFilePath();
		std::ofstream file(path, std::ios::app);

		if(!file)
		{
			LogError("[mxrexs] Failed to open <rex.bin> file.");
			return false;
		}

		file << RexCreateEntryString(cid, absbwd, binpath, srvaddr);
		file << std::flush;
		return true;
	}

	bool RexUpdateClientInfo(std::uint64_t cid, const std::string& absbwd, const std::string& binpath, const std::string& srvaddr)
	{
		RexLockGuard lock;
		std::string path = RexGetClientListFilePath();
		std::ostringstream ss;

		std::ifstream file(path, std::ios::in);
		bool found = false;

		if(!file)
		{
			LogError("[mxrexs] Failed to open <rex.bin> file.");
			return false;
		}

		std::string line;
		while(std::getline(file, line))
		{
			if(RexLineHasCid(cid, line))
			{
				ss << RexCreateEntryString(cid, absbwd, binpath, srvaddr);
				found = true;
			}
			else
			{
				ss << line << "\n";
			}
		}

		if(!found)
		{
			LogDebug("[mxrexs] Failed to find cid entry <0x%llx> on file.", cid);
			return false;
		}

		RexWriteBufferToFile(ss, path);
		return true;
	}

	bool RexDeleteClientInfo(std::uint64_t cid)
	{
		RexLockGuard lock;
		std::string path = RexGetClientListFilePath();
		std::ostringstream ss;

		std::ifstream file(path, std::ios::in);
		bool found = false;

		if(!file)
		{
			LogError("[mxrexs] Failed to open <rex.bin> file.");
			return false;
		}

		std::string line;
		while(std::getline(file, line))
		{
			if(RexLineHasCid(cid, line))
			{
				// NOTE: (Cesar) Ellipsis line
				found = true;
			}
			else
			{
				ss << line << "\n";
			}
		}

		if(!found)
		{
			LogError("[mxrexs] Failed to find cid entry <0x%llx> on file.", cid);
			return false;
		}

		RexWriteBufferToFile(ss, path);
		return true;
	}

	static RexClientInfo RexParseClientInfoLine(const std::string& line)
	{
		std::istringstream ss(line);
		std::string token;
		RexClientInfo cinfo;

		std::getline(ss, token, '|');
		cinfo._cid = std::stoull(token, 0, 16);

		std::getline(ss, token, '|');
		cinfo._bwd = token;

		std::getline(ss, token, '|');
		cinfo._bin_path = token;
		
		std::getline(ss, token, '|');
		cinfo._srv_host = token;

		return cinfo;
	}

	std::optional<RexClientInfo> RexGetClientInfo(std::uint64_t cid)
	{
		RexLockGuard lock;
		std::string path = RexGetClientListFilePath();
		std::ifstream file(path, std::ios::in);
		if(!file)
		{
			LogError("[mxrexs] Failed to open <rex.bin> file.");
			return std::nullopt;
		}

		std::string line;
		while(std::getline(file, line))
		{
			if(RexLineHasCid(cid, line))
			{
				return RexParseClientInfoLine(line);
			}
		}

		LogError("[mxrexs] Could not find client <0x%llx> on rex file.", cid);
		return std::nullopt;
	}

	RexCommandStatus RexStartBackend(const RexClientInfo& cinfo)
	{
		std::vector<std::string> args;

		args.push_back("--server");
		args.push_back(cinfo._srv_host);

		if(!SysSpawnProcess(cinfo._bin_path, cinfo._bwd, args))
		{
			return RexCommandStatus::BACKEND_START_FAILED;
		}
		return RexCommandStatus::BACKEND_START_OK;
	}

	RexCommandStatus RexStopBackend(const RexClientInfo& cinfo)
	{
	}

	RexCommandStatus RexSendStartCommand(std::uint64_t backend, const std::string& host)
	{
	}

	RexCommandStatus RexSendStopCommand(std::uint64_t backend, const std::string& host)
	{
	}
}
