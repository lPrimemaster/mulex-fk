# Brief  : Python wrapper for mxbackend
# Author : César Godinho
# Date   : 07/10/2025

from ._platform import get_lib_location
from ._logger import logger
from ._exceptions import InvalidKey, RpcCallFailed
import ctypes as ct
import os
import sys
from abc import ABC  # , abstractmethod
import struct
from typing import Union, Callable, Any, List
from enum import Enum


def _ensure_gil(func):
    def wrapper(*args, **kwargs):
        state = ct.pythonapi.PyGILState_Ensure()
        try:
            return func(*args, **kwargs)
        finally:
            ct.pythonapi.PyGILState_Release(state)
    return wrapper


class Backend(ABC):
    RpcReturn = Union[
        int,
        float,
        bytes,
        str,
        bool,
        ct.c_uint8,
        ct.c_uint16,
        ct.c_uint32,
        ct.c_uint64,
        ct.c_int8,
        ct.c_int16,
        ct.c_int32,
        ct.c_int64,
        ct.c_float,
        ct.c_double,
        ct.c_bool
    ]

    class LogType(Enum):
        INFO = 1
        WARN = 2
        ERROR = 3

    class RpcReturnData(ct.Structure):
        _fields_ = [
            ('status', ct.c_uint8),
            ('data', ct.POINTER(ct.c_uint8)),
            ('size', ct.c_uint64)
        ]

    def __init__(self, name: str):
        lib_loc = get_lib_location()

        if lib_loc is None:
            logger.error('Failed to locate the library object.')
            return

        try:
            self._lib = ct.CDLL(lib_loc)
        except Exception as e:
            logger.error('Failed to load the library object.')
            logger.error(f'Error: {e}.')
            return

        # Context
        self._lib.CMxContextCreate.restype = ct.c_void_p
        self._lib.CMxContextDestroy.argtypes = [ct.POINTER(ct.c_void_p)]

        # Binary name
        self._lib.CMxBinaryOverrideName.argtypes = [ct.c_char_p]

        # Backend
        self._lib.CMxBackendCreate.argtypes = [ct.c_void_p, ct.c_int32, ct.POINTER(ct.c_char_p)]
        self._lib.CMxBackendDestroy.argtypes = [ct.c_void_p]
        self._lib.CMxBackendEventDispatch.argtypes = [ct.c_void_p, ct.c_char_p, ct.POINTER(ct.c_uint8), ct.c_uint64]
        self._lib.CMxBackendEventRegister.argtypes = [ct.c_void_p, ct.c_char_p]
        self._lib.CMxBackendEventSubscribe.argtypes = [ct.c_void_p, ct.c_char_p, ct.CFUNCTYPE(None, ct.POINTER(ct.c_uint8), ct.c_uint64, ct.POINTER(ct.c_uint8))]
        self._lib.CMxBackendEventUnsubscribe.argtypes = [ct.c_void_p, ct.c_char_p]
        self._lib.CMxBackendRpcRegister.argtypes = [ct.c_void_p, ct.CFUNCTYPE(ct.c_char_p, ct.POINTER(ct.c_char), ct.c_uint64)]
        self._lib.CMxBackendRunRegisterStartStop.argtypes = [ct.c_void_p, ct.CFUNCTYPE(None, ct.c_uint64), ct.CFUNCTYPE(None, ct.c_uint64)]
        self._lib.CMxBackendStatusSet.argtypes = [ct.c_void_p, ct.c_char_p, ct.c_char_p]
        self._lib.CMxBackendRpcCall.argtypes = [ct.c_void_p, ct.c_char_p, ct.POINTER(ct.c_uint8), ct.c_uint64, ct.c_int64]
        self._lib.CMxBackendRpcCall.restype = Backend.RpcReturnData
        self._lib.CMxBackendRpcFreeReturn.argtypes = [Backend.RpcReturnData]
        self._lib.CMxBackendRunLogWriteFile.argtypes = [ct.c_void_p, ct.c_char_p, ct.POINTER(ct.c_uint8), ct.c_uint64]
        self._lib.CMxBackendInit.argtypes = [ct.c_void_p]

        # Logger
        self._lib.CMxMsgEmitterLogInfo.argtypes = [ct.c_void_p, ct.c_char_p]
        self._lib.CMxMsgEmitterLogWarning.argtypes = [ct.c_void_p, ct.c_char_p]
        self._lib.CMxMsgEmitterLogError.argtypes = [ct.c_void_p, ct.c_char_p]
        self._lib.CMxMsgEmitterAttachLogger.argtypes = [ct.c_void_p, ct.c_bool]

        # Rdb
        self._lib.CMxRdbCreate.argtypes = [ct.c_void_p, ct.c_char_p, ct.c_uint8, ct.POINTER(ct.c_uint8), ct.c_uint64]
        self._lib.CMxRdbCreate.restype = ct.c_bool
        self._lib.CMxRdbDelete.argtypes = [ct.c_void_p, ct.c_char_p]
        self._lib.CMxRdbDelete.restype = ct.c_bool
        self._lib.CMxRdbExists.argtypes = [ct.c_void_p, ct.c_char_p]
        self._lib.CMxRdbExists.restype = ct.c_bool
        self._lib.CMxRdbRead.argtypes = [ct.c_void_p, ct.c_char_p, ct.POINTER(ct.c_uint8), ct.POINTER(ct.c_uint64)]
        self._lib.CMxRdbRead.restype = ct.c_bool
        self._lib.CMxRdbWrite.argtypes = [ct.c_void_p, ct.c_char_p, ct.POINTER(ct.c_uint8), ct.c_uint64]
        self._lib.CMxRdbWrite.restype = ct.c_bool
        self._lib.CMxRdbWatch.argtypes = [ct.c_void_p, ct.c_char_p, ct.CFUNCTYPE(None, ct.POINTER(ct.c_char), ct.POINTER(ct.c_uint8), ct.c_uint64)]
        self._lib.CMxRdbUnwatch.argtypes = [ct.c_void_p, ct.c_char_p]
        self._lib.CMxRdbKeyType.argtypes = [ct.c_void_p, ct.c_char_p]
        self._lib.CMxRdbKeyType.restype = ct.c_uint8

        # NOTE: (César) Override the binary name
        #               This makes it so that the correct backend name
        #               appears under the mx system
        self._lib.CMxBinaryOverrideName(
            ct.create_string_buffer(
                os.path.basename(f'{name}'.encode('utf-8'))
            )
        )

        # TODO: (César) This can fail?? Check
        # Create context
        self._ctx = self._lib.CMxContextCreate()

        # Create the backend
        nargs = len(sys.argv)
        cargv = (ct.c_char_p * nargs)()
        cargv[:] = [arg.encode('utf-8') for arg in sys.argv]
        self._lib.CMxBackendCreate(
            self._ctx,
            ct.c_int32(nargs),
            cargv
        )

        # Init shared buffer
        self._shared_buff = ct.create_string_buffer(1024)

        # Register user functions
        self._run_start_cb = self._cb_register(
            ct.CFUNCTYPE(None, ct.c_uint64),
            self._run_start_entry
        )

        self._run_stop_cb = self._cb_register(
            ct.CFUNCTYPE(None, ct.c_uint64),
            self._run_stop_entry
        )

        self._user_rpc_cb = self._cb_register(
            ct.CFUNCTYPE(ct.c_char_p, ct.POINTER(ct.c_char), ct.c_uint64),
            self._user_rpc_entry
        )

        self._rdb_watches = {}
        self._sub_events = {}

        self._lib.CMxBackendRunRegisterStartStop(
            self._ctx,
            self._run_start_cb,
            self._run_stop_cb
        )

        self._lib.CMxBackendRpcRegister(
            self._ctx,
            self._user_rpc_cb
        )

        # Init the backend
        self._lib.CMxBackendInit.restype = ct.c_bool
        init_ok = self._lib.CMxBackendInit(self._ctx)

        # Init other usefull variables
        self._rdb_types_cache = {}

        if not init_ok:
            logger.error('Failed to initialize the backend.')
            return

    def close(self):
        self._lib.CMxBackendDestroy(self._ctx)
        self._lib.CMxContextDestroy(ct.byref(ct.c_void_p(self._ctx)))

    def __enter__(self):
        return self

    def __exit__(self, et, ev, etb):
        self.close()

    def _serialize_user_data(self, data):
        if isinstance(data, int):
            return struct.pack('<i', data)
        elif isinstance(data, float):
            return struct.pack('<d', data)
        elif isinstance(data, bytes):
            return struct.pack(f'<{len(data)}s', data)
        elif isinstance(data, str):
            return data.encode('utf-8')
        elif isinstance(data, bool):
            return struct.pack('<?', data)
        elif isinstance(data, ct.c_uint8):
            return struct.pack('<B', data.value)
        elif isinstance(data, ct.c_uint16):
            return struct.pack('<H', data.value)
        elif isinstance(data, ct.c_uint32):
            return struct.pack('<L', data.value)
        elif isinstance(data, ct.c_uint64):
            return struct.pack('<Q', data.value)
        elif isinstance(data, ct.c_int8):
            return struct.pack('<b', data.value)
        elif isinstance(data, ct.c_int16):
            return struct.pack('<h', data.value)
        elif isinstance(data, ct.c_int32):
            return struct.pack('<l', data.value)
        elif isinstance(data, ct.c_int64):
            return struct.pack('<q', data.value)
        elif isinstance(data, ct.c_float):
            return struct.pack('<f', data.value)
        elif isinstance(data, ct.c_double):
            return struct.pack('<d', data.value)
        elif isinstance(data, ct.c_bool):
            return struct.pack('<?', data.value)
        else:
            raise TypeError(f'Unsupported type: {type(data)}')

    def _deserialize_user_data(self, data, dtype):
        if dtype is int:
            return struct.unpack('<i', data[:4])[0], 4
        elif dtype is float:
            return struct.unpack('<d', data[:8])[0], 8
        elif dtype is bytes:
            return struct.unpack(f'<{len(data)}s', data)[0], len(data)
        elif dtype is str:
            return data.decode('utf-8'), len(data)
        elif dtype is bool:
            return struct.unpack('<?', data[:1])[0], 1
        elif dtype is ct.c_uint8:
            return struct.unpack('<B', data[:1])[0], 1
        elif dtype is ct.c_uint16:
            return struct.unpack('<H', data[:2])[0], 2
        elif dtype is ct.c_uint32:
            return struct.unpack('<L', data[:4])[0], 4
        elif dtype is ct.c_uint64:
            return struct.unpack('<Q', data[:8])[0], 8
        elif dtype is ct.c_int8:
            return struct.unpack('<b', data[:1])[0], 1
        elif dtype is ct.c_int16:
            return struct.unpack('<h', data[:2])[0], 2
        elif dtype is ct.c_int32:
            return struct.unpack('<l', data[:4])[0], 4
        elif dtype is ct.c_int64:
            return struct.unpack('<q', data[:8])[0], 8
        elif dtype is ct.c_float:
            return struct.unpack('<f', data[:4])[0], 4
        elif dtype is ct.c_double:
            return struct.unpack('<d', data[:8])[0], 8
        elif dtype is ct.c_bool:
            return struct.unpack('<?', data[:1])[0], 1
        else:
            raise TypeError(f'Unsupported type: {dtype}')

    def _serialize_generic(self, buffer: bytes):
        data = struct.pack('<Q', len(buffer)) + buffer
        self._shared_buff.value = data
        return ct.addressof(self._shared_buff)

    def _cb_register(self, type_ctor, func):
        return type_ctor(_ensure_gil(func))

    def _run_start_entry(self, no):
        self.run_start(no)

    def _run_stop_entry(self, no):
        self.run_stop(no)

    def _user_rpc_unpack(self, data) -> List[Any]:
        params = list(self.rpc.__annotations__.items())
        args = []
        offset = 0

        for pname, ptype in params:
            if pname == 'return':
                continue

            value, noff = self._deserialize_user_data(
                data[offset:],
                ptype
            )
            offset += noff
            args.append(value)
        return args

    def _user_rpc_entry(self, data, size):
        try:
            if not hasattr(self, 'rpc'):
                logger.error('Failed to call user rpc. Not defined.')
                logger.error('Please define a local rpc function.')
                return

            args = self._user_rpc_unpack(data[:size])
            return self._serialize_generic(
                self._serialize_user_data(
                    self.rpc(*args)
                )
            )
        except TypeError as e:
            logger.error('Failed to serialize data.')
            logger.error(f'Error: {e}.')

    def _deserialize_data(self, data: bytes, typeid: int) -> Any:
        match typeid:
            case 0: return struct.unpack('<b', data)[0]
            case 1: return struct.unpack('<h', data)[0]
            case 2: return struct.unpack('<l', data)[0]
            case 3: return struct.unpack('<q', data)[0]
            case 4: return struct.unpack('<B', data)[0]
            case 5: return struct.unpack('<H', data)[0]
            case 6: return struct.unpack('<L', data)[0]
            case 7: return struct.unpack('<Q', data)[0]
            case 8: return struct.unpack('<f', data)[0]
            case 9: return struct.unpack('<d', data)[0]
            case 10: return data.decode('utf-8')
            case 11: return struct.unpack('<?', data)[0]
        raise TypeError(f'Unsupported typeid: {typeid}')

    def _deserialize_key_value(self, key: str, buffer: bytes) -> Any:
        bkey = key.encode('utf-8')
        if key not in self._rdb_types_cache:
            ktype = self._lib.CMxRdbKeyType(self._ctx, bkey)
            self._rdb_types_cache[key] = ktype
        else:
            ktype = self._rdb_types_cache[key]

        return self._deserialize_data(buffer, ktype)

    # @abstractmethod
    def run_start(self, no: int) -> None:
        pass

    # @abstractmethod
    def run_stop(self, no: int) -> None:
        pass
    #
    # def rpc(self, data: bytes, size: int) -> RpcReturn:
    #     pass

    def register(self, name: str) -> None:
        self._lib.CMxBackendEventRegister(self._ctx, name.encode('utf-8'))

    def dispatch(self, name: str, data: bytes) -> None:
        self._lib.CMxBackendEventDispatch(
            self._ctx,
            name.encode('utf-8'),
            data,
            ct.c_uint64(len(data))
        )

    def subscribe(
        self,
        name: str,
        callback: Callable[[bytes], None]
    ) -> None:
        def cb_entry(data, len, udata):
            callback(
                bytes(data[:ct.c_uint64(len).value])
            )

        self._sub_events[name] = self._cb_register(
            ct.CFUNCTYPE(
                None,
                ct.POINTER(ct.c_uint8),
                ct.c_uint64,
                ct.POINTER(ct.c_uint8)
            ),
            cb_entry
        )

        self._lib.CMxBackendEventSubscribe(
            self._ctx,
            name.encode('utf-8'),
            self._sub_events[name]
        )

    def unsubscribe(self, name: str) -> None:
        self._lib.CMxBackendEventUnsubscribe(
            self._ctx,
            name.encode('utf-8')
        )

    def call(
        self,
        target: str,
        *args: RpcReturn,
        timeout: int = 1000,
        fmt: str = None
    ) -> Any:
        parts = []
        for arg in args:
            parts.append(self._serialize_user_data(arg))

        ibuff = b''.join(parts)
        size = len(ibuff)
        buffer = (ct.c_uint8 * size).from_buffer_copy(ibuff)

        ret = self._lib.CMxBackendRpcCall(
            self._ctx,
            target.encode('utf-8'),
            buffer,
            ct.c_uint64(size),
            ct.c_int64(timeout)
        )

        if ret.status == 0:
            if ret.size == 0:
                return None
            if not fmt and ret.size > 0:
                logger.warning(
                    'No return format specified. '
                    'Returning raw bytes.'
                )
                data = bytes(ret.data[:ret.size])
                self._lib.CMxBackendRpcFreeReturn(ret)
                return data
            else:
                data = struct.unpack(fmt, bytes(ret.data[:ret.size]))[0]
                self._lib.CMxBackendRpcFreeReturn(ret)
                return data
        else:
            logger.error('Failed to call user function.')
            logger.error(f'Error: {ret.status}')
            status = ret.status
            self._lib.CMxBackendRpcFreeReturn(ret)
            raise RpcCallFailed(
                f"Failed to call RPC from: {target} (error: {status})"
            )

    def read(self, key: str) -> Any:
        sz = ct.c_uint64(1024)
        buffer = (ct.c_uint8 * sz.value)()
        bkey = key.encode('utf-8')

        ok = self._lib.CMxRdbRead(
            self._ctx,
            bkey,
            buffer,
            ct.byref(sz)
        )

        if not ok:
            raise InvalidKey(f"Failed to read key: {key}")

        return self._deserialize_key_value(key, bytes(buffer[:sz.value]))

    def write(self, key: str, value: Any) -> bool:
        ibuff = self._serialize_user_data(value)
        buffer = (ct.c_uint8 * len(ibuff)).from_buffer_copy(ibuff)
        return self._lib.CMxRdbWrite(
            self._ctx,
            key.encode('utf-8'),
            buffer,
            ct.c_uint64(len(buffer))
        )

    def create(self, key: str, ktype: int) -> bool:
        return self._lib.CMxRdbCreate(
            self._ctx,
            key.encode('utf-8'),
            ct.c_uint8(ktype),
            None,
            0x0
        )

    def delete(self, key: str) -> bool:
        return self._lib.CMxRdbDelete(
            self._ctx,
            key.encode('utf-8')
        )

    def watch(self, key: str, callback: Callable[[str, Any], None]) -> None:
        def cb_entry(key, value, size):
            decoded_key = ct.string_at(key).decode('utf-8')
            callback(
                decoded_key,
                self._deserialize_key_value(
                    decoded_key,
                    bytes(value[:ct.c_uint64(size).value])
                )
            )

        self._rdb_watches[key] = self._cb_register(
            ct.CFUNCTYPE(
                None,
                ct.POINTER(ct.c_char),
                ct.POINTER(ct.c_uint8),
                ct.c_uint64
            ),
            cb_entry
        )

        self._lib.CMxRdbWatch(
            self._ctx,
            key.encode('utf-8'),
            self._rdb_watches[key]
        )

    def unwatch(self, key: str) -> None:
        self._lib.CMxRdbUnwatch(self._ctx, key.encode('utf-8'))
        # del self._rdb_watches[key]

    def log(self, ltype: LogType, msg: str) -> None:
        match ltype:
            case Backend.LogType.INFO:
                self._lib.CMxMsgEmitterLogInfo(
                    self._ctx,
                    msg.encode('utf-8')
                )
            case Backend.LogType.WARN:
                self._lib.CMxMsgEmitterLogWarning(
                    self._ctx,
                    msg.encode('utf-8')
                )
            case Backend.LogType.ERROR:
                self._lib.CMxMsgEmitterLogError(
                    self._ctx,
                    msg.encode('utf-8')
                )

    def set_status(self, status: str, color: str) -> None:
        self._lib.CMxBackendStatusSet(
            self._ctx,
            status.encode('utf-8'),
            color.encode('utf-8')
        )

    def write_file(self, filename: str, data: bytes) -> None:
        sz = len(data)
        buff = (ct.c_uint8 * sz).from_buffer_copy(data)

        self._lib.CMxBackendRunLogWriteFile(
            self._ctx,
            filename.encode('utf-8'),
            buff,
            ct.c_uint64(sz)
        )
