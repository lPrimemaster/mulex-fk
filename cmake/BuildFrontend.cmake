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

	if(WIN32)
		set(YARN_CMD cmd.exe /C yarn)
	else()
		set(YARN_CMD yarn)
	endif()

	if(NOT EXISTS ${CMAKE_SOURCE_DIR}/frontend/node_modules/)
		message(STATUS "Fetching node dependencies...")
		execute_process(
			COMMAND ${YARN_CMD}
			WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/frontend
			COMMAND_ECHO STDOUT
		)
	endif()

	execute_process(
		COMMAND ${YARN_CMD} build
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/frontend
		COMMAND_ECHO STDOUT
	)

	# Read the files back and embed them into the executable via mxres.h.in
	file(GLOB_RECURSE files "${CMAKE_SOURCE_DIR}/frontend/dist/*")
	foreach(file ${files})
		mx_resource_append(${file})
	endforeach()

	# Add the favicon
	mx_resource_append("${CMAKE_SOURCE_DIR}/frontend/src/assets/favicon.ico")
endmacro()
