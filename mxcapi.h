// Author : César Godinho
// Date   : 16/09/2025
// Brief  : C-API compatibility layer of mxbackend

#pragma once
#include "mxbackend.h"
#include <cstdint>

#if defined _WIN32
#define MX_API __declspec(dllexport)
#else
#define MX_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
#define C_LINKAGE extern "C"
#else
// This should never happen
#define C_LINKAGE
#endif

// Context struct
C_LINKAGE struct CMxContext
{
	void* _backend;
};

C_LINKAGE MX_API CMxContext* CMxContextCreate();
C_LINKAGE MX_API void CMxContextDestroy(CMxContext** ctx);

// ==========
// MxBackend
// ==========
typedef void (*CMxBackendEvtCallbackFunc)(const std::uint8_t* data, std::uint64_t len, const std::uint8_t* userdata);
typedef char* (*CMxBackendRpcCallbackFunc)(const char* data, std::uint64_t len);
typedef void (*CMxBackendRunStartStopCallbackFunc)(std::uint64_t);

// NOTE: (César) This is not managed by us and
// 				 should be freed on the caller side
C_LINKAGE struct CMxRpcReturn
{
	std::uint8_t status;
	std::uint8_t* data;
	std::uint64_t size;
};

// NOTE: (César)
// Allow for overriding the backend name
// This is specially usefull for python
// We don't want all of the the backends to be named python3
// This also prevents the lockfile from locking all python backends
C_LINKAGE MX_API void CMxBinaryOverrideName(const char* name);
C_LINKAGE MX_API void CMxBackendCreate(CMxContext* ctx, int argc, char* argv[]);
C_LINKAGE MX_API void CMxBackendDestroy(CMxContext* ctx);
C_LINKAGE MX_API void CMxBackendEventDispatch(CMxContext* ctx, const char* evt, const std::uint8_t* data, std::uint64_t size);
C_LINKAGE MX_API void CMxBackendEventRegister(CMxContext* ctx, const char* evt);
C_LINKAGE MX_API void CMxBackendEventSubscribe(CMxContext* ctx, const char *evt, CMxBackendEvtCallbackFunc func);
C_LINKAGE MX_API void CMxBackendEventUnsubscribe(CMxContext* ctx, const char *evt);
C_LINKAGE MX_API void CMxBackendRpcRegister(CMxContext* ctx, CMxBackendRpcCallbackFunc func);
C_LINKAGE MX_API void CMxBackendRunRegisterStartStop(CMxContext* ctx, CMxBackendRunStartStopCallbackFunc start, CMxBackendRunStartStopCallbackFunc stop);
C_LINKAGE MX_API void CMxBackendStatusSet(CMxContext* ctx, const char* status, const char* color);
C_LINKAGE MX_API CMxRpcReturn CMxBackendRpcCall(CMxContext* ctx, const char* backend, const std::uint8_t* data, std::uint64_t size, std::int64_t timeout);
C_LINKAGE MX_API void CMxBackendRpcFreeReturn(CMxRpcReturn ret);
C_LINKAGE MX_API void CMxBackendRunLogWriteFile(CMxContext* ctx, const char* alias, const std::uint8_t* buffer, std::uint64_t size);
C_LINKAGE MX_API bool CMxBackendInit(CMxContext* ctx);

// TODO: (César): Emulate registerDependency

// NOTE: (César): as we only use this API for python
// 				  implementing spin and terminate
// 				  might not make sense

// ==========
// MsgEmitter
// ==========
C_LINKAGE MX_API void CMxMsgEmitterLogError(CMxContext* ctx, const char* msg);
C_LINKAGE MX_API void CMxMsgEmitterLogWarning(CMxContext* ctx, const char* msg);
C_LINKAGE MX_API void CMxMsgEmitterLogInfo(CMxContext* ctx, const char* msg);
C_LINKAGE MX_API void CMxMsgEmitterAttachLogger(CMxContext* ctx, bool attach);

// ==========
// RdbAccess
// ==========
typedef void (*CMxRdbWatchCallback)(const char* key, const std::uint8_t* value, std::uint64_t size);

C_LINKAGE MX_API bool CMxRdbCreate(CMxContext* ctx, const char* key, std::uint8_t type, const std::uint8_t* data, std::uint64_t size);
C_LINKAGE MX_API bool CMxRdbDelete(CMxContext* ctx, const char* key);
C_LINKAGE MX_API bool CMxRdbExists(CMxContext* ctx, const char* key);
C_LINKAGE MX_API bool CMxRdbRead(CMxContext* ctx, const char* key, std::uint8_t* data, std::uint64_t* rsize);
C_LINKAGE MX_API bool CMxRdbWrite(CMxContext* ctx, const char* key, const std::uint8_t* data, std::uint64_t size);
C_LINKAGE MX_API void CMxRdbWatch(CMxContext* ctx, const char* key, CMxRdbWatchCallback func);
C_LINKAGE MX_API void CMxRdbUnwatch(CMxContext* ctx, const char* key);
C_LINKAGE MX_API std::uint8_t CMxRdbKeyType(CMxContext* ctx, const char* key);
