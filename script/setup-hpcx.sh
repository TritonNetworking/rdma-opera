#!/usr/bin/env bash

USE_OFED_OMPI=true

load_ofed_ompi() {
    export PATH=/usr/mpi/gcc/openmpi-3.1.1rc1/bin:$PATH
}

load_hpcx_ompi() {
    CWD=$PWD
    OFED_VER=$(ofed_info -s)
    if [[ $OFED_VER = *"4.3"* ]]; then
        cd ~/infra/drivers/hpcx-v2.1.0-gcc-MLNX_OFED_LINUX-4.3-1.0.1.0-ubuntu16.04-x86_64/
        #cd ~/infra/drivers/hpcx-v2.2.0-gcc-MLNX_OFED_LINUX-4.3-1.0.1.0-ubuntu16.04-x86_64
    elif [[ $OFED_VER = *"4.4"* ]]; then
        cd ~/infra/drivers/hpcx-v2.2.0-gcc-MLNX_OFED_LINUX-4.4-1.0.0.0-ubuntu18.04-x86_64/
    fi
    export HPCX_HOME=$PWD

    source $HPCX_HOME/hpcx-init.sh
    hpcx_load
    #env | grep HPCX

    cd $CWD
}

if $USE_OFED_OMPI; then
    echo "Use OFED OMPI ..."
    load_ofed_ompi
else
    echo "Use HPC-X OMPI ..."
    load_hpcx_ompi
fi

echo "mpirun is $(which mpirun)"

