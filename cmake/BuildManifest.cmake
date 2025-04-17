macro(build_manifest_json)
	set(output "{}")

	# Version
	string(JSON output SET ${output} version \"${CMAKE_PROJECT_VERSION}\")

	# Version name
	string(JSON output SET ${output} vname \"${PROJECT_VNAME}\")

	# NOTE: (Cesar)
	# Here we need to use configure time
	# The manifest only makes sense if cmake configure step is ran
	# Since the build command does not regenerate the mxres.h file
	execute_process(
		COMMAND git rev-parse --short HEAD
		OUTPUT_VARIABLE GIT_HEAD_HASH
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	string(JSON output SET ${output} hash \"${GIT_HEAD_HASH}\")

	execute_process(
		COMMAND git rev-parse --abbrev-ref HEAD
		OUTPUT_VARIABLE GIT_HEAD_BRANCH
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	string(JSON output SET ${output} branch \"${GIT_HEAD_BRANCH}\")

	# Write file
	file(WRITE ${CMAKE_BINARY_DIR}/manifest.json ${output})
endmacro()
