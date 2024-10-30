configure_file(mxrpcgen.py ../mxrpcgen.py)

# Generate RPC calls at configure time
execute_process(
	COMMAND python3 mxrpcgen.py 
		--dirs ${CMAKE_SOURCE_DIR}
		--ignore ${CMAKE_BINARY_DIR} test .cache .git
		--recursive
		--output-file ${RPC_SPEC_FILE}
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
	RESULT_VARIABLE mxrpcret
)

if(mxrpcret EQUAL "1")
	message(FATAL_ERROR "mxrpcgen failed")
endif()

add_library(mxrpc
	rpc.cpp
	socket.cpp
	../mxsystem.cpp
)

if(WIN32)
	target_link_libraries(mxrpc wsock32 ws2_32)
endif()

target_include_directories(mxrpc PRIVATE
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)
