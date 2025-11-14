set(MXRES_MAP_ENTRIES "")

if(WIN32)
	set(RC_FILE "${CMAKE_BINARY_DIR}/resources_tpl.rc")
	file(REMOVE "${RC_FILE}")
endif()


macro(mx_resource_clear)
	set(MXRES_MAP_ENTRIES "")
	if(WIN32)
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
		list(LENGTH MXRES_MAP_ENTRIES MME_LEN)
		file(APPEND ${RC_FILE} "IDR_${MME_LEN} RCDATA \"${fileni}\"\n")
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
		# list(APPEND MXRES_MAP_ENTRIES "{ \"${namespace_arg}::${filename}\", ResParseResourceString(\"${filecontent}\") },")
		list(APPEND MXRES_MAP_ENTRIES "{ \"${namespace_arg}::${filename}\", { ${CPP_ARRAY} } },")
	else()
		message(STATUS "[mxres] Adding resource: ${fileni} | Alias: ${filename}")
		# list(APPEND MXRES_MAP_ENTRIES "{ \"${filename}\", ResParseResourceString(\"${filecontent}\") },")
		list(APPEND MXRES_MAP_ENTRIES "{ \"${filename}\", { ${CPP_ARRAY} } },")
	endif()
endmacro()

macro(mx_resource_gen target)
	message(STATUS "[mxres] Generating resource header...")
	string(REPLACE ";" "\n" MXRES_MAP_ENTRIES "${MXRES_MAP_ENTRIES}")
	configure_file(${CMAKE_SOURCE_DIR}/mxres.h.in mxres.h @ONLY)
	if(WIN32)
		file(COPY_FILE "${RC_FILE}" "${CMAKE_CURRENT_BINARY_DIR}/resources.rc")
		target_sources(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/resources.rc")
	endif()
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
