cmake_minimum_required(VERSION 3.10)
include(FetchContent)
include(cmake/InternetConnection.cmake)
include(cmake/BuildManifest.cmake)
include(cmake/BuildFrontend.cmake)
include(cmake/InstallMulex.cmake)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Version configuration
project(mulex-fk VERSION 0.2.0)
set(PROJECT_VNAME Barnard)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LTRACE "Enable/Disable trace logging." OFF)
option(PROFILE "Enable/Disable profiling with tracy." OFF)
option(USB_SUPPORT "Enable/Disable bulk USB transfer support with libusb." ON)

if(LTRACE)
	add_definitions(-DLTRACE)
endif()

if(USB_SUPPORT)
	add_definitions(-DUSB_SUPPORT)
endif()

# Check internet connection
check_network_connection()
if(NOT NETWORK_STATUS)
	set(FETCHCONTENT_FULLY_DISCONNECTED ON)
	message(WARNING "FetchContent offline mode. '_deps' need to be available and populated.")
else()
	set(FETCHCONTENT_FULLY_DISCONNECTED OFF)
endif()

# SQLite3
FetchContent_Declare(
	sqlite3
	URL https://www.sqlite.org/2024/sqlite-amalgamation-3470000.zip
	URL_HASH SHA3_256=e35ee48efc24fe58d0e9102034ac0a41e3904641a745e94ab11a841fe9d7355e
	EXCLUDE_FROM_ALL
)
if (NOT sqlite3_POPULATED)
	FetchContent_Populate(sqlite3)
endif()

# Tracy Profiler
FetchContent_Declare(
	tracy
	GIT_REPOSITORY https://github.com/wolfpld/tracy.git
	GIT_TAG v0.11.1
	GIT_SHALLOW TRUE
	EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(tracy)

# RPC related configuration
set(RPC_CALL_KEYWORD "MX_RPC_METHOD")
add_definitions(-D${RPC_CALL_KEYWORD}=)
set(RPC_SPEC_FILE rpcspec.inl)

if(PROFILE)
	add_definitions(-DTRACY_ENABLE)
endif()

# Make sure tracy profiler is installed/linked system wide
link_libraries(TracyClient)
include_directories(${tracy_SOURCE_DIR}/public)

add_executable(mxmain
	main.cpp
)

# Add mulex functionalities
add_subdirectory(network)
add_subdirectory(rdb)
add_subdirectory(drv)
add_subdirectory(run)
add_subdirectory(bck)
add_subdirectory(ctargets)
add_subdirectory(examples)

# Link main
target_link_libraries(mxmain PRIVATE mxrpc mxrdb mxdrv mxhttp)

if(WIN32)
	target_link_libraries(mxmain PRIVATE pdh)
endif()

# Invoke yarn build (pre build)
build_manifest_json()
build_frontend_yarn()

# Add testing
enable_testing()
add_subdirectory(test)

# Generate binary resources (last step)
set(vite_config_ts ${CMAKE_SOURCE_DIR}/network/vite.config.ts)
mx_resource_append(${vite_config_ts})

mx_resource_gen()

# Finally install everything for the user
install_mulex()
