# find_package(SQLite3 REQUIRED)

add_library(mxrdb
	mxrdb.cpp
)

target_include_directories(mxrdb PRIVATE
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)

# target_link_libraries(mxrdb PRIVATE SQLite::SQLite3)
