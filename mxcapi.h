// Author : César Godinho
// Date   : 16/09/2025
// Brief  : C-API compatibility layer of mxbackend

#pragma once
#include "mxbackend.h"
#include <cstdint>

#ifdef __cplusplus
#define C_LINKAGE extern "C"
#else
#define C_LINKAGE
#endif

typedef void CMxContext;
typedef void (*CMxBackendEvtCallbackFunc)(const std::uint8_t* data, std::uint64_t len, const std::uint8_t* userdata);
typedef void (*CMxBackendRpcCallbackFunc)(const std::uint8_t* data, std::uint64_t len);
typedef void (*CMxBackendRunStartStopCallbackFunc)(std::uint64_t);

// Emulate Ctor / Dtor
C_LINKAGE CMxContext* CMxBackendCreate(int argc, char* argv[]);
C_LINKAGE void CMxBackendDestroy(CMxContext* ctx);

// Emulate event dispatch
C_LINKAGE void CMxBackendEventDispatch(const CMxContext* ctx, const char* evt, const std::uint8_t* data, std::uint64_t size);
C_LINKAGE void CMxBackendEventRegister(const CMxContext* ctx, const char* evt);
C_LINKAGE void CMxBackendEventSubscribe(const CMxContext* ctx, const char *evt, CMxBackendEvtCallbackFunc func);
C_LINKAGE void CMxBackendEventUnsubscribe(const CMxContext* ctx, const char *evt);

// Emulate user RPC
C_LINKAGE void CMxBackendRpcRegister(const CMxContext* ctx, CMxBackendRpcCallbackFunc func);

// Emulate start/stop
C_LINKAGE void CMxBackendRunRegisterStartStop(const CMxContext* ctx, CMxBackendRunStartStopCallbackFunc start, CMxBackendRunStartStopCallbackFunc stop);

// Emulate setStatus
C_LINKAGE void CMxBackendStatusSet(const CMxContext* ctx, const char* status, const char* color);

// Emulate registerDependency
// TODO: (César)

// Emulate calling user RPC
// TODO: (César)

// Emulate writting to run log
C_LINKAGE void CMxBackendRunLogWriteFile(const CMxContext* ctx, const char* alias, const std::uint8_t* buffer, std::uint64_t size);

// TODO: (César) Check if it makes sense to emulate execution deferal

// Emulate init
C_LINKAGE void CMxBackendInit(const CMxContext* ctx);

// NOTE: (César) as we only use this API for python
// 				 implementing sping and terminate
// 				 might not make sense
