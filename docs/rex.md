## Summary
The mxrexs (**r**emote **ex**ecution **s**ervice - Rex for short) is a small background service that quietly runs on the system providing backend remote start/stop capabilities.

## Running rex
You can run rex two ways: inplace and daemon. Running rex as a daemon is recommended (contrary to backends, this workds both on Linux and Windows).

To simply run rex on the current console type `mxrexs`. That's it, nothing else required, just leave the process up.
Otherwise it is recommended to run rex on the background via `mxrexs --start` (stop it using `mxrexs --stop`).

Once rex is running, any local backend that has been started in the past is known to rex and is able to be started/stopped via RPC call.
Since this is a RPC call one can start backends via:

- Browser
- Other backends on same system
- Other backends on other system

The recommended easy way to make rex always available is to put its executable on a system service (systemd on Linux, taskscheduler on Windows)
with auto-start on computer wake-up.

## Starting/Stopping Backends Remotely
```cpp
MX_RPC_METHOD mulex::RexCommandStatus RexSendStartCommand(std::uint64_t backend);
MX_RPC_METHOD mulex::RexCommandStatus RexSendStopCommand(std::uint64_t backend);
```

To start/stop a backend remotely use the aforementioned RPC calls.
