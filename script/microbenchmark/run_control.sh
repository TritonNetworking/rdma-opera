#!/usr/bin/env bash

if [[ $# -gt 2 ]]; then
    echo "$0 [<server ip>]"
fi

source ./config

count=1
length=6
mode="latency"
verb="write"
repeat=1000
warmup=0
mr_count=1
tos=32

server="$1"
execpath=$CONTROL_SIGNAL_EXECPATH

set -x
$execpath -b $length -c $count -v $verb -m $mode -r $repeat -w $warmup --mr_count=$mr_count --tos=$tos $server

