message(STATUS "Copying required dlls...")
file(GLOB RUNTIME_DEPS "${CMAKE_BINARY_DIR}/*.dll")
install(FILES ${RUNTIME_DEPS} DESTINATION bin)
