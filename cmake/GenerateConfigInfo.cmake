file(WRITE ${CONFIG_HEADER_FILE}.tmp "// mxconfig.h Generated file\n")

# Get git short hash (assume first 7 chars are always unique)
# somewhat the same as --short
execute_process(
	# COMMAND git rev-parse --short HEAD
	COMMAND git describe --match=kusefh8 --always --abbrev=7 --dirty
	OUTPUT_VARIABLE GIT_HEAD_HASH
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get git branch name
execute_process(
	COMMAND git rev-parse --abbrev-ref HEAD
	OUTPUT_VARIABLE GIT_HEAD_BRANCH
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

file(APPEND ${CONFIG_HEADER_FILE}.tmp "#pragma once\n")
file(APPEND ${CONFIG_HEADER_FILE}.tmp "#define MX_VNAME \"${PROJECT_VNAME}\"\n")
file(APPEND ${CONFIG_HEADER_FILE}.tmp "#define MX_VSTR \"${PROJECT_VERSION}\"\n")
file(APPEND ${CONFIG_HEADER_FILE}.tmp "#define MX_HASH \"${GIT_HEAD_HASH}\"\n")
file(APPEND ${CONFIG_HEADER_FILE}.tmp "#define MX_BRANCH \"${GIT_HEAD_BRANCH}\"\n")

execute_process(
	COMMAND ${CMAKE_COMMAND} -E compare_files "${CONFIG_HEADER_FILE}.tmp" "${CONFIG_HEADER_FILE}"
	RESULT_VARIABLE build_info_outdated
)

if(NOT build_info_outdated EQUAL 0)
	message(STATUS "HEAD changed: Regenerating mxconfig.h file...")
	file(RENAME "${CONFIG_HEADER_FILE}.tmp" "${CONFIG_HEADER_FILE}")
else()
	message(STATUS "HEAD unchanged.")
	file(REMOVE "${CONFIG_HEADER_FILE}.tmp")
endif()

