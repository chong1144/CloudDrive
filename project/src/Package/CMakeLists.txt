cmake_minimum_required(VERSION 2.8)
project(Package)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/ SUB_SOURCES)

# ($(SUB_SOURCES))

# include_directories(../../include/)

add_library(Package STATIC ${SUB_SOURCES})

install(TARGETS Package DESTINATION lib)