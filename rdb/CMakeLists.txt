# find_package(SQLite3 REQUIRED)

add_library(mxrdb
	mxrdb.cpp
)

# target_link_libraries(mxrdb PRIVATE SQLite::SQLite3)
