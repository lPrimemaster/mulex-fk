# Instalation

The only way to install mulex-fk is via it's source code. There are no official releases nor packages.
Mulex-fk is tested and is known to work under Linux and Windows.

## Requirements

To install mulex-fk you will need:

- cmake (>= 3.10)
- gcc/g++ (tested under v12.3.0)
- python3 (>= 3.10)
- node.js (tested under v20.12.2)
- yarn (any node package manager will do with a bit of changing on the `CMakeLists.txt`)
- zlib
- libuv
- libusb (optional - if you wish builtin USB support on the backends)

Additional libraries are installed under the cmake configuration step and an internet connection is required for the first configuration.

## Building
Under the repository main directory (you can look under the main `CMakeLists.txt` file for additional configure time options):
```sh
mkdir build && cmake -DCMAKE_BUILD_TYPE=Release ..
```

> :warning: If the configuration step fails at the `yarn build` go to `frontend/` and run `yarn` without arguments to download dependencies.

After this just call your prefered build tool and install (e.g. using make):
```sh
make -j8 && make install
```
> :warning: Installing might require sudo/administrative rights

If everything was configured properly you should now be able to type:
```sh
mxmain --help
```
This completes the installation of mulex-fk on the system.
