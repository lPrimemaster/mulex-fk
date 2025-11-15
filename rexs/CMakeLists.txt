add_executable(mxrexs
	main.cpp
)

# Link main
target_link_libraries(mxrexs PRIVATE mxapi)

if(WIN32)
	target_link_libraries(mxrexs PRIVATE advapi32)
endif()

# For config
target_include_directories(mxrexs PRIVATE
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)
