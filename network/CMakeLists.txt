include(ExternalProject)

configure_file(mxrpcgen.py ../mxrpcgen.py)

# Generate RPC calls at configure time
execute_process(
	COMMAND python3 mxrpcgen.py 
		--dirs ${CMAKE_SOURCE_DIR}
		--ignore ${CMAKE_BINARY_DIR} test .cache .git build build_win # Specify build and build_win commonly used to dev
		--recursive
		--output-file ${RPC_SPEC_FILE}
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
	RESULT_VARIABLE mxrpcret
)

if(mxrpcret EQUAL "1")
	message(FATAL_ERROR "mxrpcgen failed")
endif()

# ExternalProject_Add(uWebSockets
# 	GIT_REPOSITORY https://github.com/uNetworking/uWebSockets.git
# 	GIT_SHALLOW 1
# 	UPDATE_COMMAND ""
# 	SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/uWebSockets
# 	BUILD_IN_SOURCE 1
# 	CONFIGURE_COMMAND ""
# 	BUILD_COMMAND make -C uSockets -j8 # We only support make tools here (repo restriction) (windows or linux)
# 	BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/uWebSockets/uSockets/uSockets.a
# 	INSTALL_COMMAND ""
# )
#
# add_library(libuSockets STATIC IMPORTED GLOBAL)
# set_property(TARGET libuSockets PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/uWebSockets/uSockets/uSockets.a)

# Don't use uSockets built-in makefile (does not work on windows - unless we are on MSYS2)
FetchContent_Declare(uWebSockets_cont
	GIT_REPOSITORY https://github.com/uNetworking/uWebSockets.git
	SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/uWebSockets
	GIT_SHALLOW ON
)
FetchContent_MakeAvailable(uWebSockets_cont)
message(STATUS ${CMAKE_CURRENT_BINARY_DIR}/uWebSockets)
file(GLOB_RECURSE uWS_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/uWebSockets/uSockets/src/*.c)
add_library(uSockets ${uWS_SOURCES})
target_include_directories(uSockets PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/uWebSockets/uSockets/src)

FetchContent_Declare(aklomp_base64
	GIT_REPOSITORY https://github.com/aklomp/base64.git
)

# Disable base64 unneeded settings
set(BASE64_BUILD_CLI OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(aklomp_base64)

FetchContent_Declare(rapidjson
	GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
)

# Disable RapidJSON testing (which collides with some of our warnings as errors)
set(RAPIDJSON_BUILD_DOC OFF CACHE INTERNAL "")
set(RAPIDJSON_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(RAPIDJSON_BUILD_TESTS OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(rapidjson)

add_library(mxrpc
	rpc.cpp
	socket.cpp
	mxevt.cpp
	mxmsg.cpp
	../mxsystem.cpp
)

if(WIN32)
	target_link_libraries(mxrpc wsock32 ws2_32 Shlwapi)
endif()

target_include_directories(mxrpc PRIVATE
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)

add_library(mxhttp
	mxhttp.cpp
)

find_package(ZLIB REQUIRED)

# add_dependencies(mxhttp uWebSockets)
target_link_libraries(mxhttp uv uSockets ZLIB::ZLIB RapidJSON base64)

target_include_directories(mxhttp PRIVATE
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>/uWebSockets/src
	$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>/uWebSockets/uSockets/src
	${rapidjson_SOURCE_DIR}/include
	${aklomp_base64_SOURCE_DIR}/include
)
