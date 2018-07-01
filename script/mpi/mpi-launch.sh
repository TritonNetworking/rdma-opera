#!/usr/bin/env bash

# Config
execname=$HPCX_MPI_TESTS_DIR/examples/hello_c
hostfile=hosts.config
hosts=$(cat $hostfile| paste -s -d "," -)
np=$(cat $hostfile | wc -l)

# From https://community.mellanox.com/docs/DOC-3076#jive_content_id_Running_MPI
HCAS=mlx5_0:1

FLAGS+="--host $hosts "
FLAGS+="-mca btl_openib_warn_default_gid_prefix 0 "
FLAGS+="-mca btl_openib_warn_no_device_params_found 0 "
FLAGS+="--report-bindings --allow-run-as-root -bind-to core "
FLAGS+="-mca coll_fca_enable 0 -mca coll_hcoll_enable 0 "
#FLAGS+="-mca pml yalla "
FLAGS+="-mca mtl_mxm_np 0 -x MXM_TLS=ud,shm,self -x MXM_RDMA_PORTS=$HCAS "
FLAGS+="-x MXM_LOG_LEVEL=ERROR -x MXM_IB_PORTS=$HCAS "
FLAGS+="-x MXM_IB_MAP_MODE=round-robin -x MXM_IB_USE_GRH=y "

# From Max
FLAGS+="-mca btl_openib_receive_queues P,65536,256,192,128:S,128,256,192,128:S,2048,1024,1008,64:S,12288,1024,1008,64:S,65536,1024,1008,64 "

# Launch MPI job
set -x
mpirun -np $np --host $hosts $FLAGS $execname

