## Summary

The `mulex::MxBackend` is where the users can write their system drivers to control any
device that might be connected to the system. Backends can also be used to
gather data from other backends and analyze them.

Each backend runs its own event loop and the user can register timed functions.
For those who are familiar with javascript, something like `setInterval` and
`setTimeout`.

Tasks that can be performed from within a backend context include:

- Reading/Writing to the RDB
- Reading/Writing to the PDB
- Dispatching/Subscribing to events
- Declare a user RPC function that can be
called from other contexts (backends or plugins)
- Sending logs to the frontend
- Registering user functions that act as data
gathering (e.g. periodic, on trigger, ...)
- Remote launch other backends that are marked as dependencies

Other than that, a backend is a standalone C++ executable that can run
any type of logic the user pretends.

## Using Backends

A backend is a C++ object that derives from the `mulex::MxBackend` class.

Here is the minimum required boilerplate code to create a user backend.

```cpp
#include <mxbackend.h>

class MyBackend : public mulex::MxBackend
{
public:
    MyBackend(int argc, char* argv[]) : mulex::MxBackend(argc, argv)
    {
        // User init goes here all communication is valid here
        // e.i. rdb, pdb, logging, events
    }
};

int main(int argc, char* argv[])
{
    MyBackend bck(argc, argv);
    bck.init();
    bck.spin();
    return 0;
}
```

This backend would compile and connect to the `mxmain` server. However it
wouldn't do much.
We can run this backend via `./MyBackend --server <mxmain_ip>`.

### Backend RDB
To access the RDB in a backend the user can use the `rdb` protected member.

For more info see [here](rdb.md).

### Backend PDB
Accessing the PDB is similar via using the `pdb` protected member.

For more info see [here](pdb.md).

### Backend Logging
The mx system supports logging. It is done via the `log` protected member.
Logging not only displays the message on backend's host console, but also
on the frontend. Messages are also stored under the PDB so the user can
access any log at any time.

#### Logging example

```cpp
MyBackend::MyBackend(int argc, char* argv[]) : mulex::MxBackend(argc, argv)
{
    info.log("Sending a log to the mxsystem.");
    info.warn("Sending a warning to the mxsystem.");
    info.error("Sending an error to the mxsystem.");
}
```

### Registering user functions
It is not uncommon that in many scenarios the user would want to run a
periodic function that checks for any changes on the underlying system.
The mx system allows to register functions via the `MxBackend::deferExec`
function.

#### `template <std::derived_from<MxBackend> D> MxBackend::deferExec(void (D::*func)(), std::int64_t delay = 0, std::int64_t interval = 0)`

Arguments:

- `func` - A method from the derived user backend class to register.
Must have a declaration of type `void <method>(void)`.

- `delay` - When to first run this function (in milliseconds). Defaults to `0`: run as soon
as possible inside the backend event loop thread. Similar to
`setTimeout(func, 0)` in javascript.

- `interval` - What interval to run this function on (in milliseconds).
Defaults to `0`: run only once. `interval` does not account for the
function's self time.

Functions that share any data and only run via the `deferExec` call don't
require data locking (as long as the data is only used within this context
), as the functions all run on the same thread.

Here is an example registering/deffering functions:

```cpp

void global_func()
{
    // Cannot access any context here.
}

class MyBackend : public mulex::MxBackend
{
public:
    MyBackend(int argc, char* argv[]) : mulex::MxBackend(argc, argv)
    {
        // periodic_data will run every second on this backend
        deferExec(&MyBackend::periodic_data, 0, 1000);

        // oneshot will run at least 100ms after this call
        deferExec(&MyBackend::oneshot, 100);

        // non-method will run as soon as possible
        deferExec(&global_func);
    }

    void oneshot()
    {
        log.info("Running after 100 ms.");
    }

    void periodic_data()
    {
        log.info("Dummy periodic.");
    }
};
```

Registering non-method functions is also allowed. But you don't have
access to the context (i.e. cannot use `rdb`, `log`, ...).

However, `deferExec` is public. This is intended for when the backend is
of a polling or software trigger type that might be controlled by an
external API. E.g.:

```cpp

// Not thread-safe nor keeping the data lifetime
// Just here to illustrate
void* data = 0;
int data_len = 0;

void my_func(/* api specific args, e.g. */void* api_data, int api_data_len, void* userdata)
{
    MyBackend* bck = reinterpret_cast<MyBackend*>(userdata);

    // Something else happens ... API code ... etc ...

    // The API just triggered, we could for example
    // save the data somewhere and access it on the onExternalAPITrigger
    // method
    data = api_data;
    data_len = api_data_len;
    // deferExec is thread-safe
    bck->deferExec(&MyBackend::onExternalAPITrigger);
}

class MyBackend : mulex::MxBackend
{
public:
    MyBackend(int argc, char* argv[]) : mulex::MxBackend(argc, argv)
    {
        // ...
        registerEvent("externalAPI_data");
    }

    void onExternalAPITrigger()
    {
        log.info("ExternalAPIFuncRegisterCallback triggered.");
        // Access data somehow ... Do some work ...

        // Or, e.g., dispatch an event
        dispatchEvent("externalAPI_data", data, data_len);
    }
};

int main(int argc, char* argv[])
{
    MyBackend bck(argc, argv);
    bck.init();

    ExternalAPIFuncRegisterCallback(my_func, &bck /* as user data e.g. */);

    bck.spin();

    return 0;
}
```

## Registering the user RPC function

There can only be one user RPC function. However it can be changed at
runtime. This function is called whenever any context (backend or plugin)
calls the `BckCallUserRpc` for this given backend. Here is a very simple
example including frontend code that calls the user RPC:

### Backend
```cpp
MyBackend::MyBackend(int argc, char* argv[]) : mulex::MxBackend(argc, argv)
{
    registerUserRpc(&MyBackend::my_rpc);
}

// This signature is required
mulex::RPCGenericType MyBackend::my_rpc(const std::vector<std::uint8_t>& data)
{
    log.info("User RPC called with data len: %llu.", data.size());
    log.info("Got: %d.", reinterpret_cast<std::int32_t*>(data.data()));
    return 3.1415f;
    // return {}; if no return is required
}
```

> :warning: Each backend can only have **one** user RPC function.

> :warning: You can, however, implement custom logic to branch to multiple other functions.

### Frontend
:warning: The frontend cannot register user RPC functions.

## Calling the user RPC function

### Backend
```cpp
void MyBackend::any_function()
{
    // Calling above 'my_rpc' function as example
    auto [status, ret] = callUserRpc<double>("backend_name", CallTimeout(1000), std::int32_t(42));
    // status is BckUserRpcStatus::OK if call succeeded
    if(status == BckUserRpcStatus::OK)
    {
        log.info("Got %lf from 'user_rpc' at '<backend_name>'.", ret);
    }

    // If function was void omit template arguments
    string32 other_arg = "hello";
    auto status = callUserRpc("other_backend", CallTimeout(1000), other_arg);
}
```

The example is pretty self explanatory. Take in mind that the arguments passed via template pack
need to be trivially copyable and default constructible (alternatively wrap them on a `RPCGenericType`).
`CallTimeout` specifies the time to wait (in milliseconds) for a response from the other backend. Assign this according
to your needs.

### Frontend
```ts
const rpc = MxRpc.Create();
rpc.then(async (handle) => {
    const res: MxGenericType = await handle.BckCallUserRpc([
        MxGenericType.str32('my_backend'),  // Executable name (as listed in home page)
        MxGenericType.int32(42, 'generic'), // Custom data
        MxGenericType.int64(10000)          // Timeout in ms
    ]);

    const [status, retval] = res.unpack(['uint8', 'float32'])[0];
    // status - status of the call
    // retval - 3.1415
});
```

The call status is based on the following enum:
```cpp
enum class BckUserRpcStatus : std::uint8_t
{
    OK,
    EMIT_FAILED,
    RESPONSE_TIMEOUT,
    NO_SUCH_BACKEND
};
```

If no return type is needed it is recommended to return an empty `MxGenericType` (i.e. `return {};`).
You can also bundle multiple data with the help of the `MxGenericType.makeData` and `MxGenericType.concatData` functions.

```ts
const rpc = MxRpc.Create();
rpc.then((handle) => {

    const data = MxGenericType.concatData([
        MxGenericType.int8(42),
        MxGenericType.f32(3.141592654)
    ]);

    const res: MxGenericType = await handle.BckCallUserRpc([
        MxGenericType.str32('my_backend'),       // Executable name (as listed in home page)
        MxGenericType.makeData(data, 'generic'), // Custom data
        MxGenericType.int64(10000)               // Timeout in ms
    ]);

    //...
});
```
Of course proper data handling must take place at the backend user function.

The plugin containing this code will call the backend user rpc function
as long as it is connected to the mx system.

## Setting dependencies
It will often happen that some backends require other backends to properly function.
This is where the `registerDependency` function comes in. It allows you to specify
that this backend relies on some other to properly function. This also can come in
handy if one would want to write a startup script, it could be done via a "composer"
backend that orchestrates the startup of the system's critical elements.

```cpp
MyBackend::MyBackend(int argc, char* argv[]) //...
{
    // This would terminate this backend if <other_backend_name> failed to start/was not running
    // The `required()` function if true, tries to remotely start the backend if found
    registerDependency("other_backend_name").required(true).onFail(RexDependencyManager::TERMINATE);

    // Also works via cid
    // Only warn if the depency is not found/running
    // Do not try to start it up (`required(false)`)
    registerDependency(0xc6a4a7935bd1e995).required(false).onFail(RexDependencyManager::LOG_WARN);
}
```

> :warning: Dependency checking occurs at the time of expression evaluation but 
the backend still runs its i/o loop for at least once before terminate is called.
Do not rely on this feature to terminate the backend consistently.

## Setting custom arguments
Let's say you want to add some custom argument as to where some config
file is and don't want to use the RDB/PDB for that. The backend
executable accepts custom arguments. They can be added via the
`SysAddArgument` function. It must be called before the constructor of
`MxBackend`. Here's an example:

```cpp
int main(int argc, char* argv[])
{
    SysAddArgument("my-arg-kebab-case", 0, true,
                   [](const std::string& value){ mulex::LogDebug("Got arg value: %s", value.c_str()); },
                   "My argument help text.");
    MyBackend bck(argc, argv);
    // ...
    return 0;
}
```

## Setting custom status
The `MxBackend` interface allows you to set a custom status that
will appear on the project's frontend page. This is usefull to
quickly the state of a backend to the user. Here's how to do it:

```cpp
void MyBackend::some_method()
{
    // ...
    // Status name followed by a custom RGB triplet
    setStatus("MyCustomStatus", "#ff0000");
}
```
The custom status is reset upon backend disconnect / exit.
