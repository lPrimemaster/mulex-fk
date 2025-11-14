set(MXRES_MAP_ENTRIES "")

if(WIN32)
	set(RC_FILE "${CMAKE_BINARY_DIR}/resources_tpl.rc")
	file(REMOVE "${RC_FILE}")
	set(MXRES_WIN32_NAME_RES "")
endif()


macro(mx_resource_clear)
	set(MXRES_MAP_ENTRIES "")
	if(WIN32)
		set(MXRES_WIN32_NAME_RES "")
		file(REMOVE "${RC_FILE}")
	endif()
endmacro()

find_package(Python3 COMPONENTS Interpreter REQUIRED)

macro(mx_resource_append fileni)
	set(extra_args ${ARGN})
	list(LENGTH extra_args extra_count)
	set(namespace_arg "")

	# file(READ ${fileni} filecontent HEX)
	set(file_i_var ${fileni})
	cmake_path(GET file_i_var FILENAME filename)

	set(CPP_ARRAY "")

	if(WIN32)
		# On Windows, cl.exe does not like big arrays
		# We use the built-in resource support
		list(LENGTH MXRES_WIN32_NAME_RES MME_LEN)
		set(IDR_NAME "IDR_${MME_LEN}")
		file(APPEND ${RC_FILE} "${IDR_NAME} RCDATA \"${fileni}\"\n")
	else()
		# On Linux generate resources by creating large arrays
		execute_process(
			COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/ctgen/filehex2array.py" "${fileni}"
			OUTPUT_VARIABLE CPP_ARRAY
		)
	endif()

	if(${extra_count} GREATER 0)
		list(GET extra_args 0 namespace_arg)
		message(STATUS "[mxres] Adding resource: ${fileni} | Alias: ${namespace_arg}::${filename}")
		if(WIN32)
			list(APPEND MXRES_WIN32_NAME_RES "{ \"${namespace_arg}::${filename}\", \"${IDR_NAME}\" },")
			list(APPEND MXRES_MAP_ENTRIES "{ \"${namespace_arg}::${filename}\", ResLoadWin32Resource(\"${namespace_arg}::${filename}\") },")
		else()
			list(APPEND MXRES_MAP_ENTRIES "{ \"${namespace_arg}::${filename}\", { ${CPP_ARRAY} } },")
		endif()
	else()
		message(STATUS "[mxres] Adding resource: ${fileni} | Alias: ${filename}")
		if(WIN32)
			list(APPEND MXRES_WIN32_NAME_RES "{ \"${filename}\", \"${IDR_NAME}\" },")
			list(APPEND MXRES_MAP_ENTRIES "{ \"${filename}\", ResLoadWin32Resource(\"${filename}\") },")
		else()
			list(APPEND MXRES_MAP_ENTRIES "{ \"${filename}\", { ${CPP_ARRAY} } },")
		endif()
	endif()
endmacro()

macro(mx_resource_gen target)
	message(STATUS "[mxres] Generating resource header...")
	string(REPLACE ";" "\n" MXRES_MAP_ENTRIES "${MXRES_MAP_ENTRIES}")
	if(WIN32)
		string(REPLACE ";" "\n" MXRES_WIN32_NAME_RES "${MXRES_WIN32_NAME_RES}")
		file(COPY_FILE "${RC_FILE}" "${CMAKE_CURRENT_BINARY_DIR}/resources.rc")
		target_sources(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/resources.rc")
	endif()
	configure_file(${CMAKE_SOURCE_DIR}/mxres.h.in mxres.h @ONLY)
endmacro()

macro(build_frontend_yarn)
	# Build frontend at configure time
	message(STATUS "Building frontend via 'yarn build'...")

	if(WIN32 AND NOT CROSS_COMP_WINDOWS)
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

	# Add the mx logo
	mx_resource_append("${CMAKE_SOURCE_DIR}/frontend/src/assets/logo.png")
endmacro()
