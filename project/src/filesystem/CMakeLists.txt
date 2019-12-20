cmake_minimum_required(VERSION 2.8)
project(filesystem)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/ SUB_SOURCES)

# ($(SUB_SOURCES))

add_library(filesystem STATIC ${SUB_SOURCES})
