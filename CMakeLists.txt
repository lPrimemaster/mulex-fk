cmake_minimum_required(VERSION 3.21)
include(FetchContent)
include(cmake/InternetConnection.cmake)
include(cmake/BuildManifest.cmake)
include(cmake/BuildFrontend.cmake)
include(cmake/InstallMulex.cmake)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Options
option(LTRACE "Enable/Disable trace logging." OFF)
option(PROFILE "Enable/Disable profiling with tracy." OFF)
option(USB_SUPPORT "Enable/Disable bulk USB transfer support with libusb." ON)
option(BUILD_TESTS "Build tests." OFF)
option(BUILD_EXAMPLES "Build examples." OFF)
option(BUILD_CTARGETS "Build extra targets." OFF)
option(GH_ACTION_MODE "Signals this configure is running in a github action." OFF)
option(CROSS_COMP_WINDOWS "When on UNIX sets the mode to cross compile to windows x64 using mingw." OFF)

if(CROSS_COMP_WINDOWS)
	set(CMAKE_TOOLCHAIN_FILE "cmake/MingwToolchain.cmake")
endif()

# Version configuration
project(mulex-fk VERSION 0.9.4)
set(PROJECT_VNAME Ross)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Disable fetch content network check for gh actions
if(NOT GH_ACTION_MODE)
	# Check internet connection
	check_network_connection()
	if(NOT NETWORK_STATUS)
		set(FETCHCONTENT_FULLY_DISCONNECTED ON)
		message(WARNING "FetchContent offline mode. '_deps' need to be available and populated.")
	else()
		set(FETCHCONTENT_FULLY_DISCONNECTED OFF)
	endif()
endif()

# Use vcpkg if we are on windows
if(WIN32)
	if(CROSS_COMP_WINDOWS)
		message(WARNING "Cross compiling for Windows (x64).")

		# Make afunix.h available
		# https://raw.githubusercontent.com/mingw-w64/mingw-w64/refs/heads/master/mingw-w64-headers/include/afunix.h
	endif()
	message(WARNING "On windows, all packages must be installed as x64-mingw-dynamic if using vcpkg.")
	set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic" CACHE STRING "Vcpkg target triplet.")
endif()

if(LTRACE)
	add_definitions(-DLTRACE)
endif()

if(USB_SUPPORT)
	add_definitions(-DUSB_SUPPORT)
endif()

# SQLite3
FetchContent_Declare(
	sqlite3
	URL https://www.sqlite.org/2024/sqlite-amalgamation-3470000.zip
	URL_HASH SHA3_256=e35ee48efc24fe58d0e9102034ac0a41e3904641a745e94ab11a841fe9d7355e
	EXCLUDE_FROM_ALL
)
if (NOT sqlite3_POPULATED)
	FetchContent_Populate(sqlite3)
endif()

if(PROFILE)
	set(TRACY_ENABLE ON CACHE BOOL "Enable Tracy" FORCE)
else()
	set(TRACY_ENABLE OFF CACHE BOOL "Enable Tracy" FORCE)
endif()

# Tracy Profiler
FetchContent_Declare(
	tracy
	GIT_REPOSITORY https://github.com/wolfpld/tracy.git
	GIT_TAG v0.11.1
	GIT_SHALLOW TRUE
	# EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(tracy)

# RPC related configuration
set(RPC_CALL_KEYWORD "MX_RPC_METHOD")
add_definitions(-D${RPC_CALL_KEYWORD}=)
set(RPC_SPEC_FILE rpcspec.inl)

# Set global warnings all
add_compile_options(
	-Wall										# All warnings!
	-Wextra										# More warnings!
	-Wno-unused-parameter						# Unused parameters are common on some scenarios (implicit constructor deductions etc)
												# So I don't need to be using [[maybe_unused]] every single time I do that (but really should)
	-Wno-implicit-fallthrough					# I like implicit fallthroughs
	$<$<COMPILE_LANGUAGE:CXX>:-Wno-reorder>     # Usually I don't use this and when I do, stuff don't depend on one another (this could mean bugs tho...)
	-Wno-unused-function 						# RPC Generation might be usefull on some compilation units in the future
)

# Make sure tracy profiler is installed/linked system wide
link_libraries(TracyClient)
include_directories(${tracy_SOURCE_DIR}/public)

add_executable(mxmain
	main.cpp
)

# Generate the <mxconfig.h> file
set(CONFIG_HEADER_FILE "${CMAKE_BINARY_DIR}/mxconfig.h")
set_property(SOURCE ${CONFIG_HEADER_FILE} PROPERTY GENERATED TRUE)
add_custom_target(generate_build_info ALL
	COMMAND ${CMAKE_COMMAND}
		-DCONFIG_HEADER_FILE=${CONFIG_HEADER_FILE}
		-DPROJECT_VNAME=${PROJECT_VNAME}
		-DPROJECT_VERSION=${CMAKE_PROJECT_VERSION}
		-P ${CMAKE_SOURCE_DIR}/cmake/GenerateConfigInfo.cmake
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	BYPRODUCTS ${CONFIG_HEADER_FILE}
	COMMENT "Checking mxconfig.h"
	VERBATIM
)
add_dependencies(mxmain generate_build_info)
target_include_directories(mxmain PRIVATE
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)

# Set global windows libs
if(WIN32)
	set(WINDOWS_GLIBS pdh)
endif()

# Add mulex functionalities
add_subdirectory(network)
add_subdirectory(rdb)
add_subdirectory(drv)
add_subdirectory(run)
add_subdirectory(bck)
add_subdirectory(plug)
add_subdirectory(rexs)

if(BUILD_CTARGETS)
	add_subdirectory(ctargets)
endif()

if(BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()

# Link main
target_link_libraries(mxmain PRIVATE mxrpc mxrdb mxdrv mxhttp mxbck)

if(WIN32)
	target_link_libraries(mxmain PRIVATE ${WINDOWS_GLIBS})

	# Statically link against C runtime (assuming we use mingw)
	target_link_options(mxmain PRIVATE -static-libstdc++ -static-libgcc)
endif()

target_compile_definitions(mxmain PUBLIC L2FILE)

# Clear resources from 'plug'
mx_resource_clear()

# Invoke yarn build (pre build)
build_manifest_json()
build_frontend_yarn()

# Add testing
if(BUILD_TESTS)
	enable_testing()
	add_subdirectory(test)
endif()

# Generate binary resources (last configure step)

set(vite_config_ts ${CMAKE_SOURCE_DIR}/network/vite.config.ts)
mx_resource_append(${vite_config_ts})

mx_resource_gen()

# Finally install everything for the user
install_mulex()

if(NOT WIN32)
	# Generate debian package using CPack
	include(InstallRequiredSystemLibraries)

	set(CPACK_DEBIAN_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
	set(CPACK_PACKAGE_VENDOR "César Godinho")
	set(CPACK_DEBIAN_PACKAGE_MAINTAINER "c.godinho@campus.fct.unl.pt")
	set(CPACK_PACKAGE_VERSION_MAJOR "${CMAKE_PROJECT_VERSION_MAJOR}")
	set(CPACK_PACKAGE_VERSION_MINOR "${CMAKE_PROJECT_VERSION_MINOR}")
	set(CPACK_PACKAGE_VERSION_PATCH "${CMAKE_PROJECT_VERSION_PATCH}")

	set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
	set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

	set(CPACK_DEBIAN_PACKAGE_DEPENDS "libuv1-dev, zlib1g-dev")

	# TODO: (Cesar) This is dangerous and should be fixed
	#				For now assuming that when we generate a cpack .deb package
	#				the cmake install prefix is set to /usr/local (default)
	set(CPACK_PACKAGING_INSTALL_PREFIX "/usr/local")
	set(CPACK_GENERATOR "DEB")
	set(CPACK_SOURCE_GENERATOR "DEB")
else()
	# Generate NSIS package using CPack
	include(InstallRequiredSystemLibraries)

	set(CPACK_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
	set(CPACK_PACKAGE_VENDOR "César Godinho")
	set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Mulex-fk Windows (x64) installer.")
	set(CPACK_PACKAGE_VERSION_MAJOR "${CMAKE_PROJECT_VERSION_MAJOR}")
	set(CPACK_PACKAGE_VERSION_MINOR "${CMAKE_PROJECT_VERSION_MINOR}")
	set(CPACK_PACKAGE_VERSION_PATCH "${CMAKE_PROJECT_VERSION_PATCH}")

	set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
	set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
	set(CPACK_NSIS_LICENSE_FILE "${CMAKE_SOURCE_DIR}/LICENSE")

	set(CPACK_PACKAGE_INSTALL_DIRECTORY "mulex-fk")
	set(CPACK_NSIS_MODIFY_PATH ON)

	set(CPACK_GENERATOR "NSIS")
	set(CPACK_SOURCE_GENERATOR "NSIS")
endif()
include(CPack)

