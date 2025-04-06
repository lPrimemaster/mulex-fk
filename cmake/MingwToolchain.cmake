# Toolchain file to let cmake know we will be cross compiling for Windows x64

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Use MinGW64 (with posix threads)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc-posix)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++-posix)

# Use vcpkg to get libraries
if(VCPKG_ROOT STREQUAL "")
	message(FATAL_ERROR "Cross compilation for Windows requires VCPKG_ROOT to be set.")
endif()
set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32 /usr/lib/gcc/x86_64-w64-mingw32 "${VCPKG_ROOT}/installed/x64-mingw-dynamic")
set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic" CACHE STRING "Vcpkg target triplet.")

# Only vcpkg
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
