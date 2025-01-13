if(USB_SUPPORT)
	find_package(libusb REQUIRED)
endif()

add_library(MxDrivers
	../network/socket.cpp
	../mxsystem.cpp
	mxdrv.cpp
)

target_link_libraries(MxDrivers PRIVATE ${LIBUSB_LIBRARY})
target_include_directories(MxDrivers PRIVATE ${LIBUSB_INCLUDE_DIR})

add_library(mxdrv ALIAS MxDrivers)

include(../cmake/InstallMulex.cmake)
if(USB_SUPPORT)
	set(LIB_DEPS
"
if(WIN32)
	find_package(libusb REQUIRED)
endif()
"
	)
else()
	set(LIB_DEPS "")
endif()
install_target(MxDrivers mxdrv.h)
set(LIB_DEPS "")
