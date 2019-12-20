cmake_minimum_required(VERSION 2.7)
project(CloudDrive)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
# set(CMAKE_CXX_STANDARD 11)
# set(CMAKE_CXX_STANDARD_REQUIRED True)

link_directories(${CMAKE_SOURCE_DIR}/lib)
link_directories(/usr/lib64/mysql)
link_directories(/usr/include/node/openssl)

configure_file(${CMAKE_SOURCE_DIR}/include/root_dir.h.in ${CMAKE_SOURCE_DIR}/include/root_dir.h)

include_directories(/usr/include/node/openssl)
include_directories(${CMAKE_SOURCE_DIR}/include)

set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(MODULES
    Utility
    filesystem
    Control
    # Test
    Database
    Package
    # Test
    Database
)

foreach(modules ${MODULES})
    add_subdirectory(${SRC_DIR}/${modules})
    set(LIBS ${LIBS} ${modules})
endforeach(modules ${MODULES})

set(LIBS ${LIBS} crypto mysqlclient)

add_executable(Top Top.cpp)

target_link_libraries(Top ${LIBS})

