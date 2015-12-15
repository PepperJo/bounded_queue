cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project (bounded_queue)

set(PSL_DIR ${CMAKE_SOURCE_DIR}/../psl)
include_directories(${CMAKE_SOURCE_DIR} ${PSL_DIR}/include)
link_directories(${PSL_DIR}/build/lib)

add_compile_options(-std=c++14)
add_compile_options(-Wall -Werror)

include_directories(.)

add_executable(bounded_queue main.cpp bounded_queue.cpp)
target_link_libraries(bounded_queue psl)

