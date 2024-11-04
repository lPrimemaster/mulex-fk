if(LIBUSB_LIBRARY AND LIBSUB_INCLUDE_DIR)
	set(LIBUSB_FOUND True)
else()
	find_path(LIBUSB_INCLUDE_DIR
		NAMES libusb.h
		PATHS
			# Linux
			/usr/include
			/usr/local/include
			/opt/local/include
			/sw/include

			# Windows - search the prefix path
			${CMAKE_PREFIX_PATH}/include

		PATH_SUFFIXES
			libusb-1.0
	)

if(WIN32)
	set(LUSB_NAMES usb-1.0.a) # Ignore .dll.a (AKA .so)
else()
	set(LUSB_NAMES usb-1.0 usb)
endif()

	find_library(LIBUSB_LIBRARY
		NAMES ${LUSB_NAMES}
		PATHS
			# Linux
			/usr/lib
			/usr/local/lib
			/opt/local/lib
			/sw/lib

			# Windows (We only support MinGW) - search the prefix path
			${CMAKE_PREFIX_PATH}/MinGW64/static
	)

	if(LIBUSB_INCLUDE_DIR AND LIBUSB_LIBRARY)
		set(LIBUSB_FOUND True)
		if(NOT libusb_FIND_QUIETLY)
			message(STATUS "Found libusb-1.0")
		endif()
	endif()

	if(NOT LIBUSB_FOUND)
		if(libusb_FIND_REQUIRED)
			message(FATAL_ERROR "Could not find libusb")
		endif()
	endif()
endif()
