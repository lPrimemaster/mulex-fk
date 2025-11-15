add_executable(mxrexs
	main.cpp
	mxrexs.cpp
)

# Link main
target_link_libraries(mxrexs PRIVATE mxrpc mxrdb mxdrv mxhttp mxbck mxlbk)

if(WIN32)
	target_link_libraries(mxrexs PRIVATE pdh advapi32)

	# Statically link against C runtime (assuming we use mingw)
	# target_link_options(mxrexs PRIVATE -static-libstdc++ -static-libgcc)
endif()

# For config
target_include_directories(mxrexs PRIVATE
	$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
)

add_custom_command(TARGET mxrexs POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:mxrexs> $<TARGET_FILE_DIR:mxrexs>
	COMMAND_EXPAND_LISTS
)
