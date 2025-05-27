include(ExternalProject)
include(../cmake/InstallMulex.cmake)

configure_file(mxrpcgen.py ../mxrpcgen.py)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

# Generate RPC calls at configure time
execute_process(
	COMMAND ${Python3_EXECUTABLE} mxrpcgen.py 
		--dirs ${CMAKE_SOURCE_DIR}
		--ignore ${CMAKE_BINARY_DIR} test .cache .git build build_win # Specify build and build_win commonly used to dev
		--recursive
		--output-file ${RPC_SPEC_FILE}
		--sql-output ${USER_DB_SETUP}
		--permissions-input ${CMAKE_CURRENT_SOURCE_DIR}/roles.json
		--permissions-check-output ${PERM_SPEC_FILE}
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
	RESULT_VARIABLE mxrpcret
)

if(mxrpcret EQUAL "1")
	message(FATAL_ERROR "mxrpcgen failed")
endif()

find_package(OpenSSL REQUIRED)

# Don't use uSockets built-in makefile (does not work on windows - unless we are on MSYS2)
FetchContent_Declare(uWebSockets_cont
	GIT_REPOSITORY https://github.com/uNetworking/uWebSockets.git
	SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/uWebSockets
	GIT_SHALLOW ON
	EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(uWebSockets_cont)
message(STATUS ${CMAKE_CURRENT_BINARY_DIR}/uWebSockets)
file(GLOB_RECURSE uWS_SOURCES
	${CMAKE_CURRENT_BINARY_DIR}/uWebSockets/uSockets/src/*.c
	${CMAKE_CURRENT_BINARY_DIR}/uWebSockets/uSockets/src/*.cpp
)
add_library(uSockets ${uWS_SOURCES})
install_target(uSockets NONE)

if(WIN32)
	find_package(libuv CONFIG REQUIRED)
	target_link_libraries(uSockets PRIVATE $<IF:$<TARGET_EXISTS:libuv::uv_a>,libuv::uv_a,libuv::uv>)
endif()

target_include_directories(uSockets PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/uWebSockets/uSockets/src)
target_compile_options(uSockets PUBLIC -w) # We don't control uSockets
target_compile_definitions(uSockets PRIVATE LIBUS_NO_SSL)

FetchContent_Declare(aklomp_base64
	GIT_REPOSITORY https://github.com/aklomp/base64.git
	# EXCLUDE_FROM_ALL
)

# Disable base64 unneeded settings
set(BASE64_BUILD_CLI OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(aklomp_base64)
# install_target(base64 NONE)

FetchContent_Declare(rapidjson
	GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
	EXCLUDE_FROM_ALL
)

# WARN: (Cesar) Deprecated, but I don't want to build, and did not find a workaround using MakeAvailable
if(NOT rapidjson_POPULATED)
	FetchContent_Populate(rapidjson)
endif()

# Prevent RapidJSON from installing on the system when running [all] install target
# add_library(RapidJSON_Interface INTERFACE IMPORTED)
# set_target_properties(RapidJSON_Interface PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${rapidjson_SOURCE_DIR}/include")

add_library(MxRpc
	rpc.cpp
	socket.cpp
	mxevt.cpp
	mxmsg.cpp
	../rexs/mxrexs.cpp
	$<TARGET_OBJECTS:mxsys>
)

if(WIN32)
	target_link_libraries(MxRpc wsock32 ws2_32 Shlwapi)
endif()

target_include_directories(MxRpc PRIVATE
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)

add_library(mxrpc ALIAS MxRpc)
install_target(MxRpc network/rpc.h network)

add_library(MxHttp
	mxhttp.cpp
)

find_package(ZLIB REQUIRED)

if(WIN32)
	target_link_libraries(MxHttp PRIVATE $<IF:$<TARGET_EXISTS:libuv::uv_a>,libuv::uv_a,libuv::uv>)
	target_link_libraries(MxHttp PRIVATE uSockets ZLIB::ZLIB base64 mxrun)
else()
	target_link_libraries(MxHttp PRIVATE uv uSockets ZLIB::ZLIB base64 mxrun)
endif()

target_link_libraries(MxHttp PRIVATE OpenSSL::SSL)

target_include_directories(MxHttp PRIVATE
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>/uWebSockets/src
	$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>/uWebSockets/uSockets/src
	${rapidjson_SOURCE_DIR}/include
	${aklomp_base64_SOURCE_DIR}/include
)

add_library(mxhttp ALIAS MxHttp)
install_target(MxHttp mxhttp.h)
