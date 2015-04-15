cmake_minimum_required(VERSION 2.8)

#----------------------------------------------------------------------------
#
# This script provides the Quantis library to the build process.
#
# 1) It depacks the archiv.
# 2) Compile the sources.
# 3) It provides all necessary variables to link the library.
#
# The provided variables are:
#
#  LIBQUANTIS_LIBRARIES : The libquantis library with the complete path and all
#                         required system libraries. The list must be separated
#                         with whitespace.
#
#  LIBQUANTIS_DEFINES : All defines for the libquantis library. This should be a
#                       list of entries starting with "-D".
#
#  LIBQUANTIS_INCLUDE_DIRECTORIES : The include folders for the library headers.
#                                   The list must be separated with whitespace.
#

PROJECT("libquantis")

INCLUDE(ExternalProject)


#----------------------------------------------------------------------------
#
# Build the project.
#
SET(LIBQUANTIS_CMAKE_ARGS "")
LIST(APPEND LIBQUANTIS_CMAKE_ARGS "-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}")
LIST(APPEND LIBQUANTIS_CMAKE_ARGS "-DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}")
LIST(APPEND LIBQUANTIS_CMAKE_ARGS "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}")
LIST(APPEND LIBQUANTIS_CMAKE_ARGS "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}")
IF(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	LIST(APPEND LIBQUANTIS_CMAKE_ARGS "-DCMAKE_RC_COMPILER=${CMAKE_RC_COMPILER}")
ENDIF(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
LIST(APPEND LIBQUANTIS_CMAKE_ARGS "-DDISABLE_QUANTIS_JAVA=1")
LIST(APPEND LIBQUANTIS_CMAKE_ARGS "-DDISABLE_QUANTIS_PCI=1")
LIST(APPEND LIBQUANTIS_CMAKE_ARGS "-DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/Quantis-v13.11.08/install")
LIST(APPEND LIBQUANTIS_CMAKE_ARGS "-DLIBUSB_INCLUDE_DIRECTORIES=${LIBUSB_INCLUDE_DIRECTORIES}")
LIST(APPEND LIBQUANTIS_CMAKE_ARGS "-DLIBUSB_LIBRARY=${LIBUSB_LIBRARY}")
LIST(APPEND LIBQUANTIS_CMAKE_ARGS "-DUSB1_DIR=${CMAKE_CURRENT_BINARY_DIR}/Quantis-v13.11.08/src/TARGET_libquantis-build/cmake")

ExternalProject_Add(TARGET_libquantis
                    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/Quantis-v13.11.08
                    URL ${CMAKE_CURRENT_SOURCE_DIR}/Quantis-v13.11.08-src.zip
                    PATCH_COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/templates ${CMAKE_CURRENT_BINARY_DIR}/Quantis-v13.11.08/src/TARGET_libquantis
                    CMAKE_ARGS ${LIBQUANTIS_CMAKE_ARGS}
                    INSTALL_COMMAND make install
)
ADD_DEPENDENCIES(TARGET_libquantis TARGET_libusb)


# 3) Provide all variables for the build.
SET(LIBRARY ${CMAKE_CURRENT_BINARY_DIR}/Quantis-v13.11.08/install/lib/libquantis.a)

IF("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"  AND  "${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
	SET(LDFLAGS "-pthread -lrt -ludev")
ENDIF("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"  AND  "${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")

SET(INCLUDE_DIRECTORIES ${CMAKE_CURRENT_BINARY_DIR}/Quantis-v13.11.08/install/include)

#----------------------------------------------------------------------------
#
# Store all information about the library in the cache.
#
SET(LIBQUANTIS_LIBRARY              ${LIBRARY}              CACHE INTERNAL "The libquantis library with the complete path.")
SET(LIBQUANTIS_LDFLAGS              ${LDFLAGS}              CACHE INTERNAL "All linker flags required to link the library. The list must be separated with whitespace.")
SET(LIBQUANTIS_DEFINES              ${DEFINES}              CACHE INTERNAL "All defines for the libquantis library. This should be a list of entries starting with -D.")
SET(LIBQUANTIS_INCLUDE_DIRECTORIES  ${INCLUDE_DIRECTORIES}  CACHE INTERNAL "The include folders for the library headers. The list must be separated with whitespace.")
