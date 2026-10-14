cmake_minimum_required(VERSION 3.15)
project(muduo_tcp_server)

add_subdirectory(src/muduo/base)
add_subdirectory(src/muduo/net)
add_subdirectory(test)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)

add_library(muduo STATIC $<TARGET_OBJECTS:muduo_base_obj> $<TARGET_OBJECTS:muduo_net_obj>)
set(INCLUDE_PATH ${PROJECT_SOURCE_DIR}/include/muduo/base ${PROJECT_SOURCE_DIR}/include/muduo/net)
target_include_directories(muduo PUBLIC ${INCLUDE_PATH})