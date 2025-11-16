# Common files and functions to tidy main CMakeLists.txt

macro(generate_rpc_calls)
	configure_file(${CMAKE_SOURCE_DIR}/network/mxrpcgen.py mxrpcgen.py)

	find_package(Python3 COMPONENTS Interpreter REQUIRED)

	# Generate RPC calls at configure time
	execute_process(
		COMMAND ${Python3_EXECUTABLE} mxrpcgen.py 
			--dirs ${CMAKE_SOURCE_DIR}
			--ignore ${CMAKE_BINARY_DIR} test .cache .git build build_win vcpkg_installed # Specify build and build_win commonly used to dev
			--recursive
			--output-file ${RPC_SPEC_FILE}
			--sql-output ${USER_DB_SETUP}
			--permissions-input ${CMAKE_SOURCE_DIR}/network/roles.json
			--permissions-check-output ${PERM_SPEC_FILE}
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
		RESULT_VARIABLE mxrpcret
	)

	if(mxrpcret EQUAL "1")
		message(FATAL_ERROR "mxrpcgen failed")
	endif()
endmacro()

macro(build_dependencies)
	message(STATUS ${CMAKE_CURRENT_BINARY_DIR}/uWebSockets)
	file(GLOB_RECURSE uWS_SOURCES
		${CMAKE_CURRENT_BINARY_DIR}/uWebSockets/uSockets/src/*.c
		${CMAKE_CURRENT_BINARY_DIR}/uWebSockets/uSockets/src/*.cpp
	)
	add_library(uSockets OBJECT ${uWS_SOURCES})
	# install_target(uSockets NONE)

	if(WIN32)
		find_package(libuv CONFIG REQUIRED)
		target_link_libraries(uSockets PUBLIC $<IF:$<TARGET_EXISTS:libuv::uv_a>,libuv::uv_a,libuv::uv>)
	endif()

	target_include_directories(uSockets PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/uWebSockets/uSockets/src)
	target_compile_options(uSockets PUBLIC -w) # We don't control uSockets
	target_compile_definitions(uSockets PRIVATE LIBUS_NO_SSL)
endmacro()

macro(fetch_dependencies)
	find_package(ZLIB REQUIRED)
	find_package(OpenSSL REQUIRED)

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

	if(PROFILE)
		set(TRACY_ENABLE ON CACHE BOOL "Enable Tracy" FORCE)
	else()
		set(TRACY_ENABLE OFF CACHE BOOL "Enable Tracy" FORCE)
	endif()

	# Tracy Profiler
	# NOTE: (Cesar) Always declare due to <tracy/TracyClient.hpp> dependencies
	FetchContent_Declare(
		tracy
		GIT_REPOSITORY https://github.com/wolfpld/tracy.git
		GIT_TAG v0.13.0
		GIT_SHALLOW TRUE
		# EXCLUDE_FROM_ALL
	)
	FetchContent_MakeAvailable(tracy)
	# install_target(TracyClient NONE)

	# Don't use uSockets built-in makefile (does not work on windows - unless we are on MSYS2)
	FetchContent_Declare(uWebSockets_cont
		GIT_REPOSITORY https://github.com/uNetworking/uWebSockets.git
		SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/uWebSockets
		GIT_SHALLOW ON
		EXCLUDE_FROM_ALL
	)
	FetchContent_MakeAvailable(uWebSockets_cont)

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
endmacro()

macro(generate_mxconfig)
	# Generate the <mxconfig.h> file
	set(CONFIG_HEADER_FILE "${CMAKE_BINARY_DIR}/mxconfig.h")
	set_property(SOURCE ${CONFIG_HEADER_FILE} PROPERTY GENERATED TRUE)
	add_custom_target(generate_build_info ALL
		COMMAND ${CMAKE_COMMAND}
			-DCONFIG_HEADER_FILE=${CONFIG_HEADER_FILE}
			-DPROJECT_VNAME=${PROJECT_VNAME}
			-DPROJECT_VERSION=${CMAKE_PROJECT_VERSION}
			-P ${CMAKE_SOURCE_DIR}/cmake/GenerateConfigInfo.cmake
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		BYPRODUCTS ${CONFIG_HEADER_FILE}
		COMMENT "Checking mxconfig.h"
		VERBATIM
	)
	add_dependencies(mxmain generate_build_info)
endmacro()
