#!/usr/bin/env bash

if [[ $# -gt 2 ]]; then
    echo "$0 [<server ip>]"
fi

source ./config

l=1
limit=8388608

count=1000
verb="read"
mode="throughput"
repeat=1
warmup=0
mr_count=1

server="$1"
execpath=$RDMA_BENCH_EXECPATH

while [[ $l -le $limit ]]; do
    echo "Length = $l ..."
    $execpath -b $l -c $count -v $verb -m $mode -r $repeat -w $warmup --mr_count=$mr_count $server
    (( l *= 2 ))
    echo ""
done

