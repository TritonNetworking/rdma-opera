#!/bin/bash

if [[ $# -gt 2 ]]; then
    echo "$0 [<server ip>]"
fi

l=1
limit=8388608

count=1000
verb="read"
mode="latency"
server="$1"

cd ../build
while [[ $l -le $limit ]]; do
    echo "Length = $l ..."
    ./rdma_exec -b $l -r $count -v $verb -m $mode $server
    (( l *= 2 ))
    echo ""
done

