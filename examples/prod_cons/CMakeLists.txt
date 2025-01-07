add_executable(temp_reader temp_reader.cpp ../../bck/mxbackend.cpp ../../mxsystem.cpp)

if(WIN32)
	target_link_libraries(temp_reader wsock32 ws2_32 Shlwapi)
endif()

target_link_libraries(temp_reader mxrdb mxhttp mxrpc mxdrv)

target_include_directories(temp_reader PRIVATE
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)

add_executable(signal_reader signal_reader.cpp ../../bck/mxbackend.cpp ../../mxsystem.cpp)

if(WIN32)
	target_link_libraries(signal_reader wsock32 ws2_32 Shlwapi)
endif()

target_link_libraries(signal_reader mxrdb mxhttp mxrpc mxdrv)

target_include_directories(signal_reader PRIVATE
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)

add_executable(coincidence_analysis coincidence_analysis.cpp ../../bck/mxbackend.cpp ../../mxsystem.cpp)

if(WIN32)
	target_link_libraries(coincidence_analysis wsock32 ws2_32 Shlwapi)
endif()

target_link_libraries(coincidence_analysis mxrdb mxhttp mxrpc mxdrv)

target_include_directories(coincidence_analysis PRIVATE
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)

add_executable(producer_be producer_be.cpp ../../bck/mxbackend.cpp ../../mxsystem.cpp)

if(WIN32)
	target_link_libraries(producer_be wsock32 ws2_32 Shlwapi)
endif()

target_link_libraries(producer_be mxrdb mxhttp mxrpc mxdrv)

target_include_directories(producer_be PRIVATE
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)

add_executable(consumer_be consumer_be.cpp ../../bck/mxbackend.cpp ../../mxsystem.cpp)

if(WIN32)
	target_link_libraries(consumer_be wsock32 ws2_32 Shlwapi)
endif()

target_link_libraries(consumer_be mxrdb mxhttp mxrpc mxdrv)

target_include_directories(consumer_be PRIVATE
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)
