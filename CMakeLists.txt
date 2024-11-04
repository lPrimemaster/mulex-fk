cmake_minimum_required(VERSION 3.10)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

project(mulex-fk VERSION 0.1.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LTRACE "Enable/Disable trace logging." OFF)
option(USB_SUPPORT "Enable/Disable bulk USB transfer support with libusb." ON)

if(LTRACE)
	add_definitions(-DLTRACE)
endif()

if(USB_SUPPORT)
	add_definitions(-DUSB_SUPPORT)
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
