include(../cmake/InstallMulex.cmake)

add_library(MxRun
	mxrun.cpp
)

target_include_directories(MxRun PRIVATE
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
	mxrdb
)

add_library(mxrun ALIAS MxRun)
install_target(MxRun mxrun.h)
