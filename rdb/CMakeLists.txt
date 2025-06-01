# find_package(SQLite3)

message(STATUS ${sqlite3_SOURCE_DIR})

add_library(MxRdb
	${sqlite3_SOURCE_DIR}/sqlite3.c
	mxrdb.cpp
	mxpdb.cpp
	mxfdb.cpp
)

target_include_directories(MxRdb PRIVATE
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
	${sqlite3_SOURCE_DIR}
)

add_library(mxrdb ALIAS MxRdb)

include(../cmake/InstallMulex.cmake)
install_target(MxRdb mxrdb.h)
