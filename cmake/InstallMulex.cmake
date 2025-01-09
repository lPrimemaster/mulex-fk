macro(install_mulex)
	# Install server binary
	install(TARGETS mxmain DESTINATION bin)

	# Common includes
	install(FILES network/socket.h DESTINATION include/network)
	install(FILES network/rpc.h DESTINATION include/network)
endmacro()

macro(install_target target header)
	include(CMakePackageConfigHelpers)

	install(TARGETS ${target}
		EXPORT ${target}Targets
		ARCHIVE DESTINATION lib
		INCLUDES DESTINATION include
	)

	if(NOT ${header} STREQUAL NONE)
		install(FILES ${CMAKE_SOURCE_DIR}/${header} DESTINATION include)
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
