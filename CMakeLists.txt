cmake_minimum_required(VERSION 3.21)
include(FetchContent)
include(cmake/InternetConnection.cmake)
include(cmake/BuildManifest.cmake)
include(cmake/BuildFrontend.cmake)
include(cmake/InstallMulex.cmake)
include(cmake/Configure.cmake)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Options
option(LTRACE "Enable/Disable trace logging." OFF)
option(PROFILE "Enable/Disable profiling with tracy." OFF)
option(USB_SUPPORT "Enable/Disable bulk USB transfer support with libusb." OFF)
option(BUILD_TESTS "Build tests." OFF)
option(BUILD_EXAMPLES "Build examples." OFF)
option(GH_ACTION_MODE "Signals this configure is running in a github action." OFF)
option(CREATE_PACKAGE "Create a tar package for this install target. Package for kpm." OFF)

# Version configuration
project(mulex-fk VERSION 1.1.1)
set(PROJECT_VNAME "Lacaille")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# We need PIC to be able to compile mxcapi
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# If we are creating a package
# Make the install dir local to build
if(CREATE_PACKAGE)
	# Store original prefix
	if(NOT DEFINED ORIGINAL_INSTALL_PREFIX)
		set(ORIGINAL_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}" CACHE INTERNAL "Original install prefix")
	endif()

	# Overwrite prefix
	set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/package_deploy/" CACHE PATH "Install prefix" FORCE)
elseif(ORIGINAL_INSTALL_PREFIX)
	# Reset to original
    set(CMAKE_INSTALL_PREFIX "${ORIGINAL_INSTALL_PREFIX}" CACHE PATH "Install prefix" FORCE)
	unset(ORIGINAL_INSTALL_PREFIX CACHE)
endif()

# Disable fetch content network check for gh actions
if(NOT GH_ACTION_MODE)
	# Check internet connection
	check_network_connection()
	if(NOT NETWORK_STATUS)
		set(FETCHCONTENT_FULLY_DISCONNECTED ON)
		message(WARNING "FetchContent offline mode. '_deps' need to be available and populated.")
	else()
		set(FETCHCONTENT_FULLY_DISCONNECTED OFF)
	endif()
endif()

if(LTRACE)
	add_definitions(-DLTRACE)
endif()

if(USB_SUPPORT)
	add_definitions(-DUSB_SUPPORT)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_definitions(-DCREATE_DEV_USER)
endif()

# RPC related configuration
set(RPC_CALL_KEYWORD "MX_RPC_METHOD")
set(RPC_PERM_KEYWORD "MX_PERMISSION")
add_definitions(-D${RPC_CALL_KEYWORD}=)
set(RPC_SPEC_FILE rpcspec.inl)
set(USER_DB_SETUP user_database.sql)
set(PERM_SPEC_FILE dbperms.inl)

# Set global warnings all
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	add_definitions(-include ${CMAKE_SOURCE_DIR}/network/rpc_macros.h)
	add_compile_options(
		-Wall										# All warnings!
		-Wextra										# More warnings!
		-Wno-unused-parameter						# Unused parameters are common on some scenarios (implicit constructor deductions etc)
													# So I don't need to be using [[maybe_unused]] every single time I do that (but really should)
		-Wno-implicit-fallthrough					# I like implicit fallthroughs
		$<$<COMPILE_LANGUAGE:CXX>:-Wno-reorder>     # Usually I don't use this and when I do, stuff don't depend on one another (this could mean bugs tho...)
		-Wno-unused-function 						# RPC Generation might be usefull on some compilation units in the future
	)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	add_definitions(/FI"${CMAKE_SOURCE_DIR}/network/rpc_macros.h")
	add_compile_options(
		/W4                                 # Enable most warnings (closest to -Wall -Wextra)
		/wd4100                             # Disable “unreferenced formal parameter” (unused parameter)
		/wd4065                             # Disable “switch statement contains 'default' but no 'case' labels” (closest to -Wno-implicit-fallthrough)
		/wd4127                             # Disable “conditional expression is constant” (common in static_asserts/macros)
		/wd4505                             # Disable “unreferenced local function has been removed” (unused function)
		/wd26451                            # Disable code analysis: arithmetic overflow warning (optional, noisy)
		/wd26812                            # Disable “enum type is unscoped” (common with plain enums)
		/wd4996								# Ignore the pesky deprecation on MSVC CRT legacy functions
		/wd4200								# Ignore zero-sized array auto ctor / copy / move warning
		/wd4458								# Class member hiding
		/wd4702								# Unreachable code (this is usefull for hiding code on tracing)
		/wd4815								# Zero sized arrays
	)
endif()

# Fetch and build external dependencies
fetch_dependencies()
build_dependencies()

generate_rpc_calls()

add_library(mxapi
	bck/mxbackend.cpp
	drv/mxdrv.cpp
	lbk/mxlbk.cpp

	network/mxevt.cpp
	network/mxhttp.cpp
	network/mxmsg.cpp
	network/rpc.cpp
	network/socket.cpp

	plug/fxfer.cpp

	rdb/mxrdb.cpp
	rdb/mxpdb.cpp
	rdb/mxfdb.cpp

	rexs/mxrexs.cpp

	run/mxrun.cpp

	mxsystem.cpp

	${sqlite3_SOURCE_DIR}/sqlite3.c

	$<TARGET_OBJECTS:uSockets>
)

# We also need to setup the prefix for windows debug shipping
if(WIN32)
	set_target_properties(mxapi PROPERTIES DEBUG_POSTFIX "d")
endif()

# OS agnostic linking
target_link_libraries(mxapi PUBLIC OpenSSL::SSL ZLIB::ZLIB)
target_link_libraries(mxapi PRIVATE base64 TracyClient)

if(WIN32)
	# Win32 style linking
	find_package(libuv CONFIG REQUIRED)
	target_link_libraries(mxapi PUBLIC $<IF:$<TARGET_EXISTS:libuv::uv_a>,libuv::uv_a,libuv::uv> ZLIB::ZLIB)
	target_link_libraries(mxapi PUBLIC wsock32 ws2_32 Shlwapi pdh)
else()
	# Linux style linking
	target_link_libraries(mxapi PUBLIC uv ZLIB::ZLIB)
endif()

if(USB_SUPPORT)
	target_link_libraries(mxapi PUBLIC ${LIBUSB_LIBRARY})
	target_include_directories(mxapi PUBLIC ${LIBUSB_INCLUDE_DIR})
endif()

target_include_directories(mxapi PRIVATE
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>/uWebSockets/src
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>/uWebSockets/uSockets/src
	${rapidjson_SOURCE_DIR}/include
	${aklomp_base64_SOURCE_DIR}/include
	${sqlite3_SOURCE_DIR}
	${tracy_SOURCE_DIR}/public
	${CMAKE_BINARY_DIR}/uWebSockets/uSockets/src
)

# Make sqlite3 support FTS5
target_compile_definitions(mxapi PRIVATE SQLITE_ENABLE_FTS5)

# Add mulex functionalities
add_subdirectory(plug)
add_subdirectory(rexs)

# Configure C-API library
add_library(mxcapi SHARED
	capi/mxcapi.cpp
)

# We also need to setup the prefix for windows debug shipping
if(WIN32)
	set_target_properties(mxcapi PROPERTIES DEBUG_POSTFIX "d")
endif()

target_link_libraries(mxcapi PRIVATE mxapi)

# Hide symbols by default
# We don't want to export the whole tables of all
# of the mx symbols
set_target_properties(mxcapi PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# Enforce c++20
target_compile_features(mxcapi PUBLIC cxx_std_20)

add_executable(mxmain
	main.cpp
)

generate_mxconfig()

# Link main
target_link_libraries(mxmain PRIVATE mxapi)
target_compile_definitions(mxmain PUBLIC L2FILE)

# Clear resources from 'plug'
mx_resource_clear()

# Invoke yarn build (pre build)
build_manifest_json()
build_frontend_yarn()

# Add testing
if(BUILD_TESTS)
	enable_testing()
	add_subdirectory(test)
endif()

# Generate binary resources (last configure step)
mx_resource_append(${CMAKE_BINARY_DIR}/${USER_DB_SETUP})
mx_resource_gen(mxmain)

# Install everything for the user
install_target(mxapi NONE)
install_mulex()

# Package for kpm
if(CREATE_PACKAGE)
	package_mulex()
endif()
