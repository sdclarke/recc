cmake_minimum_required(VERSION 3.6)
project(reccdevshim C)

add_library(reccdevshim SHARED reccdevshim.c)
install(TARGETS reccdevshim LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
