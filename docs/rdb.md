## Summary

The **R**ealtime **D**ata**B**ase (**RDB**) works much like a disk database in the sense it stores data from multiple backends/frontends on a contiguous
chunk of data. The only difference being, the RDB lives on system memory directly, making it orders of magnitude faster to store/retreive values.

Inside the mx system the RDB is, however, still considered to be a way of storing "slow" control/data variables.

Much like an SQL database, the RDB allows unrestricted access to all of its variables from all backends/plugins. The only exception being plugins cannot
modify/delete `/system/*` variables. The RDB is based on a key/values access policy, i.e. one key corresponds to one value. Although the keys are presented
on a directory fashion (much like a tree) this hirerarchy is only present for eye candy as the real memory layout is flat and there is no real dependency
between key "directories". Here is an example of how a tiny RDB might look like:

```bash
├── system
│   ├── syskey1     # /system/syskey1
│   └── sysdir1     # In reality does not exist
│       ├── syskey1 # /system/sysdir1/syskey1
│       └── syskey2 # /system/sysdir1/syskey2
└── user
    ├── mykey       # /user/mykey
    └── mydir       # In reality does not exist
        └── mykey2  # /user/mykey2
```

Here I used the `/user/` directory to contain an experiment user keys but in reality you can write (although discouraged) keys directly on root dir
or any other named directory (user preference really).

Keys are available to all of the backends/plugins as soon as possible and concurrency is allowed and automatically managed (again, much like an SQL database).

Although the RDB has a much higher throughput than the typical disk database, it is recommended to use [events](events.md) as the primary method to transfer large/fast
chunks of data between fronteds/plugins ("fast" control).

## RDB view
The main webapp page contains a page for inspecting the RDB values in realtime. There you can quickly inspect/create/delete/modify keys and values.

## Using the RDB

### From C++

The backend class that extends MxBackend as access to an rdb proxy object, the `mulex::RdbAccess` class. Named `rdb` as a protected member. It works much like
a `std::map` in terms of accessing.

#### `inline RdbProxyValue RdbAccess::operator[](const std::string& key)`
The returned `RdbProxyValue` allows you to manipulate values.
Allows accessing rdb keys remotely. Examples:
```cpp
// Reading
int key = rdb["/user/mykey"]; // If mykey is an integer
float fkey = rdb["/user/myfloatkey"]; // If mykey is a float

// You can also explitly cast
auto key = static_cast<int>(rdb["/user/mykey"]);

// Writing
rdb["/user/mykey"] = 3; // Implicit cast int '/user/mykey' must be int32_t type
rdb["/user/mykey"] = static_cast<float>(3.14); // Cast float '/user/mykey' must be float type
```

#### `bool RdbProxyValue::exists()`
Checks if the given key exists on the RDB:
```cpp
bool has_key = rdb["/user/mykey"].exists();
```

#### `bool RdbProxyValue::create(RdbValueType type, RPCGenericType value, std::uint64_t count = 0)`
Creates a new key under the provided location with type `type` and value `value`. `count` specifies if the key is an array or not (count zero).
This last argument is reserved for future use and must be zero.
```cpp
if(!rdb["/user/intkey"].create(RdbValueType::INT32, std::int32_t(42)))
{
    // Failed to create key
}
```

#### `bool RdbProxyValue::erase()`
Erases the given key:
```cpp
if(!rdb["/user/mykey"].erase())
{
    // Failed to erase key, maybe it does not exist?
}
```

#### `void RdbProxyValue::watch(std::function<void(const RdbKeyName& key, const RPCGenericType& value)> callback)`
Watch the given key/pattern for changes and trigger `callback` as a result:
```cpp
rdb["/user/mykey"].watch([&](const RdbKeyName& key, const RPCGenericType& value) {
    // This callback gets triggered everytime '/user/mykey' changes
    log.info("Key %s changed and now contains value %d.", key.c_str(), value.asType<std::int32_t>());
});
```
This specific method allows for a `RdbProxyValue` with a pattern for a key. For example:
```cpp
rdb["/user/*"].watch([&](const RdbKeyName& key, const RPCGenericType& value) {
    // This callback gets triggered everytime any key under the '/user' directory changes
    log.info("Key %s changed and now contains value %d.", key.c_str(), value.asType<std::int32_t>());
});
```
**Valid** patterns example:

- `/*`
- `/*/*`
- `/dir/*/dir2/key`
- `/dir/*/dir2/*`

**Invalid** patterns example:

- `*`
- `/*sub/*`
- `/dir/*/dir2/key*suffix`

#### `void RdbProxyValue::unwatch()`
Unwatch key on a given `RdbAccess` instance:
```cpp
rdb["/user/mykey"].unwatch(); // Stop watching this key
```

#### `void RdbProxyValue::history(bool status)`
Enable/disable variable history logging. This feature, when enabled, logs all the changes the given variable
went into the [persistent database (PDB)](pdb.md).
*This feature is still a work in progress.*
```cpp
rdb["/user/mykey"].history(true); // Log this variable for the foreseeable future
```

### From Typescript
Accessing the rdb from plugins via Typescript is similar to C++. It is performed using the `MxRdb` class.

#### `public async MxRdb.create(key: string, type: string, value: any, count: number = 0) : Promise<void>`
Create a key.
```ts
const rdb = new MxRdb();
rdb.create('/user/mykey', 'int32', 42);
```

#### `public MxRdb.delete(key: string) : void`
Delete a key.
```ts
const rdb = new MxRdb();
rdb.delete('/user/mykey');
```

#### `public async MxRdb.read(key: string) : Promise<any>`
Read a key. Unlike its C++ counterpart, this function does not require the variable type. It is automatically inferred.
```ts
const rdb = new MxRdb();
const value : number = rdb.read('/user/mykey'); // If we know it is a number (for example int32)
```

#### `public async MxRdb.write(key: string, value: any) : Promise<void>`
Write a key. Unlike its C++ counterpart, this function does not require the variable type. It is automatically inferred.
```ts
const rdb = new MxRdb();
rdb.write('/user/mykey', 42); // If we know it is a number (for example int32)
```
#### `public MxRdb.watch(key: string, callback: Function) : void`
Watch a key/pattern for changes (Patterns are the same as for the C++ API).
```ts
const rdb = new MxRdb();
rdb.watch('/user/mykey', (key: string, value: MxGenericType) => {
    console.log('Key ', key, ' changed and now contains value ', value.astype('int32'));
});
```
#### `public MxRdb.unwatch(key: string) : void`
Unwatch a key on the current `MxRdb` instance.
```ts
const rdb = new MxRdb();
//...
rdb.unwatch('/user/mykey');
```
