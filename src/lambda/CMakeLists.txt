set(HEADER_FILES
        dccs_config.h
        dccs_parameters.h
        dccs_rdma.h
        dccs_utils.h
)

add_executable(remote_lambda ${HEADER_FILES} remote_lambda.c)
target_link_libraries(remote_lambda m ssl crypto ibverbs rdmacm)

add_subdirectory(examples)

