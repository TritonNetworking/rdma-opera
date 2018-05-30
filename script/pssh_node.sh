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

next=$(echo "($index + 1) % $host_count" | bc)

# Launch
cd ../build
echo "launch with option --host=$host --hosts=$hosts --index=$index --next=$next"

#echo "test" > "${host_list[$index]}_to_${host_list[$ndext]}.server.out"
ib_write_bw -F -R -D 20s --report_gbits > "${host_list[$index]}_to_${host_list[$next]}.server.out" &
sleep 1
ib_write_bw -F -R -D 20s --report_gbits "${host_list[$next]}" | tee "${host_list[$index]}_to_${host_list[$next]}.client.out" &

