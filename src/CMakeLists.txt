cmake_minimum_required(VERSION 3.5)
project(rdma LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wpedantic -Wconversion")
set(default_build_type "Release")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -O3")

set(HEADER_FILES
        dccs_config.h
        dccs_parameters.h
        dccs_rdma.h
        dccs_utils.h
)
set_source_files_properties(rdma_main.c PROPERTIES LANGUAGE CXX)
add_executable(rdma_exec
        ${HEADER_FILES}
        rdma_main.c)

target_link_libraries(rdma_exec m ssl crypto ibverbs rdmacm)

