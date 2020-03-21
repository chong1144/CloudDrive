cmake_minimum_required(VERSION 2.8)
project(Upload)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/ SUB_SOURCES)

# ($(SUB_SOURCES))

# include_directories(../../include/)

set(LIBS filesystem)
add_library(Upload STATIC ${SUB_SOURCES})
target_link_libraries(Upload PUBLIC ${LIBS})

install(TARGETS Upload DESTINATION lib)