cmake_minimum_required(VERSION 3.10)

project(mulex-fk VERSION 0.1.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LTRACE "Enable/Disable trace logging." OFF)

if(LTRACE)
	add_definitions(-DLTRACE)
endif()

# RPC related configuration
set(RPC_CALL_KEYWORD "MX_RPC_METHOD")
set(RPC_CALL_RAW_KEYWORD "MX_RPC_RAW_METHOD")
add_definitions(-D${RPC_CALL_KEYWORD}=)
add_definitions(-D${RPC_CALL_RAW_KEYWORD}=)
set(RPC_SPEC_FILE rpcspec.inl)

add_executable(mxmain
	main.cpp
)

# Add mulex functionalities
add_subdirectory(rpc)
add_subdirectory(rdb)

# Link main
target_link_libraries(mxmain PRIVATE mxrpc mxrdb)

# Add testing
enable_testing()
add_subdirectory(test)
