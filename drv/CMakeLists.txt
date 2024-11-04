if(USB_SUPPORT)
	find_package(libusb REQUIRED)
endif()

add_library(mxdrv
	../rpc/socket.cpp
	../mxsystem.cpp
	mxdrv.cpp
)

target_link_libraries(mxdrv PRIVATE ${LIBUSB_LIBRARY})
target_include_directories(mxdrv PRIVATE ${LIBUSB_INCLUDE_DIR})
