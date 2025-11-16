macro(install_mulex)
	# Install server binary
	install(TARGETS mxmain DESTINATION bin)

	# Install plugin manager
	install(TARGETS mxplug DESTINATION bin)

	# Install rex remote executioner
	install(TARGETS mxrexs DESTINATION bin)

	# Common includes
	install(FILES network/socket.h DESTINATION include/network)
	install(FILES network/rpc.h DESTINATION include/network)

	file(GLOB COMMON_HEADERS ${CMAKE_SOURCE_DIR}/*.h)
	install(FILES ${COMMON_HEADERS} DESTINATION include)

	# Install python library
	install(DIRECTORY ${CMAKE_SOURCE_DIR}/pymx/ DESTINATION pymx
		PATTERN "__pycache__" EXCLUDE
		PATTERN ".gitignore" EXCLUDE
		PATTERN "test" EXCLUDE
	)
	install(TARGETS mxcapi DESTINATION pymx)

	# Install required runtime dlls on windows
	# vcpkg already puts the required dlls on the build tree
	if(WIN32)
		set_property(TARGET mxmain PROPERTY INSTALL_RPATH_USE_LINK_PATH TRUE)
		set_property(TARGET mxplug PROPERTY INSTALL_RPATH_USE_LINK_PATH TRUE)
		set_property(TARGET mxrexs PROPERTY INSTALL_RPATH_USE_LINK_PATH TRUE)

		install(FILES $<TARGET_RUNTIME_DLLS:mxmain> DESTINATION bin)
		install(FILES $<TARGET_RUNTIME_DLLS:mxplug> DESTINATION bin)
		install(FILES $<TARGET_RUNTIME_DLLS:mxrexs> DESTINATION bin)

		install(DIRECTORY ${CMAKE_BINARY_DIR}/$<$<CONFIG:Debug>:Debug>$<$<CONFIG:Release>:Release>/ DESTINATION bin FILES_MATCHING PATTERN "*.dll")
	endif()
endmacro()

macro(install_target target header)
	include(CMakePackageConfigHelpers)

	install(TARGETS ${target}
		EXPORT ${target}Targets
		ARCHIVE DESTINATION lib
		INCLUDES DESTINATION include
	)

	set(extra_args ${ARGN})
	list(LENGTH extra_args extra_count)
	set(dir_arg "")

	if(${extra_count} GREATER 0)
		list(GET extra_args 0 dir_arg)
	endif()

	if(NOT ${header} STREQUAL NONE)
		install(FILES ${CMAKE_SOURCE_DIR}/${header} DESTINATION include/${dir_arg})
	endif()

	set(LIB_TARGET ${target})

	configure_package_config_file(
		${CMAKE_SOURCE_DIR}/LibConfig.cmake.in
		${CMAKE_CURRENT_BINARY_DIR}/${target}Config.cmake
		INSTALL_DESTINATION lib/cmake/${target}
	)

	configure_file(${CMAKE_SOURCE_DIR}/LibConfigVersion.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/${target}ConfigVersion.cmake)

	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${target}Config.cmake DESTINATION lib/cmake/${target})
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${target}ConfigVersion.cmake DESTINATION lib/cmake/${target})
	install(EXPORT ${target}Targets FILE ${target}Targets.cmake NAMESPACE Mx:: DESTINATION lib/cmake/${target})
endmacro()
