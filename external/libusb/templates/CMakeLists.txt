cmake_minimum_required(VERSION 2.8)

PROJECT("libusb")

INCLUDE(CheckIncludeFiles)
INCLUDE(CheckFunctionExists)
INCLUDE(CheckLibraryExists)
INCLUDE(CheckTypeSize)


#----------------------------------------------------------------------------
#
# Get all defines for the configuration.
#
SET(ENABLE_LOGGING 1)
CHECK_INCLUDE_FILES(dlfcn.h HAVE_DLFCN_H)
CHECK_FUNCTION_EXISTS(gettimeofday HAVE_GETTIMEOFDAY)
CHECK_INCLUDE_FILES(inttypes.h HAVE_INTTYPES_H)
CHECK_LIBRARY_EXISTS(udev udev_new "" HAVE_LIBUDEV)
CHECK_INCLUDE_FILES(libudev.h HAVE_LIBUDEV_H)
CHECK_INCLUDE_FILES(memory.h HAVE_MEMORY_H)
CHECK_INCLUDE_FILES(poll.h HAVE_POLL_H)
CHECK_INCLUDE_FILES(signal.h HAVE_SIGNAL_H)
CHECK_INCLUDE_FILES(stdint.h HAVE_STDINT_H)
CHECK_INCLUDE_FILES(stdlib.h HAVE_STDLIB_H)
CHECK_INCLUDE_FILES(strings.h HAVE_STRINGS_H)
CHECK_INCLUDE_FILES(string.h HAVE_STRING_H)
CHECK_TYPE_SIZE("struct timespec" STRUCT_TIMESPEC)
CHECK_FUNCTION_EXISTS(syslog HAVE_SYSLOG_FUNC)
CHECK_INCLUDE_FILES(syslog.h HAVE_SYSLOG_H)
CHECK_INCLUDE_FILES(sys/stat.h HAVE_SYS_STAT_H)
CHECK_INCLUDE_FILES(sys/time.h HAVE_SYS_TIME_H)
CHECK_INCLUDE_FILES(sys/types.h HAVE_SYS_TYPES_H)
CHECK_INCLUDE_FILES(unistd.h HAVE_UNISTD_H)
CHECK_INCLUDE_FILES(sys/timerfd.h USBI_TIMERFD_AVAILABLE)

SET(LT_OBJDIR ".libs/")

SET(PACKAGE "libusb")
SET(PACKAGE_BUGREPORT "libusb-devel@lists.sourceforge.net")
SET(PACKAGE_NAME "libusb")
SET(PACKAGE_STRING "libusb 1.0.18")
SET(PACKAGE_TARNAME "libusb")
SET(PACKAGE_URL "http://libusb.info")
SET(PACKAGE_VERSION "1.0.18")

SET(VERSION "1.0.18")

IF(UNIX)
	SET(OS_LINUX 1)
	# Use UDEV on linux.
	SET(USE_UDEV 1)
	
	# Use POSIX threads on linux.
	SET(THREADS_POSIX 1)
	
	SET(POLL_NFDS_TYPE nfds_t)
ELSEIF(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	SET(OS_WINDOWS 1)
	
	SET(POLL_NFDS_TYPE "unsigned int")
ENDIF(UNIX)

IF(${CMAKE_COMPILER_IS_GNUCC})
	SET(_GNU_SOURCE 1)
	SET(DEFAULT_VISIBILITY "__attribute__((visibility(\"default\")))")
	SET(STDC_HEADERS 1)
ENDIF(${CMAKE_COMPILER_IS_GNUCC})

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config/config.h)


#----------------------------------------------------------------------------
#
# Compile the sources.
#

IF(UNIX)
	SET(SOURCES_libusb libusb/core.c
	                   libusb/descriptor.c
	                   libusb/io.c
	                   libusb/strerror.c
	                   libusb/sync.c
	                   libusb/hotplug.c
	                   libusb/os/threads_posix.c
	                   libusb/os/linux_usbfs.c
	                   libusb/os/poll_posix.c
	                   libusb/os/linux_udev.c
	)
	
	add_definitions(-DHAVE_CONFIG_H)
ELSEIF(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	SET(SOURCES_libusb libusb/core.c
	                   libusb/descriptor.c
	                   libusb/io.c
	                   libusb/strerror.c
	                   libusb/sync.c
	                   libusb/hotplug.c
	                   libusb/os/threads_windows.c
	                   libusb/os/poll_windows.c
	                   libusb/os/windows_usb.c
	)
	
	add_definitions(-DHAVE_CONFIG_H)
	IF(MSVC)
		INCLUDE_DIRECTORIES(msvc) # use only with Visual C++, not with MSYS
	ENDIF(MSVC)
ENDIF(UNIX)

INCLUDE_DIRECTORIES(libusb ${CMAKE_CURRENT_BINARY_DIR}/config)

ADD_LIBRARY(TARGET_libusb STATIC ${SOURCES_libusb})

# This static library will be linked with a shared object later. Add "-fPIC" on GCC systems or X64 builds break.
IF("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"  AND  "${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
	SET_TARGET_PROPERTIES(TARGET_libusb PROPERTIES COMPILE_FLAGS -fPIC)
ENDIF("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"  AND  "${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")

# Set the name of the output file to "usb".
SET_TARGET_PROPERTIES(TARGET_libusb PROPERTIES OUTPUT_NAME "usb")

SET(CMAKE_INSTALL_PREFIX /install)
INSTALL(TARGETS TARGET_libusb DESTINATION lib)
INSTALL(FILES libusb/libusb.h DESTINATION include/libusb-1.0)

