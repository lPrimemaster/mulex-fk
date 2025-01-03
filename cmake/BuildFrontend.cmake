set(MXRES_MAP_ENTRIES "")

macro(mx_resource_append fileni)
	file(READ ${fileni} filecontent HEX)
	set(file_i_var ${fileni})
	cmake_path(GET file_i_var FILENAME filename)
	message(STATUS "[mxres] Adding resource: ${fileni} | Alias: ${filename}")
	list(APPEND MXRES_MAP_ENTRIES "{ \"${filename}\", ResParseResourceString(\"${filecontent}\") },")
endmacro()

macro(mx_resource_gen)
	message(STATUS "[mxres] Generating resource header...")
	string(REPLACE ";" "\n" MXRES_MAP_ENTRIES "${MXRES_MAP_ENTRIES}")
	configure_file(mxres.h.in mxres.h @ONLY)
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
