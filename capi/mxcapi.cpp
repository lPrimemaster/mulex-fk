#include "../mxcapi.h"
#include <vector>

// NOTE: (César) This is required because MxBackend
// 				 has a virtual dtor
// 				 Also might become usefull for some
// 				 generalizations
class CEmulatedBackend : public mulex::MxBackend
{
public:
	CEmulatedBackend(int argc, char* argv[]) : mulex::MxBackend(argc, argv)
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

	inline void publicRunLogWriteFile(const std::string& alias, const std::uint8_t* buffer, std::uint64_t size)
	{
		logRunWriteFile(alias, buffer, size);
	}

private:
	inline mulex::RPCGenericType internalRpcCallback(const std::vector<std::uint8_t>& data)
	{
		void* retval = _rpc_func(data.data(), data.size());
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
};

inline static CEmulatedBackend* CMxGetBackendPointer(CMxContext* ctx)
{
	return reinterpret_cast<CEmulatedBackend*>(ctx);
}

static bool CMxCheckContext(CMxContext* ctx)
{
	if(ctx == nullptr)
	{
		mulex::LogError("[mxcapi] Invalid context. Aborting...");
		return false;
	}
	return true;
}

C_LINKAGE void CMxBinaryOverrideName(const char* name)
{
	mulex::SysOverrideBinaryName(name);
}

C_LINKAGE CMxContext* CMxBackendCreate(int argc, char* argv[])
{
	CEmulatedBackend* bck = new CEmulatedBackend(argc, argv);
	if(!bck)
	{
		return nullptr;
	}

	if(!bck->checkStatus())
	{
		return nullptr;
	}

	return reinterpret_cast<CMxContext*>(bck);
}

C_LINKAGE void CMxBackendDestroy(CMxContext* ctx)
{
	if(CMxCheckContext(ctx))
	{
		CEmulatedBackend* bck = CMxGetBackendPointer(ctx);

		// NOTE: (César) C++ dtor cleans up after itself
		delete bck;
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
// TODO: (César)

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
