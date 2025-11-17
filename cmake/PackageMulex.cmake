# This script creates a kpm package for mulex-fk

if(WIN32)
	set(PACKAGE_NAME "windows_amd64.tar.gz")
else()
	set(PACKAGE_NAME "linux_amd64.tar.gz")
endif()

execute_process(
	COMMAND tar cfz ${CMAKE_BINARY_DIR}/${PACKAGE_NAME} -C ${CMAKE_INSTALL_PREFIX} .
)
