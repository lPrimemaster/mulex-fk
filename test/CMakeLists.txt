add_executable(test_log logger.cpp ../mxlogger.h)
add_test(test_log test_log)

# Generate test RPC calls at configure time
execute_process(
	COMMAND python3 ../mxrpcgen.py
		--dirs ${CMAKE_CURRENT_SOURCE_DIR}
		--output-file rpcspec.inl
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	RESULT_VARIABLE mxrpcret
)
 
if(mxrpcret EQUAL "1")
	message(FATAL_ERROR "mxrpcgen failed")
endif()

add_executable(test_rpc rpc.cpp ../rpc/rpc.cpp ../rpc/socket.cpp ../mxsystem.cpp)

if(WIN32)
	target_link_libraries(test_rpc wsock32 ws2_32)
endif()

target_include_directories(test_rpc PRIVATE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)
add_test(test_rpc test_rpc)

add_executable(test_rdb rdb.cpp ../rdb/mxrdb.cpp ../rpc/rpc.cpp ../rpc/socket.cpp ../mxsystem.cpp)

if(WIN32)
	target_link_libraries(test_rdb wsock32 ws2_32)
endif()

target_include_directories(test_rdb PRIVATE
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)

add_test(test_rdb test_rdb)

add_executable(test_drv drv.cpp ../drv/mxdrv.cpp)
add_test(test_drv test_drv)
