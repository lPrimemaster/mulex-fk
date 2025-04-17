# macro(generate_config_info)

file(WRITE ${CONFIG_HEADER_FILE} "// mxconfig.h Generated file\n")

# Get git short hash
execute_process(
	COMMAND git rev-parse --short HEAD
	OUTPUT_VARIABLE GIT_HEAD_HASH
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get git branch name
execute_process(
	COMMAND git rev-parse --abbrev-ref HEAD
	OUTPUT_VARIABLE GIT_HEAD_BRANCH
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

file(APPEND ${CONFIG_HEADER_FILE} "#pragma once\n")
file(APPEND ${CONFIG_HEADER_FILE} "#define MX_VNAME \"${PROJECT_VNAME}\"\n")
file(APPEND ${CONFIG_HEADER_FILE} "#define MX_VSTR \"${PROJECT_VERSION}\"\n")
file(APPEND ${CONFIG_HEADER_FILE} "#define MX_HASH \"${GIT_HEAD_HASH}\"\n")
file(APPEND ${CONFIG_HEADER_FILE} "#define MX_BRANCH \"${GIT_HEAD_BRANCH}\"\n")

# add_custom_command(
# 	OUTPUT ${CONFIG_HEADER_FILE}
# 	COMMAND ${CMAKE_COMMAND} -E echo "#pragma once" > ${CONFIG_HEADER_FILE}
# 	COMMAND ${CMAKE_COMMAND} -E echo "#define MX_VNAME \"${PROJECT_VNAME}\"" >> ${CONFIG_HEADER_FILE}
# 	COMMAND ${CMAKE_COMMAND} -E echo "#define MX_VSTR \"${CMAKE_PROJECT_VERSION}\"" >> ${CONFIG_HEADER_FILE}
# 	COMMENT "Generating mxconfig.h"
# 	VERBATIM
# )

# add_custom_target(generate_build_info ALL DEPENDS ${CONFIG_HEADER_FILE})
# add_dependencies(mxmain generate_build_info)
#
# target_include_directories(mxmain PRIVATE
# 	$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
# )
# endmacro()
