macro(install_mulex)
	# Install server binary
	install(TARGETS mxmain DESTINATION bin COMPONENT Runtime)

	# Install plugin manager
	install(TARGETS mxplug DESTINATION bin COMPONENT Runtime)

	# Install rex remote executioner
	install(TARGETS mxrexs DESTINATION bin COMPONENT Runtime)

	# Common includes
	install(FILES network/socket.h DESTINATION include/network COMPONENT Lib)
	install(FILES network/rpc.h DESTINATION include/network COMPONENT Lib)

	file(GLOB COMMON_HEADERS ${CMAKE_SOURCE_DIR}/*.h)
	install(FILES ${COMMON_HEADERS} DESTINATION include COMPONENT Lib)

	# Install required runtime dlls on windows
	# vcpkg already puts the required dlls on the build tree
	if(WIN32)
		message(STATUS "Copying required dlls...")
		install(FILES $<TARGET_RUNTIME_DLLS:mxmain> DESTINATION bin COMPONENT Runtime)
		# HACK: (Cesar) This works on CI/CD but might break...
		install(FILES "${CMAKE_BINARY_DIR}/libzlib1.dll" DESTINATION bin COMPONENT Runtime)
	endif()
endmacro()

macro(install_target target header)
	include(CMakePackageConfigHelpers)

	install(TARGETS ${target}
		EXPORT ${target}Targets

		ARCHIVE DESTINATION lib
		COMPONENT Lib

		INCLUDES DESTINATION include
		COMPONENT Lib
	)

	set(extra_args ${ARGN})
	list(LENGTH extra_args extra_count)
	set(dir_arg "")

	if(${extra_count} GREATER 0)
		list(GET extra_args 0 dir_arg)
	endif()

	if(NOT ${header} STREQUAL NONE)
		install(FILES ${CMAKE_SOURCE_DIR}/${header} DESTINATION include/${dir_arg} COMPONENT Lib)
	endif()

	set(LIB_TARGET ${target})

	configure_package_config_file(
		${CMAKE_SOURCE_DIR}/LibConfig.cmake.in
		${CMAKE_CURRENT_BINARY_DIR}/${target}Config.cmake
		INSTALL_DESTINATION lib/cmake/${target}
	)

	configure_file(${CMAKE_SOURCE_DIR}/LibConfigVersion.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/${target}ConfigVersion.cmake)

	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${target}Config.cmake DESTINATION lib/cmake/${target} COMPONENT Modules)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${target}ConfigVersion.cmake DESTINATION lib/cmake/${target} COMPONENT Modules)
	install(EXPORT ${target}Targets FILE ${target}Targets.cmake NAMESPACE Mx:: DESTINATION lib/cmake/${target} COMPONENT Modules)
endmacro()
