cmake_minimum_required(VERSION 2.8)
project(Utility)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/ SUB_SOURCES)

# ($(SUB_SOURCES))

# include_directories(../../include/)

add_library(Utility STATIC ${SUB_SOURCES})

install(TARGETS Utility DESTINATION lib)