cmake_minimum_required(VERSION 2.8)

ADD_SUBDIRECTORY(external)



PROJECT("quantisd")

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/src)
INCLUDE_DIRECTORIES(${LIBQUANTIS_INCLUDE_DIRECTORIES})

SET(quantisd_SOURCES
	src/main.c
)

add_executable(TARGET_quantisd ${quantisd_SOURCES})
ADD_DEPENDENCIES(TARGET_quantisd TARGET_libquantis)

