#!/usr/bin/env bash

# Configuration
host=$(hostname -s)
hosts=$(cat hosts.config | paste -s -d "," -)
host_list=($(cat hosts.config))
host_count=${#host_list[@]}

# find the index
index=-1
for i in "${!host_list[@]}"; do
    if [[ "${host_list[$i]}" = "$host" ]]; then
        index=$i
        break
    fi
done

if [ $index -lt 0 ]; then
    echo >&2 "Host $host not included in config file, exiting ..."
    exit 2
fi

# Launch
cd ../build
echo "launch with option --host=$host --hosts=$hosts --index=$index"

