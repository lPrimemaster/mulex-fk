set(MXRES_MAP_ENTRIES "")

macro(mx_resource_clear)
	set(MXRES_MAP_ENTRIES "")
endmacro()

macro(mx_resource_append fileni)
	set(extra_args ${ARGN})
	list(LENGTH extra_args extra_count)
	set(namespace_arg "")

	file(READ ${fileni} filecontent HEX)
	set(file_i_var ${fileni})
	cmake_path(GET file_i_var FILENAME filename)
	if(${extra_count} GREATER 0)
		list(GET extra_args 0 namespace_arg)
		message(STATUS "[mxres] Adding resource: ${fileni} | Alias: ${namespace_arg}::${filename}")
		list(APPEND MXRES_MAP_ENTRIES "{ \"${namespace_arg}::${filename}\", ResParseResourceString(\"${filecontent}\") },")
	else()
		message(STATUS "[mxres] Adding resource: ${fileni} | Alias: ${filename}")
		list(APPEND MXRES_MAP_ENTRIES "{ \"${filename}\", ResParseResourceString(\"${filecontent}\") },")
	endif()
endmacro()

macro(mx_resource_gen)
	message(STATUS "[mxres] Generating resource header...")
	string(REPLACE ";" "\n" MXRES_MAP_ENTRIES "${MXRES_MAP_ENTRIES}")
	configure_file(${CMAKE_SOURCE_DIR}/mxres.h.in mxres.h @ONLY)
endmacro()

macro(build_frontend_yarn)
	# Build frontend at configure time
	message(STATUS "Building frontend via 'yarn build'...")
	execute_process(
		COMMAND yarn build
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/frontend
		RESULT_VARIABLE yarn_result
		OUTPUT_VARIABLE yarn_output
		ERROR_VARIABLE yarn_error
	)

	if(yarn_result)
		message(FATAL_ERROR "Yarn build failed with error: ${yarn_error}")
	else()
		message(STATUS "Yarn build OK.")
	endif()

	# Read the files back and embed them into the executable via mxres.h.in
	file(GLOB_RECURSE files "${CMAKE_SOURCE_DIR}/frontend/dist/*")
	foreach(file ${files})
		mx_resource_append(${file})
	endforeach()
endmacro()
