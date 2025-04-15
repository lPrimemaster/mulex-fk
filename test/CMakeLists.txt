add_executable(test_log logger.cpp ../mxlogger.h)
add_test(test_log test_log)

# Generate test RPC calls at configure time
# This generates some RPC call stubs for testing
execute_process(
	COMMAND python3 ../mxrpcgen.py
		--dirs ${CMAKE_SOURCE_DIR}
		--ignore ${CMAKE_BINARY_DIR}
		--output-file rpcspec.inl
		--recursive
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	RESULT_VARIABLE mxrpcret
)
 
if(mxrpcret EQUAL "1")
	message(FATAL_ERROR "mxrpcgen failed")
endif()

add_executable(test_rpc rpc.cpp ../mxsystem.cpp)

if(WIN32)
	target_link_libraries(test_rpc wsock32 ws2_32 Shlwapi)
endif()

target_link_libraries(test_rpc mxrdb mxhttp mxrpc mxbck)

target_include_directories(test_rpc PRIVATE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)
add_test(test_rpc test_rpc)

add_executable(test_rdb rdb.cpp ../mxsystem.cpp)

if(WIN32)
	target_link_libraries(test_rdb wsock32 ws2_32 Shlwapi)
endif()

target_link_libraries(test_rdb mxrdb mxhttp mxrpc mxbck)

target_include_directories(test_rdb PRIVATE
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)

add_test(test_rdb test_rdb)

add_executable(test_drv drv.cpp ../mxsystem.cpp)

if(WIN32)
	target_link_libraries(test_drv PRIVATE wsock32 ws2_32 Shlwapi)
endif()

if(USB_SUPPORT)
	find_package(libusb REQUIRED)
endif()

target_link_libraries(test_drv PRIVATE mxdrv mxrdb mxhttp mxrpc mxbck)

target_include_directories(test_drv PRIVATE
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
	${LIBUSB_INCLUDE_DIR}
)

if(USB_SUPPORT)
	target_link_libraries(test_drv PRIVATE ${LIBUSB_LIBRARY})
endif()

add_test(test_drv test_drv)

add_executable(test_argparse argparse.cpp ../mxsystem.cpp)

if(WIN32)
	target_link_libraries(test_argparse wsock32 ws2_32 Shlwapi)
endif()

target_link_libraries(test_argparse mxrdb mxhttp mxrpc mxbck)

target_include_directories(test_argparse PRIVATE
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)

add_test(test_argparse test_argparse)

add_executable(test_evt evt.cpp ../mxsystem.cpp)

if(WIN32)
	target_link_libraries(test_evt wsock32 ws2_32 Shlwapi)
endif()

target_link_libraries(test_evt mxrdb mxhttp mxrpc mxbck)

target_include_directories(test_evt PRIVATE
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)
add_test(test_evt test_evt)

add_executable(test_bck backend.cpp)

target_link_libraries(test_bck mxbck)

target_include_directories(test_bck PRIVATE
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)
add_test(test_bck test_bck)

add_executable(test_ksmatch ksmatch.cpp ../mxsystem.cpp)

if(WIN32)
	target_link_libraries(test_ksmatch wsock32 ws2_32 Shlwapi)
endif()

target_link_libraries(test_ksmatch mxrdb mxhttp mxrpc mxbck)

target_include_directories(test_ksmatch PRIVATE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)

add_test(test_ksmatch test_ksmatch)

add_executable(test_listen listen_bck.cpp)

target_link_libraries(test_listen mxbck)

target_include_directories(test_listen PRIVATE
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)
add_test(test_listen test_listen)
