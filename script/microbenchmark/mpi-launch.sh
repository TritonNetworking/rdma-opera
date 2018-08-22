#!/usr/bin/env bash

# Check if MPI environment is loaded
if ! [ -x "$(command -v mpirun)" ]; then
    source ../setup-hpcx.sh
fi

source ./config

# Default command line argument
YALLA=false
UCX=false
PROGRAM="benchmark"

# Parse command line argument
while [[ $# -gt 0 ]]
do
key="$1"

case $key in
    -y|--yalla)
        YALLA=true
        shift # past argument
        ;;
    -u|--ucx)
        UCX=true
        shift # past argument
        ;;
    -m|--mode)
        MODE="$2"
        shift # past argument
        shift # past value
        ;;
    -p|--program)
        PROGRAM="$2"
        if [[ $PROGRAM != "roter" && $PROGRAM != "benchmark" && $PROGRAM != "osu" ]]; then
            >&2 echo "Invalid program \"$PROGRAM\""
            exit 2
        fi
        shift # past argument
        shift # past value
        ;;
    *)
        >&2 echo "Unrecognized option $key"
        exit 2
esac
done

# Config
hostfile=$HOSTS_PATH
hosts=$(cat $hostfile| paste -s -d "," -)
np=$(cat $hostfile | wc -l)

# Machine-dependent variables
hcaid=$(ibv_devinfo | grep hca_id | awk '{ print $2 }')

# From https://community.mellanox.com/docs/DOC-3076#jive_content_id_Running_MPI
: '
HCAS="$hcaid:1"
FLAGS+="-mca btl_openib_warn_default_gid_prefix 0 "
FLAGS+="-mca btl_openib_warn_no_device_params_found 0 "
FLAGS+="--report-bindings --allow-run-as-root -bind-to core "
FLAGS+="-mca coll_fca_enable 0 -mca coll_hcoll_enable 0 "
if $YALLA; then
    FLAGS+="-mca pml yalla "
fi
if $UCX; then
    FLAGS+="-mca pml ucx -mca osc ucx "
fi
FLAGS+="-mca mtl_mxm_np 0 -x MXM_TLS=ud,shm,self -x MXM_RDMA_PORTS=$HCAS "
FLAGS+="-x MXM_LOG_LEVEL=ERROR -x MXM_IB_PORTS=$HCAS "
FLAGS+="-x MXM_IB_MAP_MODE=round-robin -x MXM_IB_USE_GRH=y "
#'

# From Max
#FLAGS+="-mca btl_openib_receive_queues P,65536,256,192,128:S,128,256,192,128:S,2048,1024,1008,64:S,12288,1024,1008,64:S,65536,1024,1008,64 "

# Mellanox recommended flags
FLAGS+="--map-by node "
FLAGS+="-mca pml yalla "
FLAGS+="-mca coll_hcoll_enable 0 "

run_roter_test() {
    execname=~/Source/rotornet-mpi/rlb_v1/rotor_test
    mpirun -np $np --host $hosts $FLAGS $execname
}

run_microbenchmark() {
    execname=$BENCH_EXEC_DIR/mpi_exec

    # Executable flags
    l=1
    #limit=1024
    limit=$((1*1024*1024))
    #limit=$((1024*1024*1024))

    if [[ $MODE = latency ]]; then
        count=1
        repeat=1000
    elif [[ $MODE = throughput ]]; then
        count=1000
        repeat=10
    else
        >&2 echo "Invalid mode \"$MODE\""
        exit 2
    fi

    direction="1-N"
    warmup=0
    mr_count=1

    set -x
    while [[ $l -le $limit ]]; do
        echo "Length = $l ..."
        execflags="-b $l -c $count -r $repeat -m $MODE -w $warmup --mr_count=$mr_count --direction=$direction"
        mpirun -np $np --host $hosts $FLAGS $execname $execflags
        (( l *= 2 ))
        echo ""
    done
}

run_osu_benchmark() {
    execname=/home/yig004/opt/libexec/osu-micro-benchmarks/mpi/pt2pt/osu_bw
    mpirun -np $np --host $hosts $FLAGS $execname
}

# Launch MPI job
set -e

case $PROGRAM in
    roter)
        run_roter_test
        ;;
    benchmark)
        run_microbenchmark
        ;;
    osu)
        run_osu_benchmark
        ;;
    *)
        >&2 echo "Program not specified ..."
        exit 2
esac

