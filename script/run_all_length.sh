#!/usr/bin/env bash

if [[ $# -gt 2 ]]; then
    echo "$0 [<server ip>]"
fi

l=1
limit=8388608

count=1000
verb="write"
mode="throughput"
repeat=1
warmup=0
mr_count=1

server="$1"

cd ../build
while [[ $l -le $limit ]]; do
    echo "Length = $l ..."
    ./rdma_exec -b $l -c $count -v $verb -m $mode -r $repeat -w $warmup --mr_count=$mr_count $server
    (( l *= 2 ))
    echo ""
done

