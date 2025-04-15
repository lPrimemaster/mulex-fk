macro(generate_config_info)
	set(CONFIG_HEADER_FILE "${CMAKE_BINARY_DIR}/mxconfig.h")

	add_custom_command(
		OUTPUT ${CONFIG_HEADER_FILE}
		COMMAND ${CMAKE_COMMAND} -E echo "#pragma once" > ${CONFIG_HEADER_FILE}
		COMMAND ${CMAKE_COMMAND} -E echo "#define MX_VNAME \"${PROJECT_VNAME}\"" >> ${CONFIG_HEADER_FILE}
		COMMAND ${CMAKE_COMMAND} -E echo "#define MX_VSTR \"${CMAKE_PROJECT_VERSION}\"" >> ${CONFIG_HEADER_FILE}
		COMMENT "Generating mxconfig.h"
		VERBATIM
	)

	add_custom_target(generate_build_info ALL DEPENDS ${CONFIG_HEADER_FILE})
	add_dependencies(mxmain generate_build_info)

	target_include_directories(mxmain PRIVATE
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
	)
endmacro()
