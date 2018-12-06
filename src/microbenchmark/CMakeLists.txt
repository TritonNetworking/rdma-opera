find_package(MPI REQUIRED)
include_directories(SYSTEM ${MPI_INCLUDE_PATH})

set(HEADER_FILES
        dccs_config.h
        dccs_parameters.h
        dccs_rdma.h
        dccs_utils.h
)

add_executable(rdma_exec ${HEADER_FILES} rdma_main.c)
target_link_libraries(rdma_exec m ssl crypto ibverbs rdmacm)

add_executable(control_exec ${HEADER_FILES} control_main.c)
target_link_libraries(control_exec m ssl crypto ibverbs rdmacm)

add_executable(mpi_exec ${HEADER_FILES} mpi_main.c)
target_link_libraries(mpi_exec m ssl crypto ibverbs rdmacm ${MPI_C_LIBRARIES})

