# find_package(SQLite3)

message(STATUS ${sqlite3_SOURCE_DIR})

add_library(mxrdb
	${sqlite3_SOURCE_DIR}/sqlite3.c
	mxrdb.cpp
	mxpdb.cpp
)

target_include_directories(mxrdb PRIVATE
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
	${sqlite3_SOURCE_DIR}
)
