add_library(MxLbk
	mxlbk.cpp
)

add_library(mxlbk ALIAS MxLbk)

target_link_libraries(MxLbk PRIVATE mxrpc mxpdb mxsys)

include(../cmake/InstallMulex.cmake)
install_target(MxLbk mxlbk.h)
