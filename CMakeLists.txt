cmake_minimum_required(VERSION 2.8)

PROJECT("quantisd")

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/src)


SET(quantisd_SOURCES
	src/main.c
)

add_executable(quantisd ${quantisd_SOURCES})


