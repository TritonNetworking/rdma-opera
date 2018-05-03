#!/bin/bash

if [[ $# -gt 2 ]]; then
    echo "$0 [<server ip>]"
fi

l=1
limit=8388608

count=1000
verb="read"
server="$1"

while [[ $l -le $limit ]]; do
    echo "Length = $l ..."
    ../build/rdma_exec -b $l -r $count -v $verb $server
    (( l *= 2 ))
    echo ""
done

