#!/usr/bin/env bash

# Check if MPI environment is loaded
if ! [ -x "$(command -v mpirun)" ]; then
    source ./setup-hpcx.sh
fi

# Config
#execname=~/Source/rotornet-mpi/rlb_v1/rotor_test
execname=../build/mpi_exec
hostfile=hosts.config
hosts=$(cat $hostfile| paste -s -d "," -)
np=$(cat $hostfile | wc -l)

# From https://community.mellanox.com/docs/DOC-3076#jive_content_id_Running_MPI
#:'
HCAS=mlx5_0:1
FLAGS+="-mca btl_openib_warn_default_gid_prefix 0 "
FLAGS+="-mca btl_openib_warn_no_device_params_found 0 "
FLAGS+="--report-bindings --allow-run-as-root -bind-to core "
FLAGS+="-mca coll_fca_enable 0 -mca coll_hcoll_enable 0 "
#FLAGS+="-mca pml yalla "
FLAGS+="-mca mtl_mxm_np 0 -x MXM_TLS=ud,shm,self -x MXM_RDMA_PORTS=$HCAS "
FLAGS+="-x MXM_LOG_LEVEL=ERROR -x MXM_IB_PORTS=$HCAS "
FLAGS+="-x MXM_IB_MAP_MODE=round-robin -x MXM_IB_USE_GRH=y "
#'

# From Max
FLAGS+="-mca btl_openib_receive_queues P,65536,256,192,128:S,128,256,192,128:S,2048,1024,1008,64:S,12288,1024,1008,64:S,65536,1024,1008,64 "

# Executable flags
l=1024
#limit=1024
limit=$((1024*1024*1024))
count=1
verb="write"
mode="throughput"
repeat=1000
warmup=0
mr_count=1
direction="1-N"

# Launch MPI job
set -x
while [[ $l -le $limit ]]; do
    echo "Length = $l ..."
    execflags="-b $l -c $count -v $verb -m $mode -c $repeat -w $warmup --mr_count=$mr_count --direction=$direction"
    mpirun -np $np --host $hosts $FLAGS $execname $execflags
    (( l *= 2 ))
    echo ""
done

