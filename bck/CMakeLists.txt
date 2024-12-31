add_library(mxbck
	../mxsystem.cpp
	mxbackend.cpp
)

if(WIN32)
	target_link_libraries(mxbck wsock32 ws2_32 Shlwapi)
endif()

target_link_libraries(mxbck mxrdb mxhttp mxrpc)
