# Getting Started

## Running the server
Mulex-fk manages all of the backends/frontends via the `mxmain` server service. You can start it with a new test experiment via
```sh
mxmain -n testexp -l
```
This opens the server with an experiment named testexp and in loopback mode so we don't serve the page on the network.


## Web frontend
If the previous step is successful you can go to your browser and type
```
localhost:8080
```
to land on the frontend home page.

### Home page
This page displays most of the info on currently active backends and connected frontends.
There is a 'log panel' where you will be able to see messages from the connected backends (more on this later),
a status pane where you see the current 'run' and status,
as well as controlling start/stop.
It also has a small widget to display system resources.

### Project page
Next is the project page. This will be empty and is where all of the user pages (plugins as called in the API) will be to control the experiment in question.


### RDB page
The RDB page allows the connected user to view all of the keys currently present in the system. You can use the top search bar to look for a specific entry.
This page also allows for the user to create a new key. Try clicking on a table key entry, the window view will be scrolled down to a pane where all of the current
variable info is. This pane updates in realtime. You can for example look under `/system/metrics/cpu_usage`.
This page also allows to edit/remove existing keys (as long as they're not system keys).

More on the RDB [here](rdb.md).

### History page
> :warning: This page is still a work in progress.

The history page allows you to look at one/multiple RDB entry/entries and log its/their value in a line plot.

## Creating Backends

When installing all of the `Mx<Lib>.a` files will be installed alongside with their header files. Cmake's `find_package` is able to retrieve include directories and library paths automatically. It becomes trivial to create a backend project with CMake when mulex is installed in the compiling system.

### Sample `CMakeLists.txt` for a backend project

```cmake
cmake_minimum_required(VERSION 3.10)
project(MyBackendProject VERSION 1.0.0)

add_executable(MyBackend
    main.cpp
)

find_package(MxBackend REQUIRED)
target_link_libraries(MyBackend PRIVATE Mx::MxBackend)
```

That simple!

### Your first backend

Under the `main.cpp` you then create a backend class extending MxBackend.

```cpp
using namespace mulex;

class MyBackend : public MxBackend
{
public:
	MyBackend(int argc, char* argv[]) : MxBackend(argc, argv)
	{
		log.info("Hello from MyBackend!");
        deferExec(&MyBackend::periodic, 0, 1000); // Run every 1000 ms, starting now
        registerRunStartStop(&MyBackend::onRunStart, &MyBackend::onRunStop);
	}

	void onRunStart(std::uint64_t runno)
	{
        log.info("MyBackend seen run start %llu.", runno);
	}

	void onRunStop(std::uint64_t runno)
	{
        log.info("MyBackend seen run stop %llu.", runno);
	}

	void periodic()
	{
        log.info("MyBackend looping periodic...");
	}
};
```
Afterwards just start its event loop on your main.

```cpp
int main(int argc, char* argv[])
{
	MyBackend backend(argc, argv);
	backend.init();
    backend.spin();
	return 0;
}
```

That's it! You just created your first mulex backend!
Try and run it via `./MyBackend --server localhost` if you have a running instance of the mulex server running.
Now check out what happens on the frontend webapp.

You can kill backend instances via the `SIGINT` signal on Linux (via console `Ctrl-C`) or on windows on terminal close (also available via `Ctrl-C`).
This will disconnect and close the `MyBackend` instance smoothly.

## Creating frontend Plugins

### Your first plugin

To create a plugin workspace there is the helper command `mxplug` installed alongside mulex. It allows you to create a new plugin workspace on a given directory.
To create a new plugin workspace go to an empty directory and type
```sh
mxplug --new
```
This command creates a simple plugin template for you to populate. To compile it use any node package tool. I like `yarn` so will be using it but you will be able
to replace it with your own in the future.
To compile just run (requires yarn)
```sh
mxplug --build <experiment_name>
```
under the plugin root directory.

This will build and install (copy) the compiled javascript plugin to the experiment cache folder. Typically

- `/home/<user>/.mxcache/<exp_name>/plugins/` on Linux
- `%LocalAppData%/mxcache/<exp_name>/plugins/` on Windows

Reload the frontend page. Navigate to the project page and look at the new plugin.

That's it! You just created your first mulex frontend plugin!

This completes the quick start tutorial. For more info check the rest of the documentation on [plugins](plugins.md) and [backends](backends.md).
