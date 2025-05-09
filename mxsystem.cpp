#include "mxsystem.h"
#include <chrono>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <signal.h>
#include "mxlogger.h"
#include "network/rpc.h"
#include "mxevt.h"
#include "mxrdb.h"
#include "mxhttp.h"
#include "mxrun.h"
#include "mxmsg.h"
#include "rexs/mxrexs.h"
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <minwinbase.h>
#include <windows.h>
#include <shlwapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#else
#include <fcntl.h>
#include <sys/sysinfo.h>
#include <sys/file.h>
#include <unistd.h>
#endif

#include <tracy/Tracy.hpp>
#include <mxconfig.h>

static mulex::Experiment _sys_experiment;
static bool _sys_experiment_connected = false;
static std::string _mxcachedir;
static std::string _mxcachelockdir;
static std::string _sys_expname;
static std::string _sys_binname;
static std::string _sys_fullbinname;
static std::string _sys_hostname;
static std::unique_ptr<mulex::RPCServerThread> _sys_rpc_thread;
static std::unique_ptr<mulex::EvtServerThread> _sys_evt_thread;
static std::map<char, std::function<void(const std::string&)>> _sys_argscmd_short;
static std::map<char, bool> _sys_argscmd_short_reqarg;
static std::map<std::string, std::function<void(const std::string&)>> _sys_argscmd_long;
static std::map<std::string, bool> _sys_argscmd_long_reqarg;
static std::map<std::string, std::string> _sys_argscmd_helptxt;
static bool _sys_isdaemon = false;
static std::uint64_t _sys_cid = 0x00;
static std::int64_t _sys_uptime_mark;

static std::unique_ptr<std::thread> _sys_performance_metrics_thread;
static std::atomic<bool> _sys_performance_metrics_running;
static constexpr std::uint64_t SYS_PERF_GATHER_INTERVAL = 1000;

#ifdef __linux__
static int _sys_lock_handle;
#else
static HANDLE _sys_lock_handle;
#endif

namespace mulex
{
	static void SysAddArgumentI(const std::string& longname, const char shortname, bool needvalue, std::function<void(const std::string&)> action, const std::string& helptxt)
	{
		if(shortname != 0)
		{
			_sys_argscmd_short[shortname] = action;
			_sys_argscmd_short_reqarg[shortname] = needvalue;
			_sys_argscmd_helptxt[longname + ":" + std::string(1, shortname)] = helptxt;
		}
		else
		{
			_sys_argscmd_helptxt[longname] = helptxt;
		}
		_sys_argscmd_long[longname] = action;
		_sys_argscmd_long_reqarg[longname] = needvalue;
	}

	void SysAddArgument(const std::string& longname, const char shortname, bool needvalue, std::function<void(const std::string&)> action, const std::string& helptxt)
	{
		if(longname == "help" || shortname == 'h')
		{
			LogError("SysAddArgument: Failed to add a new argument: <help/h> is a reserved argument name.");
			return;
		}

		if(longname == "daemon" || shortname == 'D')
		{
			LogError("SysAddArgument: Failed to add a new argument: <daemon/D> is a reserved argument name.");
			return;
		}

		if(_sys_argscmd_long.find(longname) != _sys_argscmd_long.end())
		{
			LogError("SysAddArgument: Argument named <%s> already exists.", longname.c_str());
			return;
		}

		if(shortname != 0)
		{
			if(_sys_argscmd_short.find(shortname) != _sys_argscmd_short.end())
			{
				LogError("SysAddArgument: Argument named <%c> already exists.", shortname);
				return;
			}
		}

		SysAddArgumentI(longname, shortname, needvalue, action, helptxt);
	}

	bool SysParseArguments(int argc, char* argv[])
	{
		SysAddArgumentI("help", 'h', false, [](const std::string&){
			std::cout << "Arguments help:" << std::endl;
			for(const auto& arg : _sys_argscmd_helptxt)
			{
				bool no_short = (arg.first.find(":") == std::string::npos);
				std::string longname  = arg.first.substr(0, arg.first.find(":"));
				if(no_short)
				{
					std::cout << "\t\t--" << longname << std::endl;
				}
				else
				{
					std::string shortname = arg.first.substr(arg.first.find(":")+1);
					std::cout << "\t-" << shortname << "\t--" << longname << std::endl;
				}

				if(arg.second.empty())
				{
					std::cout << "\t\t" << "<No description provided>" << std::endl;
				}
				else
				{
					std::cout << "\t\t" << arg.second << std::endl;
				}
				std::cout << std::endl;
			}
			::exit(0);
		}, "Prints this help message.");

		SysAddArgumentI("daemon", 'D', false, [](const std::string&){
			if(!SysDaemonize())
			{
#ifdef __linux__
				LogError("Daemonize failed or not available on this system.");
#else
				LogError("Daemonize is not available on Windows.");
#endif
				LogDebug("Aborting execution.");
				::exit(0);
			}
		}, "Turn the current process into a daemon (linux only).");

		SysAddArgumentI("version", 'v', false, [](const std::string&){
			std::cout << "v" MX_VSTR " - " MX_VNAME << " (" << MX_HASH "-" << MX_BRANCH << ")" << std::endl;
			::exit(0);
		}, "Gets the program's current version.");

		// Ignore argv[0]
		for(int i = 1; i < argc;)
		{
			if(strlen(argv[i]) < 2)
			{
				LogError("SysParseArguments: Failed to parse arguments.");
				return false;
			}
			if(argv[i][0] != '-')
			{
				// We don't support positional arguments
				LogError("SysParseArguments: Positional arguments are not supported [%s].", argv[i]);
				return false;
			}
			else if(argv[i][1] != '-')
			{
				// Parsing shortname (-a)
				if(strlen(argv[i]) > 2)
				{
					LogError("SysParseArguments: Wrong syntax at: %s. Expected one character identifier.", argv[i]);
					return false;
				}

				auto it = _sys_argscmd_short.find(argv[i][1]);
				if(it == _sys_argscmd_short.end())
				{
					LogError("SysParseArguments: Unrecognized command line argument: %s.", argv[i]);
					return false;
				}

	  			bool reqarg = _sys_argscmd_short_reqarg[argv[i++][1]];
				if(reqarg && (i >= argc || argv[i][0] == '-'))
				{
					LogError("SysParseArguments: Expected argument after %s.", argv[i - 1]);
					return false;
				}

				if(reqarg)
				{
					it->second(argv[i++]);
				}
				else
	  			{
					it->second("");
				}
			}
			else
			{
				// Parsing longname (--argument)
				auto it = _sys_argscmd_long.find(&argv[i][2]);
				if(it == _sys_argscmd_long.end())
				{
					LogError("SysParseArguments: Unrecognized command line argument: %s.", argv[i]);
					return false;
				}

	  			bool reqarg = _sys_argscmd_long_reqarg[&argv[i++][2]];
				if(reqarg && (i >= argc || argv[i][0] == '-'))
				{
					LogError("SysParseArguments: Expected argument after %s.", argv[i - 1]);
					return false;
				}

				if(reqarg)
				{
					it->second(argv[i++]);
				}
				else
	  			{
					it->second("");
				}
			}
		}
		return true;
	}

	bool SysDaemonize()
	{
#ifdef __linux__
		LogTrace("SysDaemonize: Attempting to fork...");
		int pid = ::fork();
		if(pid < 0)
		{
			LogError("SysDaemonize: Failed to spawn daemon with fork().");
			return false;
		}
		else if(pid != 0)
		{
			::exit(0);
		}

		_sys_isdaemon = true;
		for(int i = 0; i < 3; i++)
		{
			::close(i);
			int fd = ::open("/dev/null", O_RDWR, 0);
			if(fd < 0)
			{
				fd = ::open("/dev/null", O_WRONLY, 0);
			}
			
			if(fd < 0)
			{
				LogError("SysDaemonize: Failed to open /dev/null.");
				return false;
			}

			if(fd != i)
			{
				LogError("SysDaemonize: Failed to assign file descriptor to /dev/null.");
				return false;
			}
		}

		setsid();

		return true;
#else
		// Do nothing on windows
		// TODO: (Cesar) Implement a service or some other mechanism to mimic a daemon on windows
		return false;
#endif
	}

	bool SysInitializeExperiment(int argc, char* argv[])
	{
		// Set the current time as the server starting time for the given experiment
		SysMarkUptimeNow();

		if(argc < 2)
		{
			// This is an experiment without a home location / name
			// and will not be saved
			LogWarning("SysInitializeExperiment: Could not detect experiment name.");
			LogWarning("SysInitializeExperiment: Data and metadata will not be saved.");
			return true;
		}

		bool loopback = false;
		std::uint16_t port = 8080;

		SysAddArgument("name", 'n', true, [](const std::string& expname){ _sys_expname = expname; }, "Set the current experiment name.");
		SysAddArgument("loopback", 'l', false, [&](const std::string&){ loopback = true; }, "Set the http server on loopback mode only.");
		SysAddArgument("port", 'p', true, [&](const std::string& portstr){ port = ::atoi(portstr.c_str()); }, "Set the http server listen port.");

		if(!SysParseArguments(argc, argv))
		{
			return false;
		}

		std::string exphome = SysGetExperimentHome();

		if(exphome.empty() && !_sys_expname.empty())
		{
			SysCreateNewExperiment(_sys_expname);
		}

		if(!std::filesystem::is_directory(exphome))
		{
			LogError("SysInitializeExperiment: Experiment home is know but does not exist.");
			LogDebug("SysInitializeExperiment: Creating directory.");
			SysCreateNewExperiment(_sys_expname);
		}

		RdbInit(1024 * 1024);
		PdbInit();

		SysStartPerformanceMetricsThread();

		// After PdbInit()
		RdbInitHistoryBuffer();

		RunInitVariables();

		_sys_rpc_thread = std::make_unique<RPCServerThread>();
		_sys_evt_thread = std::make_unique<EvtServerThread>();

		while(!_sys_rpc_thread->ready())
		{
			std::this_thread::yield();
		}

		while(!_sys_evt_thread->ready())
		{
			std::this_thread::yield();
		}

		// After evt thread init
		MsgInit();

		HttpStartServer(port, loopback);

		return true;
	}

	void SysCloseExperiment()
	{
		// Force run stop if running
		RunStop();

		MsgClose();

		HttpStopServer();

		_sys_rpc_thread.reset();
		_sys_evt_thread.reset();

		RdbCloseHistoryBuffer();

		SysStopPerformanceMetricsThread();

		PdbClose();
		RdbClose();

		SysUnlockCurrentProcess();
	}

	static void SysWriteRexClientInfo(const std::string& hostname)
	{
		const std::uint64_t cid = SysGetClientId();
		const std::string cwd = std::filesystem::current_path().string();
		const std::string bpath(SysGetBinaryFullName());

		if(!RexUpdateClientInfo(cid, cwd, bpath, hostname))
		{
			if(!RexCreateClientInfo(cid, cwd, bpath, hostname))
			{
				LogError("SysWriteRexClientInfo: Failed to create/update rex client info.");
			}
		}
	}

	bool SysInitializeBackend(int argc, char* argv[])
	{
		// NOTE: (Cesar)
		// Try to lock
		// This prevents multiple instances of the same backend to run simultaneously
		if(!SysLockCurrentProcess())
		{
			LogError("SysInitializeBackend: Failed to lock current process.");
			LogDebug("Maybe it's already running?");
			return false;
		}

		std::string server_name = "localhost";

		SysAddArgument("server", 's', true, [&](const std::string& server){ server_name = server; }, "Set the server to connect to.");

		if(!SysParseArguments(argc, argv))
		{
			return false;
		}

		// Setup Rex
		SysWriteRexClientInfo(server_name);

		// Now try to connect
		if(!SysConnectToExperiment(server_name.c_str()))
		{
			return false;
		}

		return true;
	}

	std::optional<const Experiment*> SysGetConnectedExperiment()
	{
		if(_sys_experiment_connected)
			return &_sys_experiment;
		return std::nullopt;
	}

	bool SysConnectToExperiment(const char* hostname, std::uint16_t port)
	{
		_sys_experiment_connected = true;
		_sys_experiment._rpc_client = std::make_unique<RPCClientThread>(hostname, RPC_PORT);
		_sys_experiment._evt_client = std::make_unique<EvtClientThread>(hostname, nullptr, EVT_PORT);
		return true;
	}

	void SysDisconnectFromExperiment()
	{
		if(_sys_experiment._rpc_client)
		{
			_sys_experiment._rpc_client.reset();
		}

		if(_sys_experiment._evt_client)
		{
			_sys_experiment._evt_client.reset();
		}
		
		_sys_experiment_connected = false;
	}

	void SysRegisterSigintAction(SysSigintActionFunc f)
	{
#ifdef __linux__
		::signal(SIGINT, f);
#else
		SetConsoleCtrlHandler(f, TRUE);
#endif
	}

	const std::atomic<bool>* SysSetupExitSignal()
	{
		static std::atomic<bool> signal_flag = false;
#ifdef __linux__
		SysRegisterSigintAction([](int sig){ signal_flag.store(sig != 0); });
#else
		SysRegisterSigintAction([](DWORD ctrlType) -> BOOL {
			switch(ctrlType)
			{
				case CTRL_C_EVENT: 	   // ctrl-c
				case CTRL_CLOSE_EVENT: // window close
				{
					signal_flag.store(true);
					return TRUE;
				}
				default:
					return FALSE;
			}
			return FALSE;
		});
#endif
		return &signal_flag;
	}

	SysRecvThread::SysRecvThread(const Socket& socket, std::uint64_t ssize, std::uint64_t sheadersize, std::uint64_t sheaderoffset)
		: _handle(), _stream(ssize, sheadersize, sheaderoffset)
	{
		_handle = std::thread([&](){
			static constexpr std::uint64_t SOCKET_RECV_BUFSIZE = 32768;
			std::uint8_t rbuffer[SOCKET_RECV_BUFSIZE];
			std::uint64_t read;
			while(true)
			{
				SocketResult r = SocketRecvBytes(socket, rbuffer, SOCKET_RECV_BUFSIZE, &read);
				if((r == SocketResult::DISCONNECT) || (r == SocketResult::ERROR))
				{
					break;
				}
				else if(r == SocketResult::TIMEOUT)
				{
					if(_stream.unblockRequested())
					{
						break;
					}
					std::this_thread::sleep_for(std::chrono::microseconds(10));
					continue;
				}

				if(!_stream.push(rbuffer, read))
				{
					break;
				}
			}
			LogTrace("SysRecvThread: Stopped.");
			_stream.requestUnblock();
		});
	}
	
	[[nodiscard]] std::unique_ptr<SysRecvThread> SysStartRecvThread(const Socket& socket, std::uint64_t headersize, std::uint64_t headeroffset)
	{
		if(SysRecvThreadCanStart(socket))
		{
			return std::make_unique<SysRecvThread>(socket, SYS_RECV_THREAD_BUFFER_SIZE, headersize, headeroffset);
		}

		LogError("SysStartRecvThread failed to start. Provided socket already has a running recv thread.");
		return nullptr;
	}

	bool SysRecvThreadCanStart(const Socket& socket)
	{
		static_cast<void>(socket);
		return true;
	}

	void SysBufferStack::push(std::vector<std::uint8_t>&& data)
	{
		ZoneScoped;
		{
			std::unique_lock<std::mutex> lock(_mutex);
			_stack.push(std::move(data));
		}
		_notifier.notify_one();	
	}

	void SysBufferStack::push(const std::vector<std::uint8_t>& data)
	{
		ZoneScoped;
		{
			std::unique_lock<std::mutex> lock(_mutex);
			_stack.push(data);
		}
		_notifier.notify_one();	
	}

	std::vector<std::uint8_t> SysBufferStack::pop()
	{
		ZoneScoped;
		std::unique_lock<std::mutex> lock(_mutex);
		_notifier.wait(lock, [this](){ return !_stack.empty() || _sig_unblock.load(); }); // Don't wait if the stack if not empty
		if(_sig_unblock.load())
		{
			return std::vector<std::uint8_t>();
		}
		std::vector<std::uint8_t> val = std::move(_stack.top());
		_stack.pop();
		return val;
	}

	void SysBufferStack::requestUnblock()
	{
		_sig_unblock.store(true);
		_notifier.notify_all();
	}

	void SysRefBufferStack::push(std::vector<std::uint8_t>&& data, std::uint16_t ref)
	{
		_refcount.store(ref);
		{
			std::unique_lock<std::mutex> lock(_mutex);
			_stack.push(std::move(data));
		}
		_notifier.notify_all();	
	}

	std::vector<std::uint8_t> SysRefBufferStack::pop()
	{
		std::unique_lock<std::mutex> lock(_mutex);
		_notifier.wait(lock);
		if(_stack.empty())
		{
			return std::vector<std::uint8_t>();
		}
		std::vector<std::uint8_t> val = _stack.top();

		decref();

		return val;
	}

	const std::vector<std::uint8_t>* SysRefBufferStack::peek()
	{
		std::unique_lock<std::mutex> lock(_mutex);
		_notifier.wait(lock);
		if(_stack.empty())
		{
			return nullptr;
		}
		return &_stack.top();
	}

	void SysRefBufferStack::decref()
	{
		_refcount--; // This is atomic
		if(_refcount.load() == 0)
		{
			std::lock_guard<std::mutex> lock(_mutex);
			if(!_stack.empty()) _stack.pop();
		}
	}

	void SysRefBufferStack::requestUnblock()
	{
		_notifier.notify_all();
	}

	SysByteStream::SysByteStream(std::uint64_t size, std::uint64_t headersize, std::uint64_t headeroffset)
	{
		_buffer.resize(size);
		_buffer_offset = 0;
		_header_size = headersize;
		_header_size_offset = headeroffset;
		_unblock_sig.store(false);
	}

	bool SysByteStream::push(std::uint8_t* data, std::uint64_t size)
	{
		ZoneScoped;
		{
			std::unique_lock<std::mutex> lock(_mutex);
			
			_notifier.wait(lock, [&](){ return (_buffer_offset + size <= _buffer.size()) || _unblock_sig.load(); });

			if(_unblock_sig.load())
			{
				return false;
			}
			
			std::memcpy(_buffer.data() + _buffer_offset, data, size);
			_buffer_offset += size;
		}
		_notifier.notify_one();
		return true;
	}

	std::uint64_t SysByteStream::fetch(std::uint8_t* buffer, std::uint64_t size)
	{
		ZoneScoped;
		std::uint64_t payloadsize = 0;
		{
			std::unique_lock<std::mutex> lock(_mutex);
			
			_notifier.wait(lock, [&](){ return (_buffer_offset >= _header_size) || _unblock_sig.load(); });
			
			if(_unblock_sig.load())
			{
				return 0;
			}
			
			std::uint32_t msg_size;
			std::memcpy(&msg_size, _buffer.data() + _header_size_offset, sizeof(std::uint32_t));
			
			payloadsize = msg_size + _header_size;

			// LogTrace("Waiting for buffer with payloadsize = %llu", payloadsize);
			// LogTrace("HS = %llu", _header_size);
			// LogTrace("MSG = %d", msg_size);
			// LogTrace("Current buffer offset = %llu", _buffer_offset);
			// LogTrace("Buffer total size = %llu", _buffer.size());
			
			_notifier.wait(lock, [&](){ return payloadsize <= _buffer_offset; });

			std::memcpy(buffer, _buffer.data(), payloadsize);
			_buffer_offset -= payloadsize;

			if(_buffer_offset > 0)
			{
				std::memmove(_buffer.data(), _buffer.data() + payloadsize, _buffer_offset);
			}
		}
		_notifier.notify_one();
		return payloadsize;
	}

	void SysByteStream::requestUnblock()
	{
		_unblock_sig.store(true);
		_notifier.notify_all();
	}

	bool SysByteStream::unblockRequested() const
	{
		return _unblock_sig.load();
	}

	SysFileWatcher::SysFileWatcher(
		const std::string& dir,
		std::function<void(const FileOp op, const std::string& filename, const std::int64_t timestamp)> f,
		std::uint32_t interval
	)
	{
		_watcher_on.store(true);
		LogDebug("SysFileWatcher: Watching dir <%s>. [interval=%d]", dir.c_str(), interval);
		_thread = std::make_unique<std::thread>([this, dir, f, interval](){
			std::unordered_map<std::string, std::filesystem::file_time_type> file_mod_time;

			// NOTE: (César) clock_cast is not yet available under GCC for C++20
			auto to_systime = [](const std::filesystem::file_time_type& time) -> std::int64_t {
				auto delta = std::filesystem::file_time_type::clock::now().time_since_epoch() - time.time_since_epoch();
				auto sys_file_time = std::chrono::system_clock::now().time_since_epoch() - delta;
				return std::chrono::duration_cast<std::chrono::milliseconds>(sys_file_time).count();
			};

			while(_watcher_on.load())
			{
				for(const auto& file : std::filesystem::recursive_directory_iterator(dir))
				{
					const std::string& file_path = file.path().string();
					const auto& last_write_ts = std::filesystem::last_write_time(file);

					if(file_mod_time.find(file_path) == file_mod_time.end())
					{
						// This is a new file
						file_mod_time[file_path] = last_write_ts;
						f(FileOp::CREATED, file_path, to_systime(last_write_ts));
					}
					else if(file_mod_time[file_path] != last_write_ts)
					{
						// This is a modified file
						file_mod_time[file_path] = last_write_ts;
						f(FileOp::MODIFIED, file_path, to_systime(last_write_ts));
					}
				}

				auto fit = file_mod_time.begin();
				while(fit != file_mod_time.end())
				{
					if(!std::filesystem::exists(fit->first))
					{
						f(FileOp::DELETED, fit->first, to_systime(fit->second));
						fit = file_mod_time.erase(fit);
						continue;
					}
					fit++;
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(interval));
			}
		});
	}

	SysFileWatcher::~SysFileWatcher()
	{
		_watcher_on.store(false);
		_thread->join();
	}

	SysAsyncEventLoop::SysAsyncEventLoop()
	{
		_running.store(true);
		_handle = std::thread([this]() {
			while(true)
			{
				SysAsyncTask task;

				{
					std::unique_lock<std::mutex> lock(_mutex);
					if(_queue.empty() && _running.load())
					{
						_cv.wait(lock);
					}

					// If a stop command was issued
					if(_queue.empty() && !_running.load())
					{
						break; // Similar to requestUnblock()
					}

					const std::int64_t now = SysGetCurrentTime();
					if(!_queue.empty() && _queue.top()._scheduled_exec <= now)
					{
						task = _queue.top();
						_queue.pop();
					}
					else if(!_queue.empty())
					{
						_cv.wait_until(lock, std::chrono::sys_time{std::chrono::milliseconds(_queue.top()._scheduled_exec)});
						continue;
					}
				}

				if(task._job) // NOTE: (Cesar) Superfluous check but guards against things like std::function<void()>()
				{
					task._job();
					if(task._interval > 0)
					{
						// This is a recurring task, reschedule excluding for task self time
						task._scheduled_exec += task._interval;
						schedule(task);
					}
				}
			}
		});
	}

	SysAsyncEventLoop::~SysAsyncEventLoop()
	{
		{
			// Locking allows for condition variable to evaluate
			std::unique_lock<std::mutex> lock(_mutex);
			_running.store(false);

			// Clear the queue
			// We don't want to wait on scheduled tasks when a close was requested
			while(!_queue.empty()) { _queue.pop(); };
		}
		_cv.notify_all();

		LogDebug("SysAsyncEventLoop: Waiting for async tasks...");
		if(_handle.joinable())
		{
			_handle.join();
		}
	}

	void SysAsyncEventLoop::schedule(SysAsyncTask& task)
	{
		std::unique_lock<std::mutex> lock(_mutex);
		_queue.push(std::move(task));
		_cv.notify_all();
	}

	void SysAsyncEventLoop::schedule(SysAsyncTask::Job job, std::int64_t delay, std::int64_t interval)
	{
		std::unique_lock<std::mutex> lock(_mutex);
		_queue.push({
			._job = std::move(job),
			._scheduled_exec = SysGetCurrentTime() + delay,
			._interval = interval
		});
		_cv.notify_all();
	}

	std::int64_t SysGetCurrentTime()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}

	std::string_view SysGetCacheDir()
	{
		if(!_mxcachedir.empty())
		{
			return _mxcachedir;
		}
#ifdef __linux__
	std::string croot = std::getenv("HOME");
	croot += "/.mxcache";
#else
	std::string croot = std::getenv("LocalAppData");
	croot += "/mxcache";
#endif
	
		if(!std::filesystem::is_directory(croot) && !std::filesystem::create_directory(croot))
		{
			LogError("SysGetCacheDir: Failed to create new directory for system cache.");
			return "";
		}

		_mxcachedir = croot;
		return _mxcachedir;	
	}

	std::string_view SysGetCacheLockDir()
	{
		if(!_mxcachelockdir.empty())
		{
			return _mxcachelockdir;
		}

		std::string lock = std::string(SysGetCacheDir()) + "/.lock";
	
		if(!std::filesystem::is_directory(lock) && !std::filesystem::create_directory(lock))
		{
			LogError("SysGetCacheDir: Failed to create new directory for system cache lock.");
			return "";
		}

		_mxcachelockdir = lock;
		return _mxcachelockdir;	
	}

	bool SysCreateNewExperiment(const std::string& expname)
	{
		// Write to cache
		std::string_view cache = SysGetCacheDir();
		std::string exp_cache = std::string(cache) + "/exp";
		std::string exp_home = std::string(SysGetCacheDir()) + "/" + expname;
		std::ofstream outfile(exp_cache, std::ios::app);
		if(!outfile.is_open())
		{
			LogError("SysCreateNewExperiment: Failed to open <exp> file.");
			return false;
		}

		outfile << expname << "," << exp_home << '\n';

		// Now create the experiment directory
		if(!std::filesystem::create_directory(exp_home))
		{
			LogError("SysCreateNewExperiment: Failed to create exp home directory under <%s>.", exp_home.c_str());
			return false;
		}
		return true;
	}

	std::string SysGetExperimentHome()
	{
		std::string_view cache = SysGetCacheDir();
		std::string exp_cache = std::string(cache) + "/exp";

		if(!std::filesystem::is_regular_file(exp_cache))
		{
			LogError("SysGetAvailableExperiments: Failed to locate <exp> file.");
			return "";
		}
		
		std::ifstream file(exp_cache);
		if(!file.is_open())
		{
			LogError("SysGetAvailableExperiments: Failed to read <exp> file.");
			return "";
		}

		std::string expline;
		while(std::getline(file, expline))
		{
			// Lines are very simple text that declare new experiments and their 'main' workdir
			std::size_t idx = expline.find(",");
			if(idx == std::string::npos)
			{
				LogError("SysGetAvailableExperiments: Invalid <exp> file format.");
				return "";
			}

			if(expline.substr(0, idx) == _sys_expname)
			{
				return expline.substr(idx + 1);
			}
		}

		LogError("SysGetAvailableExperiments: Failed to find experiment <%s>.", _sys_expname.c_str());
		return "";
	}

	mulex::mxstring<512> SysGetExperimentName()
	{
		return _sys_expname.c_str();
	}

	std::string_view SysGetBinaryName()
	{
		if(!_sys_binname.empty())
		{
			return _sys_binname;
		}

#ifdef __linux__
		std::string binname;
		std::ifstream("/proc/self/comm") >> binname;
#else
		char binnamebuf[MAX_PATH];
		GetModuleFileNameA(nullptr, binnamebuf, MAX_PATH);
		char* binname = PathFindFileNameA(binnamebuf);
#endif
		_sys_binname = binname;
		return _sys_binname;
	}

	std::string_view SysGetBinaryFullName()
	{
		if(!_sys_fullbinname.empty())
		{
			return _sys_fullbinname;
		}

#ifdef __linux__
		char binnamebuf[PATH_MAX];
		ssize_t count = ::readlink("/proc/self/exe", binnamebuf, PATH_MAX);
		if(count == -1)
		{
			LogError("SysGetBinaryFullName: Failed to fetch the current process' binary absolute path.");
			return "";
		}
		_sys_fullbinname = std::string(binnamebuf, count);
		return _sys_fullbinname;
#else
		char binnamebuf[MAX_PATH];
		GetModuleFileNameA(nullptr, binnamebuf, MAX_PATH);
		_sys_fullbinname = binnamebuf;
		return _sys_fullbinname;
#endif
	}

	std::string_view SysGetHostname()
	{
		if(!_sys_hostname.empty())
		{
			return _sys_hostname;
		}

		char hostname[256];
#ifdef __linux__
		if(::gethostname(hostname, 256) < 0)
		{
			LogError("SysGetHostname: Failed to get local host name.");
			return "";
		}
#else
		DWORD size = 256;
		if(GetComputerNameA(hostname, &size) == 0)
		{
			LogError("SysGetHostname: Failed to get local host name.");
			return "";
		}
#endif
		_sys_hostname = hostname;
		return _sys_hostname;
	}

	static std::uint64_t mmh64a(const char* key, int len)
	{
		static constexpr std::uint64_t seed = 0x42069;
		const std::uint64_t m = 0xc6a4a7935bd1e995LLU;
		const int r = 47;

		std::uint64_t h = seed ^ (len * m);

		const std::uint64_t* data = (const uint64_t*)key;
		const std::uint64_t* end = (len >> 3) + data;

		while(data != end)
		{
			std::uint64_t k = *data++;

			k *= m; 
			k ^= k >> r; 
			k *= m; 

			h ^= k;
			h *= m; 
		}

		const std::uint8_t* data2 = (const std::uint8_t*)data;

		switch(len & 7)
		{
			case 7: h ^= (std::uint64_t)(data2[6]) << 48;
			case 6: h ^= (std::uint64_t)(data2[5]) << 40;
			case 5: h ^= (std::uint64_t)(data2[4]) << 32;
			case 4: h ^= (std::uint64_t)(data2[3]) << 24;
			case 3: h ^= (std::uint64_t)(data2[2]) << 16;
			case 2: h ^= (std::uint64_t)(data2[1]) << 8;
			case 1: h ^= (std::uint64_t)(data2[0]);
				  	h *= m;
		};

		h ^= h >> r;
		h *= m;
		h ^= h >> r;

		return h;
	}

	std::uint64_t SysStringHash64(const std::string& key)
	{
		return mmh64a(key.c_str(), key.size());
	}

	std::uint64_t SysGetClientId()
	{
		if(_sys_cid == 0x00)
		{
			std::string bname = std::string(SysGetBinaryName());
			std::string hname = std::string(SysGetHostname());
			std::string tname = bname + "@" + hname;
			// int size = tname.size() > 128 ? 128 : static_cast<int>(tname.size());
			int size = tname.size();
			_sys_cid = mmh64a(tname.c_str(), size);
		}
		return _sys_cid;
	}

	std::string SysI64ToHexString(std::uint64_t value)
	{
		std::stringstream ss;
		ss << std::hex << value;
		return ss.str();
	}

	std::string SysI16ToHexString(std::uint16_t value)
	{
		std::stringstream ss;
		ss << std::hex << value;
		return ss.str();
	}

	std::vector<std::uint8_t> SysReadBinFile(const std::string& file)
	{
		std::ifstream in(file, std::ios::in | std::ios::binary);
		if(!in.is_open())
		{
			LogError("SysReadBinFile: Failed to open file <%s>", file.c_str());
			return std::vector<std::uint8_t>();
		}

		std::vector<std::uint8_t> out;
		in.seekg(0, std::ios::end);	
		out.resize(in.tellg());
		in.seekg(0, std::ios::beg);
		in.read(reinterpret_cast<char*>(out.data()), out.size());
		return out;
	}

	void SysWriteBinFile(const std::string& file, const std::vector<std::uint8_t>& data)
	{
		std::ofstream out(file, std::ios::out | std::ios::binary);
		if(!out.is_open())
		{
			LogError("SysWriteBinFile: Failed to open file <%s>", file.c_str());
			return;
		}

		out.write(reinterpret_cast<const char*>(data.data()), data.size());
	}

	void SysCopyFile(const std::string& source, const std::string& destination)
	{
		std::ifstream src(source, std::ios::binary);
		std::ofstream dst(destination, std::ios::binary);
		dst << src.rdbuf();
	}

	bool EvtEmit(const std::string& event, const std::uint8_t* data, std::uint64_t len)
	{
		return _sys_evt_thread->emit(event, data, len);
	}

	bool SysMatchPattern(const std::string& pattern, const std::string& target)
	{
		// Pattern examples (only Kleene Star is available) (this is a shortcut to be fast with our rdb key cases)
		// /system/backends/*/connected
		// /system/*/intermediate/*/value
		// /*/intermediate/*/value
		// * could match nothing
		// stuff like /*/*/value is not allowed use /*/value instead
		// this has limitations but is faster than a DP table
		
		// LogTrace("Initial state [p, t]: [%s, %s]", pattern.c_str(), target.c_str());

		// Match anything
		if(pattern == "/*")
		{
			return true;
		}
	
		auto ks_pos = pattern.find_first_of('*');
		if(ks_pos == std::string::npos)
		{
			return (pattern == target);
		}

		if(ks_pos > 1)
		{
			std::string prefix = pattern.substr(0, ks_pos);
			// LogTrace("Mode: prefix [%s]", prefix.c_str());
			if(target.find(prefix) != 0)
			{
				return false;
			}

			return SysMatchPattern(pattern.substr(ks_pos - 1), target.substr(ks_pos - 1));
		}
		else if(ks_pos == 1)
		{
			std::string suffix = pattern.substr(ks_pos + 1);
			// LogTrace("Mode: suffix [%s] [tgt: %s]", suffix.c_str(), target.c_str());
			auto next_ks = suffix.find_first_of('*');
			if(next_ks != std::string::npos)
			{
				std::string midfix = pattern.substr(ks_pos + 1, next_ks - ks_pos + 1);
				// LogTrace("Midfix: %s", midfix.c_str());
				auto next_target_pos = target.find(midfix);
				if(next_target_pos == std::string::npos)
				{
					return false;
				}
				return SysMatchPattern(pattern.substr(next_ks + 1), target.substr(next_target_pos + midfix.size()));
			}
			std::int64_t match_sz = target.size() - suffix.size();
			return match_sz >= 0 ? (target.find(suffix) == static_cast<std::uint64_t>(match_sz)) : false;
		}

		// ks is not in a valid position
		return false;	
	}

	bool SysSpawnProcess(const std::string& binary, const std::string& workdir, const std::vector<std::string>& argv)
	{
#ifdef __linux__
		pid_t pid = ::fork();
		if(pid < 0)
		{
			LogError("SysSpawnProcess: Failed to fork process.");
			return false;
		}
		
		// Child process
		if(pid == 0)
		{
			if(::chdir(workdir.c_str()) != 0)
			{
				LogError("SysSpawnProcess: Failed to change to working directory <%s>.", workdir.c_str());
				::exit(EXIT_FAILURE);
			}

			std::string filename = binary.substr(binary.find_last_of('/') + 1);
			std::ostringstream ss;

			ss << binary;
			for(const auto& arg : argv)
			{
				ss << " " << arg;
			}
			std::vector<char*> args;

			args.push_back(const_cast<char*>(filename.c_str()));
			for(const auto& arg : argv)
			{
				args.push_back(const_cast<char*>(arg.c_str()));
			}
			args.push_back(NULL);

			LogDebug("Details:");
			LogDebug("\tName: %s", filename.c_str());
			LogDebug("\tArgs: %s", ss.str().c_str());
			LogDebug("\tWorkdir: %s", workdir.c_str());

			for(int i = 0; i < 3; i++)
			{
				::close(i);
				int fd = ::open("/dev/null", O_RDWR, 0);
				if(fd < 0)
				{
					fd = ::open("/dev/null", O_WRONLY, 0);
				}
				
				if(fd < 0)
				{
					LogError("SysSpawnProcess: Failed to open /dev/null.");
					return false;
				}

				if(fd != i)
				{
					LogError("SysSpawnProcess: Failed to assign file descriptor to /dev/null.");
					return false;
				}
			}

			setsid();

			::execv(binary.c_str(), args.data());
		}
		
		return true;
#else
		STARTUPINFOA si;
		ZeroMemory( &si, sizeof(si) );
		si.cb = sizeof(si);

		PROCESS_INFORMATION pi;
		ZeroMemory( &pi, sizeof(pi) );

		std::string filename = binary.substr(binary.find_last_of("/\\") + 1);
		std::ostringstream ss;

		ss << binary;
		for(const auto& arg : argv)
		{
			ss << " " << arg;
		}

		LogDebug("Details:");
		LogDebug("\tName: %s", filename.c_str());
		LogDebug("\tArgs: %s", ss.str().c_str());
		LogDebug("\tWorkdir: %s", workdir.c_str());

		BOOL success = CreateProcessA(
			NULL, //filename.c_str(),
			const_cast<char*>(ss.str().c_str()),
			NULL,
			NULL,
			FALSE,
			CREATE_NO_WINDOW | DETACHED_PROCESS,
			NULL,
			workdir.c_str(),
			&si,
			&pi
		);
		
		if(!success)
		{
			LogError("SysSpawnProcess: Failed to start process.");
			LogError("Windows error: %d", GetLastError());
			return false;
		}
		
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		return true;
#endif
	}

	bool SysInterruptProcess(SysProcHandle handle)
	{
#ifdef __linux__
		return (::kill(handle, SIGINT) == 0);
#else
		// Try SIGINT equivalent if not hard terminate
		if(AttachConsole(handle))
		{
			SetConsoleCtrlHandler(NULL, TRUE);
			GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
			FreeConsole();
			return true;
		}

		HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, handle);
		if(h)
		{
			TerminateProcess(h, 1);
			CloseHandle(h);
			return true;
		}

		return false;
#endif
	}

	std::string SysGetProcessBinaryName(SysProcHandle handle)
	{
#ifdef __linux__
		char binnamebuf[PATH_MAX];
		std::string link = "/proc/" + std::to_string(handle) + "/exe";
		ssize_t count = ::readlink(link.c_str(), binnamebuf, PATH_MAX);
		if(count == -1)
		{
			LogError("SysGetBinaryFullName: Failed to fetch the current process' binary absolute path.");
			return "";
		}
		return std::string(binnamebuf, count);
#else
		HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, handle);
		if(!h) return "";

		char binnamebuf[MAX_PATH];
		DWORD size = MAX_PATH;

		if(QueryFullProcessImageNameA(h, 0, binnamebuf, &size))
		{
			CloseHandle(h);
			return std::string(binnamebuf);
		}
		
		CloseHandle(h);
		return "";
#endif
	}

	bool SysLockCurrentProcess()
	{
		const static std::string proc_lock_file = std::string(SysGetCacheLockDir()) + "/" + std::string(SysGetBinaryName()) + ".lock";
#ifdef __linux__
		_sys_lock_handle = ::open(proc_lock_file.c_str(), O_RDWR | O_CREAT, 0640);
		if(_sys_lock_handle < 0)
		{
			LogError("SysLockCurrentProcess: Error on opening lockfile.");
			return false;
		}

		if(::flock(_sys_lock_handle, LOCK_EX | LOCK_NB) < 0)
		{
			LogError("SysLockCurrentProcess: Another instance of this process is already running.");
			::close(_sys_lock_handle);
			return false;
		}

		// Erase file and write PID
		::ftruncate(_sys_lock_handle, 0);
		std::string pid = std::to_string(::getpid());
		::write(_sys_lock_handle, pid.c_str(), pid.size() + 1);
#else
		_sys_lock_handle = CreateFileA(
			proc_lock_file.c_str(),
			GENERIC_WRITE,
			FILE_SHARE_READ,
			NULL,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL
		);
		if(_sys_lock_handle == INVALID_HANDLE_VALUE)
		{
			LogError("SysLockCurrentProcess: Error on opening lockfile.");
			LogError("SysLockCurrentProcess: Another instance of this process is already running.");
			return false;
		}

		// OVERLAPPED o;
		// ZeroMemory(&o, sizeof(o));
		//
		// if(!LockFileEx(_sys_lock_handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, MAXDWORD, MAXDWORD, &o))
		// {
		// 	LogError("SysLockCurrentProcess: Another instance of this process is already running.");
		// 	CloseHandle(_sys_lock_handle);
		// 	return false;
		// }

		// SetEndOfFile(_sys_lock_handle);
		std::string pid = std::to_string(GetCurrentProcessId());
		std::cout << pid << std::endl;
		// SetFilePointer(_sys_lock_handle, 0, NULL, FILE_BEGIN);
		DWORD written;
		WriteFile(_sys_lock_handle, pid.c_str(), pid.size(), &written, NULL);
		// SetEndOfFile(_sys_lock_handle);
#endif
		return true;
	}

	bool SysUnlockCurrentProcess()
	{
#ifdef __linux__
		return ::close(_sys_lock_handle) == 0;
#else
		return CloseHandle(_sys_lock_handle) != 0;
#endif
	}

	static double SysGetCpuUsage()
	{
#ifdef __linux__
		static std::uint64_t prev_idle = 0;
		static std::uint64_t prev_total = 0;
		std::ifstream stat("/proc/stat");
		std::string line;
		std::getline(stat, line);
		std::istringstream iss(line);
		std::string cpu;
		std::uint64_t times[8];
		iss >> cpu >> times[0] >> times[1] >> times[2] >> times[3] >> times[4] >> times[5] >> times[6] >> times[7];
		std::uint64_t total = times[0] + times[1] + times[2] + times[3] + times[4] + times[5] + times[6] + times[7];
		std::uint64_t delta_total = total - prev_total;
		std::uint64_t delta_idle = times[3] - prev_idle;

		prev_idle = times[3];
		prev_total = total;

		return 100.0 * (delta_total - delta_idle) / delta_total;
#else
		static PDH_HQUERY cpu_query;
		static PDH_HCOUNTER cpu_total;
		static bool init = false;
		if (!init)
		{
			PdhOpenQuery(nullptr, 0, &cpu_query);
			PdhAddCounter(cpu_query, "\\Processor(_Total)\\% Processor Time", 0, &cpu_total);
			PdhCollectQueryData(cpu_query);
			init = true;
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}

		PdhCollectQueryData(cpu_query);

		PDH_FMT_COUNTERVALUE cpu_value;
		PdhGetFormattedCounterValue(cpu_total, PDH_FMT_DOUBLE, nullptr, &cpu_value);
		return cpu_value.doubleValue;
#endif
	}

	static std::pair<std::uint64_t, std::uint64_t> SysGetRAMUsage()
	{
#ifdef __linux__
		std::ifstream stat("/proc/meminfo");
		std::string line;
		std::uint64_t total_ram;
		std::uint64_t free_ram;
		std::uint8_t done = 0;

		while(std::getline(stat, line) && !(done & 2))
		{
			std::istringstream iss(line);
			std::string token;
			iss >> token;

			if(token == "MemAvailable:")
			{
				iss >> free_ram; done++;
			}
			else if(token == "MemTotal:")
			{
				iss >> total_ram; done++;
			}
		}

		return { total_ram * 1024, (total_ram - free_ram) * 1024 };

		// struct sysinfo info;
		// sysinfo(&info);
		// return { info.totalram * info.mem_unit, (info.totalram - info.freeram - info.bufferram) * info.mem_unit };
#else
		MEMORYSTATUSEX statex;
		statex.dwLength = sizeof(statex);
		GlobalMemoryStatusEx(&statex);
		return { statex.ullTotalPhys, statex.ullTotalPhys - statex.ullAvailPhys };
#endif
	}

	SysPerformanceMetrics SysGetPerformanceMetrics()
	{
		SysPerformanceMetrics metrics;

		metrics._cpu_usage = SysGetCpuUsage();
		std::tie(metrics._ram_total, metrics._ram_used) = SysGetRAMUsage();

		return metrics;
	}

	void SysStartPerformanceMetricsThread()
	{
		RdbCreateValueDirect("/system/metrics/cpu_usage", RdbValueType::FLOAT64, 0, 0.0);
		RdbCreateValueDirect("/system/metrics/mem_total", RdbValueType::UINT64, 0, std::uint64_t(0));
		RdbCreateValueDirect("/system/metrics/mem_used" , RdbValueType::UINT64, 0, std::uint64_t(0));

		SysPerformanceMetrics metrics = SysGetPerformanceMetrics();
		RdbWriteValueDirect("/system/metrics/mem_total", metrics._ram_total);

		_sys_performance_metrics_running.store(true);

		_sys_performance_metrics_thread = std::make_unique<std::thread>([](){
			while(_sys_performance_metrics_running.load())
			{
				SysPerformanceMetrics metrics = SysGetPerformanceMetrics();

				RdbWriteValueDirect("/system/metrics/cpu_usage", metrics._cpu_usage);
				RdbWriteValueDirect("/system/metrics/mem_used", metrics._ram_used);

				std::this_thread::sleep_for(std::chrono::milliseconds(SYS_PERF_GATHER_INTERVAL));
			}
		});
		LogTrace("[sys] SysStartPerformanceMetricsThread() OK.");
	}

	void SysStopPerformanceMetricsThread()
	{
		LogTrace("[sys] Shutting down performance metrics thread.");
		_sys_performance_metrics_running.store(false);
		_sys_performance_metrics_thread->join();
		_sys_performance_metrics_thread.reset();
	}

	void SysMarkUptimeNow()
	{
		_sys_uptime_mark = SysGetCurrentTime();
		LogTrace("[sys] Setting uptime mark to unix timestamp: %ull.", _sys_uptime_mark);
	}

	std::int64_t SysGetUptimeMark()
	{
		return _sys_uptime_mark;
	}
} // namespace mulex
