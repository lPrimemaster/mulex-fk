#include "../mxbackend.h"
#include "../mxsystem.h"
#include "../mxlogger.h"
#include "../mxevt.h"
#include "../mxrun.h"
#include <condition_variable>
#include <thread>
#include <vector>
#include <rpcspec.inl>

static const std::atomic<bool>* stop;

namespace mulex
{
	mulex::RPCGenericType BckCallUserRpc(mulex::string32 bck, mulex::RPCGenericType data, std::int64_t timeout)
	{
		std::string evt = bck.c_str();
		std::string evt_emt = evt + "::rpc";
		std::string evt_res = evt + "::rpc_res";
		std::vector<std::uint8_t> response_full;

		LogTrace("[mxbackend] User RPC called for <%s>.", bck.c_str());

		if(EvtGetId(evt_emt.c_str()) == 0)
		{
			response_full.push_back(static_cast<std::uint8_t>(BckUserRpcStatus::NO_SUCH_BACKEND));
			return response_full;
		}

		if(!EvtEmit(evt_emt, data.getData(), data.getSize()))
		{
			response_full.push_back(static_cast<std::uint8_t>(BckUserRpcStatus::EMIT_FAILED));
			return response_full;
		}

		// Wait for response
		std::shared_ptr<std::mutex> mtx = std::make_shared<std::mutex>();
		std::shared_ptr<std::condition_variable> cv = std::make_shared<std::condition_variable>();
		std::shared_ptr<bool> proceed = std::make_shared<bool>(false);
		std::shared_ptr<std::vector<std::uint8_t>> response = std::make_shared<std::vector<std::uint8_t>>();
		EvtServerRegisterCallback(evt_res, [=](auto, auto, auto, const std::uint8_t* data, std::uint64_t len) {
			LogTrace("[bck] Relaying RPC response.");
			if(len > 0)
			{
				std::unique_lock<std::mutex> lock(*mtx);
				response->resize(len);
				std::memcpy(response->data(), data, len);
				*proceed = true;
			}
			cv->notify_one();
		});

		// Wait timeout
		std::unique_lock<std::mutex> lock(*mtx);
		if(timeout > 0)
		{
			if(!cv->wait_for(lock, std::chrono::milliseconds(timeout), [&](){ return *proceed; }))
			{
				response_full.push_back(static_cast<std::uint8_t>(BckUserRpcStatus::RESPONSE_TIMEOUT));
				return response_full;
			}
		}
		else
		{
			static std::once_flag flag;
			std::call_once(flag, [](){ LogWarning("[mxbackend] Calling user RPC functions with zero timeout is discouraged."); });
			cv->wait(lock, [&](){ return *proceed; });
		}

		if(response->empty())
		{
			response_full.push_back(static_cast<std::uint8_t>(BckUserRpcStatus::OK));
			return response_full;
		}

		response_full.push_back(static_cast<std::uint8_t>(BckUserRpcStatus::OK));
		response_full.insert(response_full.end(), response->begin(), response->end());
		return response_full;
	}

	void BckDeleteMeta(std::uint64_t cid)
	{
		// Check if we are connected to this backend first
		std::string root = "/system/backends/" + SysI64ToHexString(cid) + "/";
		bool connected = static_cast<bool>(RdbReadValueDirect(root + "connected"));

		if(connected)
		{
			LogError("[mxbackend] Cannot delete backend keys. Backend is currently running.");
			return;
		}

		std::vector<RdbKeyName> bck_keys = RdbListSubkeys(root);
		LogTrace("[mxbackend] Deleting subkeys from: %s", root.c_str());
		for(const auto& key : bck_keys)
		{
			LogTrace("[mxbackend] Deleted metadata key <%s>.", key.c_str());
			RdbDeleteValueDirect(key);
		}
	}

	MxBackend::MxBackend(int argc, char* argv[])
	{
		if(!SysInitializeBackend(argc, argv))
		{
			LogError("[mxbackend] Failed to initialize the backend.");
			SysKillProcess("Failed to initialize backend.");
			return;
		}

		std::optional<const Experiment*> experiment = SysGetConnectedExperiment();

		if(!experiment.has_value())
		{
			LogError("[mxbackend] Failed to get connected experiment.");
			return;
		}

		_experiment = experiment.value();

		_init_ok = true;
	}

	MxBackend::~MxBackend()
	{
		if(_init_ok)
		{
			_experiment = nullptr;
			SysDisconnectFromExperiment();
			SysUnlockCurrentProcess();
		}
	}

	void MxBackend::registerEvent(const std::string& name)
	{
		if(_init_ok && _experiment)
		{
			_experiment->_evt_client->regist(name);
		}
	}

	void MxBackend::dispatchEvent(const std::string& evt, const std::uint8_t* data, std::uint64_t size)
	{
		// NOTE: (Cesar) We could ignore these checks unless the user uses this function out of place
		if(_init_ok && _experiment)
		{
			_experiment->_evt_client->emit(evt, data, size);
		}
	}

	void MxBackend::dispatchEvent(const std::string& evt, const std::vector<std::uint8_t>& data)
	{
		dispatchEvent(evt, data.data(), data.size());
	}

	void MxBackend::subscribeEvent(const std::string& evt, EvtClientThread::EvtCallbackFunc func)
	{
		if(_init_ok && _experiment)
		{
			_experiment->_evt_client->subscribe(evt, func);
		}
	}

	void MxBackend::unsubscribeEvent(const std::string& evt)
	{
		if(_init_ok && _experiment)
		{
			_experiment->_evt_client->unsubscribe(evt);
		}
	}

	void MxBackend::registerUserRpcEvent()
	{
		std::string rpc_event = std::string(SysGetBinaryName()) + "::rpc";
		std::string rpc_response = std::string(SysGetBinaryName()) + "::rpc_res";
		registerEvent(rpc_event);
		registerEvent(rpc_response);
		subscribeEvent(rpc_event, [this](auto* data, auto len, auto*){
			this->userRpcInternal(data, len, nullptr);
		});
	}

	void MxBackend::emitRpcResponse(const RPCGenericType& data)
	{
		static std::string rpc_response = std::string(SysGetBinaryName()) + "::rpc_res";
		dispatchEvent(rpc_response, data._data);
	}

	void MxBackend::userRpcInternal(const std::uint8_t* data, std::uint64_t len, const std::uint8_t* udata)
	{
		if(_user_rpc)
		{
			std::vector<std::uint8_t> vdata(data, data + len);
			_io.schedule([this, vdata]() {
				RPCGenericType response = _user_rpc(vdata);
				emitRpcResponse(response);
			});
		}
	}

	void MxBackend::setStatus(const std::string& status, const std::string& color)
	{
		// This only happens on non-ghost backends so one can call the SysGetClientId function
		static const std::string root_key = "/system/backends/" + SysI64ToHexString(SysGetClientId()) + "/";
		rdb[root_key + "user_status/text"] = mxstring<512>(status);
		rdb[root_key + "user_status/color"] = mxstring<512>(color);
	}

	MxRexDependencyManager MxBackend::registerDependency(const std::string& backend)
	{
		return MxRexDependencyManager(this, backend, 0x0, [this]() -> MsgEmitter& { return log; });
	}

	MxRexDependencyManager MxBackend::registerDependency(const std::uint64_t id)
	{
		return MxRexDependencyManager(this, "", id, [this]() -> MsgEmitter& { return log; });
	}

	std::tuple<BckUserRpcStatus, RPCGenericType> MxBackend::callUserRpc(const std::string& backend, const std::vector<uint8_t>& data, std::int64_t timeout)
	{
		RPCGenericType retval = _experiment->_rpc_client->call<RPCGenericType>(
			RPC_CALL_MULEX_BCKCALLUSERRPC,
			string32(backend),
			RPCGenericType(data),
			timeout
		);

		const std::uint8_t* ptr = retval.getData();
		BckUserRpcStatus status = static_cast<BckUserRpcStatus>(*ptr);
		if(status == BckUserRpcStatus::OK && retval.getSize() > sizeof(BckUserRpcStatus))
		{
			return std::make_tuple(
				status,
				RPCGenericType::FromData(retval.getData() + sizeof(BckUserRpcStatus), retval.getSize() - sizeof(BckUserRpcStatus))
			);
		}
		
		return std::make_tuple(status, RPCGenericType());
	}

	void MxBackend::terminate() const
	{
		LogDebug("[mxbackend] Received stop signal via terminate().");
		_prog_stop.store(true);
	}

	bool MxBackend::checkStatus() const
	{
		return _init_ok;
	}

	void MxBackend::deferExec(std::function<void()> func, std::int64_t delay, std::int64_t interval)
	{
		_io.schedule(func, delay, interval);
	}

	void MxBackend::init()
	{
		if(!_init_ok)
		{
			return;
		}

		// Register start/stop listeners
		// std::shared_ptr<MxBackend> self = std::shared_ptr<MxBackend>(this);
		rdb["/system/run/status"].watch([this](const RdbKeyName&, const RPCGenericType& value) {
			RunStatus status = static_cast<mulex::RunStatus>(value.asType<std::uint8_t>());

			if(status == RunStatus::RUNNING)
			{
				std::uint64_t no = rdb["/system/run/number"];
				LogTrace("[mxbacked] Run %llu starting.", no);
				if(_user_run_start)
				{
					_io.schedule([this, no](){ (*this.*_user_run_start)(no); });
				}
			}
			else if(status == RunStatus::STOPPED)
			{
				std::uint64_t no = rdb["/system/run/number"];
				LogTrace("[mxbacked] Run %llu stopping.", no);
				if(_user_run_stop)
				{
					_io.schedule([this, no](){ 
						_inside_stop_ctx.store(true);
						(*this.*_user_run_stop)(no);
						_inside_stop_ctx.store(false);
					});
				}
			}

			// Silently ignore other transition states
		});
		
		// Check if we want to setup a ctrl-C signal catcher handler
		if(!_bypass_int_sig)
		{
			// So we stop on ctrl-C
			LogMessage("[mxbackend] Running...");
			LogMessage("[mxbackend] Press ctrl-C to exit.");
			stop = mulex::SysSetupExitSignal();
		}

		// Check the current status and schedule start
		std::uint8_t status = rdb["/system/run/status"];
		if(static_cast<RunStatus>(status) == RunStatus::RUNNING)
		{
			std::uint64_t no = rdb["/system/run/number"];
			LogTrace("[mxbacked] Run %llu starting.", no);
			if(_user_run_start)
			{
				_io.schedule([this, no](){ (*this.*_user_run_start)(no); });
			}
		}

		// Register user rpc call function
		registerUserRpcEvent();
	}

	void MxBackend::spin()
	{
		if(!_init_ok)
		{
			return;
		}

		while(!*stop && !_prog_stop)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			std::this_thread::yield();
		}
	}

	MxRexDependencyManager::MxRexDependencyManager(
		const MxBackend* bck,
		const std::string& dependency,
		std::uint64_t cid,
		std::function<MsgEmitter&()> log_hook
	) : _dep_ptr(bck), _dep_name(dependency), _dep_cid(cid), _dep_hook(log_hook)
	{
	}

	MxRexDependencyManager::~MxRexDependencyManager()
	{
		auto failLog = [this](const std::string& msg){
			static const std::string log = "This backend requires dependency <%s> <0x%llx>. " + msg;
			if(_dep_fail == TERMINATE)
			{
				_dep_hook().error(log.c_str(), _dep_name.c_str(), _dep_cid);
				if(_dep_ptr) _dep_ptr->terminate();
				LogError("[rexdependencymanager] Required dependency marked as TERMINATE. Signal interrupt.");
			}
			else if(_dep_fail == LOG_WARN)
			{
				_dep_hook().warn(log.c_str(), _dep_name.c_str(), _dep_cid);
			}
			else if(_dep_fail == LOG_ERROR)
			{
				_dep_hook().error(log.c_str(), _dep_name.c_str(), _dep_cid);
			}
		};
		
		// Check for dependencies at the end of constructor
		if(!checkDependencyExists())
		{
			LogDebug(
				"[rexdependencymanager] Dependency <%s> <0x%llx> was not found on the mx registry.",
				_dep_name.c_str(),
				_dep_cid
			);
			failLog("Not found on system.");
			return;
		}

		if(!checkDependencyRunning())
		{
			LogDebug(
				"[rexdependencymanager] Dependency <%s> <0x%llx> not running.",
				_dep_name.c_str(),
				_dep_cid
			);
			if(_dep_req)
			{
				// Try to launch dependency via REx
				auto exp = SysGetConnectedExperiment();
				if(!exp.has_value())
				{
					failLog("Not connected to an experiment.");
				}
				else
				{
					RexCommandStatus status = exp.value()->_rpc_client->call<RexCommandStatus>(RPC_CALL_MULEX_REXSENDSTARTCOMMAND, _dep_cid);
					if(status != RexCommandStatus::BACKEND_START_OK)
					{
						failLog("Failed to start. Marked as required.");
					}
					else
					{
						LogDebug("[rexdependencymanager] Start dependency OK (<%s> | <0x%llx>).", _dep_name.c_str(), _dep_cid);
					}
				}
			}
			else
			{
				failLog("Not running. Not marked required.");
			}
		}

		LogDebug("[rexdependencymanager] Dependency <%s> already running.", _dep_name.c_str());
	}

	bool MxRexDependencyManager::checkDependencyExists()
	{
		RdbAccess rdb;
		auto extractCid = [](const std::string& key) -> std::uint64_t {
			std::string nkey = key.substr(0, key.find_last_of('/'));
			nkey = nkey.substr(nkey.find_last_of('/') + 1);
			return std::stoull(nkey, 0, 16);
		};

		auto readName = [&rdb](const std::string& key) -> std::string {
			std::string nkey = key.substr(0, key.find_last_of('/')) + "/name";
			mulex::mxstring<512> output = rdb[nkey];
			return output.c_str();
		};

		if(_dep_cid)
		{
			std::string key = "/system/backends/" + SysI64ToHexString(_dep_cid) + "/connected";
			bool exists = rdb[key].exists();
			if(exists)
			{
				_dep_name = readName(key);
			}

			return exists;
		}
		else if(!_dep_name.empty())
		{
			auto exp = SysGetConnectedExperiment();
			if(!exp.has_value())
			{
				return false;
			}

			std::vector<RdbKeyName> keys = exp.value()->_rpc_client->call<RPCGenericType>(RPC_CALL_MULEX_RDBLISTSUBKEYS, RdbKeyName("/system/backends/*/name"));
			for(const RdbKeyName& key : keys)
			{
				mulex::mxstring<512> kname = rdb[key.c_str()];
				std::string name = kname.c_str();
				
				if(name == _dep_name)
				{
					_dep_cid = extractCid(key.c_str());
					return true;
				}
			}
		}

		return false;
	}

	bool MxRexDependencyManager::checkDependencyRunning()
	{
		RdbAccess rdb;
		bool running = rdb["/system/backends/" + SysI64ToHexString(_dep_cid) + "/connected"];
		return running;
	}

	MxRexDependencyManager& MxRexDependencyManager::required(bool r)
	{
		_dep_req = r;
		return *this;
	}

	MxRexDependencyManager& MxRexDependencyManager::onFail(const RDMFailFlags& flag)
	{
		_dep_fail = flag;
		return *this;
	}

	void MxBackend::logRunWriteFile(const std::string& alias, const std::vector<std::uint8_t>& buffer)
	{
		logRunWriteFile(alias, buffer.data(), buffer.size());
	}

	void MxBackend::logRunWriteFile(const std::string& alias, const std::uint8_t* buffer, std::uint64_t size)
	{
		if(!SysGetConnectedExperiment().has_value())
		{
			LogError("[mxbackend] Failed to log run file. Not connected to an experiment.");
			return;
		}

		std::uint8_t status = rdb["/system/run/status"];
		if(static_cast<RunStatus>(status) != RunStatus::RUNNING && !_inside_stop_ctx) // Run if we are inside stop ctx
		{
			LogError("[mxbackend] Failed to log run file. Run not running.");
			return;
		}
		
		static constexpr std::uint64_t CHUNK_SIZE = 64 * 1024; // Upload chunks of 64KB
		auto ext_pos = alias.find_last_of('.');
		string32 deduced_mime = "application/octet-stream";
		if(ext_pos != std::string::npos)
		{
			deduced_mime = FdbGetMimeFromExt(alias.substr(ext_pos + 1));
		}

		FdbHandle handle = _experiment->_rpc_client->call<mulex::FdbHandle>(RPC_CALL_MULEX_FDBCHUNKEDUPLOADSTART, deduced_mime);

		for(std::uint64_t i = 0; i < size; i += CHUNK_SIZE)
		{
			const std::uint8_t* ptr = buffer + i;
			const std::uint64_t sz = (i + CHUNK_SIZE) > size ? (size - i) : CHUNK_SIZE;
			_experiment->_rpc_client->call<bool>(RPC_CALL_MULEX_FDBCHUNKEDUPLOADSEND, mxstring<512>(handle), RPCGenericType::FromData(ptr, sz));
		}

		if(!_experiment->_rpc_client->call<bool>(RPC_CALL_MULEX_FDBCHUNKEDUPLOADEND, handle))
		{
			LogError("[mxbackend] Failed to log run file. Failed to upload file.");
			return;
		}

		RunLogFileMetadata meta = {
			._runno = rdb["/system/run/number"],
			._handle = handle,
			._alias = alias
		};

		if(!_experiment->_rpc_client->call<bool>(RPC_CALL_MULEX_RUNLOGFILE, meta))
		{
			LogError("[mxbackend] Failed to log run file. Failed to register metadata.");
			return;
		}

		LogTrace("[mxbackend] Log run file OK.");
	}

	void MxBackend::bypassIntHandler(bool value)
	{
		_bypass_int_sig = value;
	}
} // namespace mulex
