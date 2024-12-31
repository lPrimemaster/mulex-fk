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
	execute_process(
		COMMAND yarn build
		COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/frontend/dist/ ${CMAKE_BINARY_DIR}/dist/
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/frontend
	)

	# Read the files back and embed them into the executable via mxres.h.in
	file(GLOB_RECURSE files "${CMAKE_BINARY_DIR}/dist/*")
	foreach(file ${files})
		mx_resource_append(${file})
	endforeach()
endmacro()
