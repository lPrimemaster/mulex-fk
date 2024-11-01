cmake_minimum_required(VERSION 3.10)

project(mulex-fk VERSION 0.1.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LTRACE "Enable/Disable trace logging." OFF)

if(LTRACE)
	add_definitions(-DLTRACE)
endif()

# RPC related configuration
set(RPC_CALL_KEYWORD "MX_RPC_METHOD")
add_definitions(-D${RPC_CALL_KEYWORD}=)
set(RPC_SPEC_FILE rpcspec.inl)

add_executable(mxmain
	main.cpp
)

# Add mulex functionalities
add_subdirectory(rpc)
add_subdirectory(rdb)
add_subdirectory(drv)

# Link main
target_link_libraries(mxmain PRIVATE mxrpc mxrdb mxdrv)

# Add testing
enable_testing()
add_subdirectory(test)
