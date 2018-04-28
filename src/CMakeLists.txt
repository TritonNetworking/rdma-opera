cmake_minimum_required(VERSION 3.5)
project(rdma)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wpedantic -Wconversion")
set(default_build_type "Release")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall -g -O0")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wall -O3")

set(HEADER_FILES
        config.h
        dccs_parameters.h
        dccs_rdma.h
        utils.h)

add_executable(rdma_exec
        ${HEADER_FILES}
        rdma_main.c)

target_link_libraries(rdma_exec ssl crypto ibverbs rdmacm)

