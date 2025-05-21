add_executable(rdbreader rdbreader.cpp)

if(WIN32)
	target_link_libraries(rdbreader wsock32 ws2_32 Shlwapi)
endif()

target_link_libraries(rdbreader mxrdb mxrpc mxhttp mxsys)

target_include_directories(rdbreader PRIVATE
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)
