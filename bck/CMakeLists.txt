add_library(MxBackend
	mxbackend.cpp
	$<TARGET_OBJECTS:mxsys>
)

if(WIN32)
	target_link_libraries(MxBackend PUBLIC wsock32 ws2_32 Shlwapi pdh)
endif()

target_link_libraries(MxBackend PUBLIC mxrdb mxhttp mxrpc mxlbk)
# target_link_libraries(MxBackend PRIVATE $<TARGET_OBJECTS:mxsys>)
# target_link_libraries(MxBackend mxrdb mxrpc)

# Enforce c++20
target_compile_features(MxBackend PUBLIC cxx_std_20)

# Set MX_RPC_METHOD keyword as a no op
target_compile_definitions(MxBackend PUBLIC MX_RPC_METHOD=)

add_library(mxbck ALIAS MxBackend)

target_include_directories(MxBackend PRIVATE
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)

include(../cmake/InstallMulex.cmake)
set(LIB_DEPS
"
include(CMakeFindDependencyMacro)
find_dependency(ZLIB REQUIRED)
find_dependency(Tracy REQUIRED)
find_dependency(MxRun REQUIRED)
find_dependency(MxRpc REQUIRED)
find_dependency(MxRdb REQUIRED)
find_dependency(uSockets REQUIRED)
find_dependency(base64 REQUIRED)
if(WIN32)
	find_dependency(libuv REQUIRED)
endif()
find_dependency(MxHttp REQUIRED)
find_dependency(MxLbk REQUIRED)
"
)
install_target(MxBackend mxbackend.h)
set(LIB_DEPS "")
