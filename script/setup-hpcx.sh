#!/usr/bin/env bash

CWD=$PWD
OFED_VER=$(ofed_info -s)
if [[ $OFED_VER = *"4.3"* ]]; then
    cd ~/infra/drivers/hpcx-v2.1.0-gcc-MLNX_OFED_LINUX-4.3-1.0.1.0-ubuntu16.04-x86_64/
elif [[ $OFED_VER = *"4.4"* ]]; then
    cd ~/infra/drivers/hpcx-v2.2.0-gcc-MLNX_OFED_LINUX-4.4-1.0.0.0-ubuntu18.04-x86_64/
fi
export HPCX_HOME=$PWD

source $HPCX_HOME/hpcx-init.sh
hpcx_load
#env | grep HPCX

cd $CWD

