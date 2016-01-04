cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project (bounded_queue)

set(PSL_DIR ${CMAKE_SOURCE_DIR}/../../IBM/psl)
include_directories(${CMAKE_SOURCE_DIR} ${PSL_DIR}/include)
link_directories(${PSL_DIR}/build/lib)

find_package(Boost COMPONENTS program_options REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

set(RDMA_LIBS rdmacm ibverbs)

add_compile_options(-std=c++14)
add_compile_options(-Wall -Werror)

include_directories(.)

add_executable(bounded_queue main.cpp bounded_queue.cpp)
target_link_libraries(bounded_queue psl)

add_executable(bq_server server.cpp bounded_queue.cpp)
target_link_libraries(bq_server psl)
target_link_libraries(bq_server ${Boost_LIBRARIES})
target_link_libraries(bq_server ${RDMA_LIBS})

add_executable(bq_client client.cpp bounded_queue.cpp)
target_link_libraries(bq_client psl)
target_link_libraries(bq_client ${Boost_LIBRARIES})
target_link_libraries(bq_client ${RDMA_LIBS})
