## Summary

Events are the primary method of transfering large ammounts of data between the backends (and plugins if necessary, but usually just for
inspection/online displaying data). They are simpler than the [RDB](rdb.md) as in they don't have storage. Once an event is triggered, it will
be sent to all of the backends/plugins that are listening and once the transfer succeeds, all of the data is voided if not saved by the user.

This means the data flow is purely one directional and there is no need of a lock/unlock mechanism as there are no data races, the event data
is simply copied and sent for all the listening backends/plugins. Events follow a pattern similar to the pub/sub scheme that so many realtime
communication frameworks use nowadays.

Events have no intrinsic metadata and therefore are treated as binary buffers internally by the mx system. All of the metadata/packing/unpacking
should be taken care of by the user under their backends/plugins. For that there are a few constructs that allow for simple manipulation of events.

## Using Events

### From C++
Issueing events is only allowed from the main backend instantiation of the `mulex::MxBackend` class.

#### `void MxBackend::registerEvent(const std::string& evt)`
Register a new event under the mx system that can be subscribed from any backend/plugin. `evt` name must be unique.
```cpp
// Under a backend constructor (for example)
registerEvent("MyBackend::MyEvent"); // Any valid unique string with size less than 32 bytes works
```

#### `void MxBackend::dispatchEvent(const std::string& evt, const std::uint8_t* data, std::uint64_t size)`
#### `void MxBackend::dispatchEvent(const std::string& evt, const std::vector<std::uint8_t>& data)`
Dispatch event named `evt` (must be a valid registered event) to the mx system with the given `data`.
```cpp
// Under a backend context (e.g. periodic)
void MyBackend::periodic()
{
    static std::vector<std::uint8_t> buffer(100);
    buffer.clear();

    // Fill buffer directly and send
    // Fill buffer ...
    dispatchEvent("MyBackend::MyEvent", buffer);
    // Or use the MxEventBuilder helper class
    dispatchEvent("MyBackend::MyEvent", mulex::MxEventBuilder(buffer)
        .add(mulex::SysGetCurrentTime())
        .add(3.14159265358979)
    );
    std::this_thread::sleep_for(100ms);
}
```

#### `void MxBackend::subscribeEvent(const std::string& evt, EvtClientThread::EvtCallbackFunc func)`
Subscribe to a given event present on the mx system. `evt` must be a valid event name. The function `func` gets triggered
everytime the event `evt` is emitted. Subscribing to events emitted on the same backend is allowed (although not very useful).
The function callback contains the following parameters:

- `const std::uint8_t* data` - The event data
- `std::uint64_t len` - The event data length
- `const std::uint8_t* udata` - Custom user data pointer

```cpp
// For example subscribe on run start
virtual void MyBackend::onRunStart(std::uint64_t runno) override
{
    subscribeEvent("MyBackend::MyEvent", [/*No lambda capture allowed.*/](auto* data, auto len, auto* udata) {
        mulex::LogTrace("Got event with len: %llu", len);
        // Destructure the data synchronously here or copy if processing the event is costly
    });
}
```

#### `void Mxbackend::unsubscribeEvent(const std::string& evt)`
Unsubscribe from the `evt` event.
```cpp
virtual void MyBackend::onRunStop(std::uint64_t runno) override
{
    unsubscribeEvent("MyBackend::MyEvent"); // Unsubscribe from this event on run stop
}
```

### From Typescript
The `MxEvent` object provides access to events for the typescript interface.
You cannot emit nor register events from the frontend plugins.

#### `public MxEvent.constructor(name: string) : MxEvent`
Create an event handler to valid/registered event `name`.
```ts
const my_event_my_bck = new MxEvent('MyBackend::MyEvent');
```

#### `public set MxEvent.onMessage(func: MxEventCallback)`
Set the callback for the given event. This is functionally equivalent to
subscribing to the event using the C++ API.
The function callback contains the following parameters:

- `data: Uint8Array` - The event data

```ts
my_event_my_bck.onMessage = (data: Uint8Array) : void => {
    console.log('Received event data with len ', data.length);
};
```

#### `public get MxEvent.name()`
Get the current event registered name.

#### Note on unsubscribing
Unsubscribing on the frontend is automatically managed once the `MxEvent` object goes out of scope
and gets handled by javascript's garbage collector.
