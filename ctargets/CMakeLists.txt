add_executable(rdbreader rdbreader.cpp ../rdb/mxrdb.cpp ../network/rpc.cpp  ../network/mxmsg.cpp ../network/socket.cpp ../mxsystem.cpp)

if(WIN32)
	target_link_libraries(rdbreader wsock32 ws2_32 Shlwapi)
endif()

target_include_directories(rdbreader PRIVATE
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)
