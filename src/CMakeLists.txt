cmake_minimum_required(VERSION 3.5)
project(rdma)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -g -O0")

set(HEADER_FILES
        config.h
        dccs_parameters.h
        dccs_rdma.h
        utils.h)

add_executable(rdma_client
        ${HEADER_FILES}
        rdma_client.c)

add_executable(rdma_server
        ${HEADER_FILES}
        rdma_server.c)

target_link_libraries(rdma_client ibverbs rdmacm)
target_link_libraries(rdma_server ibverbs rdmacm)
