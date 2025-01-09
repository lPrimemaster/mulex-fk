add_library(MxBackend
	../mxsystem.cpp
	mxbackend.cpp
)

if(WIN32)
	target_link_libraries(MxBackend wsock32 ws2_32 Shlwapi)
endif()

target_link_libraries(MxBackend mxrdb mxhttp mxrpc)

add_library(mxbck ALIAS MxBackend)

include(../cmake/InstallMulex.cmake)
set(LIB_DEPS
	"find_package(ZLIB REQUIRED)
find_package(Tracy REQUIRED)
find_package(MxRun REQUIRED)
find_package(MxRpc REQUIRED)
find_package(MxRdb REQUIRED)
find_package(uSockets REQUIRED)
find_package(RapidJSON REQUIRED)
find_package(base64 REQUIRED)
find_package(MxHttp REQUIRED)"
)
install_target(MxBackend mxbackend.h)
set(LIB_DEPS "")
