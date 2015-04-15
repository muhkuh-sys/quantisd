cmake_minimum_required(VERSION 2.8)

SET(CMAKE_INSTALL_LIB_DIR "lib")
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/cmake/usb1-config.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/cmake/usb1-config.cmake)

ADD_SUBDIRECTORY(Libs-Apps/Quantis)
