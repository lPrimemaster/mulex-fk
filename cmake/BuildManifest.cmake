macro(build_manifest_json)
	set(output "{}")

	# Version
	string(JSON output SET ${output} version \"${CMAKE_PROJECT_VERSION}\")

	# Version name
	string(JSON output SET ${output} vname \"${PROJECT_VNAME}\")

	# Write file
	file(WRITE ${CMAKE_BINARY_DIR}/manifest.json ${output})
endmacro()
