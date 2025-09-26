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
#define C_LINKAGE
#endif

typedef void CMxContext;
typedef void (*CMxBackendEvtCallbackFunc)(const std::uint8_t* data, std::uint64_t len, const std::uint8_t* userdata);
typedef void* (*CMxBackendRpcCallbackFunc)(const std::uint8_t* data, std::uint64_t len);
typedef void (*CMxBackendRunStartStopCallbackFunc)(std::uint64_t);

// NOTE: (César)
// Allow for overriding the backend name
// This is specially usefull for python
// We don't want all of the the backends to be named python3
// This also prevents the lockfile from locking all python backends
C_LINKAGE MX_API void CMxBinaryOverrideName(const char* name);

// Emulate Ctor / Dtor
C_LINKAGE MX_API CMxContext* CMxBackendCreate(int argc, char* argv[]);
C_LINKAGE MX_API void CMxBackendDestroy(CMxContext* ctx);

// Emulate event dispatch
C_LINKAGE MX_API void CMxBackendEventDispatch(CMxContext* ctx, const char* evt, const std::uint8_t* data, std::uint64_t size);
C_LINKAGE MX_API void CMxBackendEventRegister(CMxContext* ctx, const char* evt);
C_LINKAGE MX_API void CMxBackendEventSubscribe(CMxContext* ctx, const char *evt, CMxBackendEvtCallbackFunc func);
C_LINKAGE MX_API void CMxBackendEventUnsubscribe(CMxContext* ctx, const char *evt);

// Emulate user RPC
C_LINKAGE MX_API void CMxBackendRpcRegister(CMxContext* ctx, CMxBackendRpcCallbackFunc func);

// Emulate start/stop
C_LINKAGE MX_API void CMxBackendRunRegisterStartStop(CMxContext* ctx, CMxBackendRunStartStopCallbackFunc start, CMxBackendRunStartStopCallbackFunc stop);

// Emulate setStatus
C_LINKAGE MX_API void CMxBackendStatusSet(CMxContext* ctx, const char* status, const char* color);

// Emulate registerDependency
// TODO: (César)

// Emulate calling user RPC
// TODO: (César)

// Emulate writting to run log
C_LINKAGE MX_API void CMxBackendRunLogWriteFile(CMxContext* ctx, const char* alias, const std::uint8_t* buffer, std::uint64_t size);

// TODO: (César) Check if it makes sense to emulate execution deferal

// Emulate init
C_LINKAGE MX_API bool CMxBackendInit(CMxContext* ctx);

// NOTE: (César) as we only use this API for python
// 				 implementing sping and terminate
// 				 might not make sense
