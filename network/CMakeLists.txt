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

ExternalProject_Add(uWebSockets
	GIT_REPOSITORY https://github.com/uNetworking/uWebSockets.git
	GIT_SHALLOW 1
	UPDATE_COMMAND ""
	SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/uWebSockets
	BUILD_IN_SOURCE 1
	CONFIGURE_COMMAND ""
	BUILD_COMMAND make -C uSockets -j8 # We only support make tools here (repo restriction) (windows or linux)
	BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/uWebSockets/uSockets/uSockets.a
	INSTALL_COMMAND ""
)

add_library(libuSockets STATIC IMPORTED GLOBAL)
set_property(TARGET libuSockets PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/uWebSockets/uSockets/uSockets.a)

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

add_dependencies(mxhttp uWebSockets)
target_link_libraries(mxhttp uv libuSockets ZLIB::ZLIB)

target_include_directories(mxhttp PRIVATE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>/uWebSockets/src
	$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>/uWebSockets/uSockets/src
)
