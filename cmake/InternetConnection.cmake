macro(check_network_connection)
	if(WIN32)
		execute_process(
			COMMAND ping www.google.com -n 2
			ERROR_QUIET
			RESULT_VARIABLE NO_CONNECTION
		)
	else()
		execute_process(
			COMMAND ping www.google.com -c 2
			ERROR_QUIET
			RESULT_VARIABLE NO_CONNECTION
		)
	endif()
	if(NO_CONNECTION EQUAL 0)
		set(NETWORK_STATUS True)
	else()
		set(NETWORK_STATUS False)
	endif()
	# return(PROPAGATE ${status})
endmacro()