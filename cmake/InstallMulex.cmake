macro(install_mulex)
	# Install server
	install(TARGETS mxmain DESTINATION bin)

	# Install backend libs
	install(TARGETS mxrpc mxhttp mxrdb mxdrv mxbck mxrun DESTINATION lib)

	# Backend includes
	file(GLOB headers ${CMAKE_SOURCE_DIR}/*.h)
	install(FILES ${headers} DESTINATION include)
endmacro()
