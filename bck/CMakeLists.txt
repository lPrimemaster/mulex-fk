add_library(MxBackend
	../mxsystem.cpp
	mxbackend.cpp
)

if(WIN32)
	target_link_libraries(MxBackend wsock32 ws2_32 Shlwapi pdh)
endif()

target_link_libraries(MxBackend mxrdb mxhttp mxrpc)

# Enforce c++20
target_compile_features(MxBackend PUBLIC cxx_std_20)

# Set MX_RPC_METHOD keyword as a no op
target_compile_definitions(MxBackend PUBLIC MX_RPC_METHOD=)

add_library(mxbck ALIAS MxBackend)

include(../cmake/InstallMulex.cmake)
set(LIB_DEPS
"
find_package(ZLIB REQUIRED)
find_package(Tracy REQUIRED)
find_package(MxRun REQUIRED)
find_package(MxRpc REQUIRED)
find_package(MxRdb REQUIRED)
find_package(uSockets REQUIRED)
find_package(RapidJSON REQUIRED)
find_package(base64 REQUIRED)
find_package(libuv REQUIRED)
find_package(MxHttp REQUIRED)
"
)
install_target(MxBackend mxbackend.h)
set(LIB_DEPS "")
