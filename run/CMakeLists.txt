add_library(mxrun
	mxrun.cpp
)

target_include_directories(mxrun PRIVATE
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
	mxrdb
)
