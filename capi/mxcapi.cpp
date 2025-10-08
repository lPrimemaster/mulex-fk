#include "../mxcapi.h"
#include <vector>

// NOTE: (César) This is required because MxBackend
// 				 has a virtual dtor
// 				 Also might become usefull for some
// 				 generalizations
class CEmulatedBackend : public mulex::MxBackend
{
public:
	CEmulatedBackend(int argc, char* argv[]) : mulex::MxBackend(argc, argv), plog(log), prdb(rdb)
	{
	}

	inline void publicEventDispatch(const std::string& evt, const std::uint8_t* data, std::uint64_t size)
	{
		dispatchEvent(evt, data, size);
	}

	inline void publicEventRegister(const std::string& evt)
	{
		registerEvent(evt);
	}

	inline void publicEventSubscribe(const std::string& evt, CMxBackendEvtCallbackFunc func)
	{
		subscribeEvent(evt, func);
	}

	inline void publicEventUnsubscribe(const std::string& evt)
	{
		unsubscribeEvent(evt);
	}

	inline void publicRpcRegister(CMxBackendRpcCallbackFunc func)
	{
		if(func)
		{
			_rpc_func = func;
			registerUserRpc(&CEmulatedBackend::internalRpcCallback);
		}
	}

	inline void bypassInterruptSignal(bool value)
	{
		bypassIntHandler(value);
	}

	inline void publicStartStopRegister(CMxBackendRunStartStopCallbackFunc start, CMxBackendRunStartStopCallbackFunc stop)
	{
		if(start && stop)
		{
			_start_func = start;
			_stop_func = stop;
			registerRunStartStop(&CEmulatedBackend::internalStartCallback, &CEmulatedBackend::internalStopCallback);
		}
		else if(start)
		{
			_start_func = start;
			registerRunStartStop<CEmulatedBackend>(&CEmulatedBackend::internalStartCallback, nullptr);
		}
		else if(stop)
		{
			_stop_func = stop;
			registerRunStartStop<CEmulatedBackend>(nullptr, &CEmulatedBackend::internalStopCallback);
		}
	}

	inline void publicStatusSet(const std::string& status, const std::string& color)
	{
		setStatus(status, color);
	}

	inline std::tuple<mulex::BckUserRpcStatus, mulex::RPCGenericType> publicRpcCall(const std::string& backend, const std::uint8_t* data, std::uint64_t size, std::int64_t timeout)
	{
		return callUserRpc(backend, std::vector<std::uint8_t>(data, data + size), timeout);
	}

	inline void publicRunLogWriteFile(const std::string& alias, const std::uint8_t* buffer, std::uint64_t size)
	{
		logRunWriteFile(alias, buffer, size);
	}

private:
	inline mulex::RPCGenericType internalRpcCallback(const std::vector<std::uint8_t>& data)
	{
		// NOTE: (César) retval should never be managed by us
		// 				 the _rpc_func is responsible for the
		// 				 returned memory lifetime
		char* retval = _rpc_func(reinterpret_cast<const char*>(data.data()), data.size());
		if(!retval)
		{
			// nullptr is void
			return {};
		}

		// Size first
		std::uint64_t size = *reinterpret_cast<std::uint64_t*>(retval);
		
		// Data later
		return mulex::RPCGenericType::FromData(reinterpret_cast<std::uint8_t*>(retval) + sizeof(std::uint64_t), size);
	}

	inline void internalStartCallback(std::uint64_t no)
	{
		_start_func(no);
	}

	inline void internalStopCallback(std::uint64_t no)
	{
		_stop_func(no);
	}

private:
	CMxBackendRpcCallbackFunc _rpc_func = nullptr;
	CMxBackendRunStartStopCallbackFunc _start_func = nullptr;
	CMxBackendRunStartStopCallbackFunc _stop_func = nullptr;

public:
	mulex::MsgEmitter& plog;
	mulex::RdbAccess& prdb;
};

inline static CEmulatedBackend* CMxGetBackendPointer(CMxContext* ctx)
{
	return reinterpret_cast<CEmulatedBackend*>(ctx->_backend);
}

static bool CMxCheckContext(CMxContext* ctx)
{
	if(ctx == nullptr || ctx->_backend == nullptr)
	{
		mulex::LogError("[mxcapi] Invalid context. Aborting...");
		return false;
	}
	return true;
}

static CMxRpcReturn CMxRpcReturnAllocateFromCall(std::uint8_t status, mulex::RPCGenericType& data)
{
	CMxRpcReturn output;
	output.status = status;
	output.size = data.getSize();
	output.data = nullptr;

	if(output.size > 0)
	{
		output.data = new std::uint8_t[data.getSize()];
		std::memcpy(output.data, data.getData(), data.getSize());
	}

	return output;
}

C_LINKAGE CMxContext* CMxContextCreate()
{
	CMxContext* ctx = new CMxContext();

	if(ctx == nullptr)
	{
		mulex::LogError("[mxcapi] Failed to allocate CMxContext.");
		return nullptr;
	}

	mulex::LogTrace("[mxcapi] CMxContextCreate() OK.");
	return ctx;
}

C_LINKAGE void CMxContextDestroy(CMxContext** ctx)
{
	if(ctx && *ctx)
	{
		delete *ctx;
		mulex::LogTrace("[mxcapi] CMxContextDestroy() OK.");
	}
}

C_LINKAGE void CMxBinaryOverrideName(const char* name)
{
	mulex::SysOverrideBinaryName(name);
}

C_LINKAGE void CMxBackendCreate(CMxContext* ctx, int argc, char* argv[])
{
	CEmulatedBackend* bck = new CEmulatedBackend(argc, argv);
	if(!bck)
	{
		mulex::LogError("[mxcapi] CMxBackendCreate: Failed to allocate.");
		ctx->_backend = nullptr;
		return;
	}

	if(!bck->checkStatus())
	{
		mulex::LogError("[mxcapi] CMxBackendCreate: Bad backend status.");
		ctx->_backend = nullptr;
		delete bck;
		return;
	}

	mulex::LogTrace("[mxcapi] CMxBackendCreate() OK.");
	ctx->_backend = bck;
}

C_LINKAGE void CMxBackendDestroy(CMxContext* ctx)
{
	if(CMxCheckContext(ctx))
	{
		CEmulatedBackend* bck = CMxGetBackendPointer(ctx);

		if(bck)
		{
			// NOTE: (César) C++ dtor cleans up after itself
			delete bck;
			ctx->_backend = nullptr;
			mulex::LogTrace("[mxcapi] CMxBackendDestroy() OK.");
		}
	}
}

// Emulate event dispatch
C_LINKAGE void CMxBackendEventDispatch(CMxContext* ctx, const char* evt, const std::uint8_t* data, std::uint64_t size)
{
	if(CMxCheckContext(ctx))
	{
		CEmulatedBackend* bck = CMxGetBackendPointer(ctx);
		bck->publicEventDispatch(std::string(evt), data, size);
	}
}

C_LINKAGE void CMxBackendEventRegister(CMxContext* ctx, const char* evt)
{
	if(CMxCheckContext(ctx))
	{
		CEmulatedBackend* bck = CMxGetBackendPointer(ctx);
		bck->publicEventRegister(std::string(evt));
	}
}

C_LINKAGE void CMxBackendEventSubscribe(CMxContext* ctx, const char *evt, CMxBackendEvtCallbackFunc func)
{
	if(CMxCheckContext(ctx))
	{
		CEmulatedBackend* bck = CMxGetBackendPointer(ctx);
		bck->publicEventSubscribe(std::string(evt), func);
	}
}

C_LINKAGE void CMxBackendEventUnsubscribe(CMxContext* ctx, const char *evt)
{
	if(CMxCheckContext(ctx))
	{
		CEmulatedBackend* bck = CMxGetBackendPointer(ctx);
		bck->publicEventUnsubscribe(std::string(evt));
	}
}

// Emulate user RPC
C_LINKAGE void CMxBackendRpcRegister(CMxContext* ctx, CMxBackendRpcCallbackFunc func)
{
	if(CMxCheckContext(ctx))
	{
		CEmulatedBackend* bck = CMxGetBackendPointer(ctx);
		bck->publicRpcRegister(func);
	}
}

// Emulate start/stop
// NOTE: (César) The python GIL needs to be acquired when calling python functions
// 				 The start/stop functions run inside the _io loop thread
C_LINKAGE void CMxBackendRunRegisterStartStop(CMxContext* ctx, CMxBackendRunStartStopCallbackFunc start, CMxBackendRunStartStopCallbackFunc stop)
{
	if(CMxCheckContext(ctx))
	{
		CEmulatedBackend* bck = CMxGetBackendPointer(ctx);
		bck->publicStartStopRegister(start, stop);
	}
}

// Emulate setStatus
C_LINKAGE void CMxBackendStatusSet(CMxContext* ctx, const char* status, const char* color)
{
	if(CMxCheckContext(ctx))
	{
		CEmulatedBackend* bck = CMxGetBackendPointer(ctx);
		bck->publicStatusSet(status, color);
	}
}

// Emulate registerDependency
// TODO: (César)

// Emulate calling user RPC
C_LINKAGE CMxRpcReturn CMxBackendRpcCall(CMxContext* ctx, const char* backend, const std::uint8_t* data, std::uint64_t size, std::int64_t timeout)
{
	if(CMxCheckContext(ctx))
	{
		CEmulatedBackend* bck = CMxGetBackendPointer(ctx);
		auto [status, value] = bck->publicRpcCall(backend, data, size, timeout);
		return CMxRpcReturnAllocateFromCall(static_cast<std::uint8_t>(status), value);
	}

	// NOTE: (César): Use NO_SUCH_BACKEND when the context is invalid
	// 				  One could generate a different code or flags just
	// 				  for this, but it is self-explanatory when the error
	// 				  log is displayed before
	mulex::LogError("[mxcapi] Invalid context. CMxBackendRpcCall() issued NO_SUCH_BACKEND to signal this.");
	return { static_cast<std::uint8_t>(mulex::BckUserRpcStatus::NO_SUCH_BACKEND), nullptr, 0 };
}

C_LINKAGE MX_API void CMxBackendRpcFreeReturn(CMxRpcReturn ret)
{
	if(ret.data && ret.size > 0)
	{
		delete[] ret.data;
	}
}

// Emulate writting to run log
C_LINKAGE void CMxBackendRunLogWriteFile(CMxContext* ctx, const char* alias, const std::uint8_t* buffer, std::uint64_t size)
{
	if(CMxCheckContext(ctx))
	{
		CEmulatedBackend* bck = CMxGetBackendPointer(ctx);
		bck->publicRunLogWriteFile(alias, buffer, size);
	}
}

// TODO: (César) Check if it makes sense to emulate execution deferral

// Emulate init
C_LINKAGE bool CMxBackendInit(CMxContext* ctx)
{
	if(CMxCheckContext(ctx))
	{
		CEmulatedBackend* bck = CMxGetBackendPointer(ctx);

		// We don't want to setup an interrupt signal handler here
		// Let the the underlying implementation control this
		// NOTE: (César) Needs to be before init()
		bck->bypassInterruptSignal(true);

		bck->init();

		return bck->checkStatus();
	}

	return false;
}

C_LINKAGE void CMxMsgEmitterLogError(CMxContext* ctx, const char* msg)
{
	if(CMxCheckContext(ctx))
	{
		CEmulatedBackend* bck = CMxGetBackendPointer(ctx);
		if(bck)
		{
			bck->plog.error(msg);
		}
	}
}

C_LINKAGE void CMxMsgEmitterLogWarning(CMxContext* ctx, const char* msg)
{
	if(CMxCheckContext(ctx))
	{
		CEmulatedBackend* bck = CMxGetBackendPointer(ctx);
		if(bck)
		{
			bck->plog.warn(msg);
		}
	}
}

C_LINKAGE void CMxMsgEmitterLogInfo(CMxContext* ctx, const char* msg)
{
	if(CMxCheckContext(ctx))
	{
		CEmulatedBackend* bck = CMxGetBackendPointer(ctx);
		if(bck)
		{
			bck->plog.info(msg);
		}
	}
}

C_LINKAGE void CMxMsgEmitterAttachLogger(CMxContext* ctx, bool attach)
{
	if(CMxCheckContext(ctx))
	{
		CEmulatedBackend* bck = CMxGetBackendPointer(ctx);
		if(bck)
		{
			bck->plog.attachLogger(attach);
		}
	}
}

C_LINKAGE bool CMxRdbCreate(CMxContext* ctx, const char* key, std::uint8_t type, const std::uint8_t* data, std::uint64_t size)
{
	if(CMxCheckContext(ctx))
	{
		CEmulatedBackend* bck = CMxGetBackendPointer(ctx);
		return bck->prdb[key].create(static_cast<mulex::RdbValueType>(type), mulex::RPCGenericType::FromData(data, size));
	}
	return false;
}

C_LINKAGE bool CMxRdbDelete(CMxContext* ctx, const char* key)
{
	if(CMxCheckContext(ctx))
	{
		CEmulatedBackend* bck = CMxGetBackendPointer(ctx);
		return bck->prdb[key].erase();
	}
	return false;
}

C_LINKAGE bool CMxRdbExists(CMxContext* ctx, const char* key)
{
	if(CMxCheckContext(ctx))
	{
		CEmulatedBackend* bck = CMxGetBackendPointer(ctx);
		return bck->prdb[key].exists();
	}
	return false;
}

C_LINKAGE bool CMxRdbRead(CMxContext* ctx, const char* key, std::uint8_t* data, std::uint64_t* rsize)
{
	if(CMxCheckContext(ctx))
	{
		CEmulatedBackend* bck = CMxGetBackendPointer(ctx);
		mulex::RPCGenericType rgt = bck->prdb[key];
		if(!rsize || *rsize == 0)
		{
			mulex::LogError("[mxcapi] CMxRdbRead: Failed to read key. Must provide a valid buffer size.");
			return false;
		}

		if(!data)
		{
			mulex::LogError("[mxcapi] CMxRdbRead: Failed to read key. Must provide a valid buffer.");
			return false;
		}

		if(*rsize < rgt.getSize())
		{
			mulex::LogError("[mxcapi] CMxRdbRead: Failed to read key. Provided buffer is too small.");
			return false;
		}

		std::memcpy(data, rgt.getData(), rgt.getSize());
		*rsize = rgt.getSize();
		return true;
	}
	return false;
}

C_LINKAGE bool CMxRdbWrite(CMxContext* ctx, const char* key, const std::uint8_t* data, std::uint64_t size)
{
	if(CMxCheckContext(ctx))
	{
		CEmulatedBackend* bck = CMxGetBackendPointer(ctx);
		bck->prdb[key] = mulex::RPCGenericType::FromData(data, size);
		return true;
	}
	return false;
}

C_LINKAGE void CMxRdbWatch(CMxContext* ctx, const char* key, CMxRdbWatchCallback func)
{
	if(CMxCheckContext(ctx))
	{
		CEmulatedBackend* bck = CMxGetBackendPointer(ctx);
		bck->prdb[key].watch([func](const mulex::RdbKeyName& key, const mulex::RPCGenericType& value) {
			func(key.c_str(), value._data.data(), value.getSize());
		});
	}
}

C_LINKAGE void CMxRdbUnwatch(CMxContext* ctx, const char* key)
{
	if(CMxCheckContext(ctx))
	{
		CEmulatedBackend* bck = CMxGetBackendPointer(ctx);
		bck->prdb[key].unwatch();
	}
}

C_LINKAGE std::uint8_t CMxRdbKeyType(CMxContext* ctx, const char* key)
{
	if(CMxCheckContext(ctx))
	{
		CEmulatedBackend* bck = CMxGetBackendPointer(ctx);
		return static_cast<std::uint8_t>(bck->prdb[key].type());
	}
	return 0;
}
