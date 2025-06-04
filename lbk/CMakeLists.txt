add_library(MxLbk
	mxlbk.cpp
	$<TARGET_OBJECTS:mxsys>
)

add_library(mxlbk ALIAS MxLbk)

target_link_libraries(MxLbk PRIVATE mxrpc mxrdb)

include(../cmake/InstallMulex.cmake)
install_target(MxLbk mxlbk.h)
