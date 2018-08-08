#!/usr/bin/env bash

source ./config

# Configuration
host=$(hostname -s)
hostfile=$HOSTS_PATH
hosts=$(cat $hostfile | paste -s -d "," -)
host_list=($(cat $HOSTS_PATH))
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

curr_index=$index
curr_host="${host_list[$curr_index]}"
next_index=$(echo "($curr_index + 1) % $host_count" | bc)
next_host="${host_list[$next_index]}"

# Launch
cd $BUILD_DIR
echo "launch with option --hosts=$hosts --host=$curr_host --index=$curr_index --next-index=$next_index --next-host=$next_host"

client=$curr_host
server=$next_host
client_log="${client}_to_${server}.client.out"
server_log="${client}_to_${server}.server.out"

# udaddy test
: '
udaddy > $server_log &
pid=$!
udaddy -s $next_host > $client_log
wait $pid
#'

# rdma_server test
: '
rdma_server > $server_log &
pid=$!
rdma_client -s $next_host > $client_log
wait $pid
#'

# Mallenox benchmark tool

: '
duration="5s"
ib_flags="-F -R -D $duration --report_gbits"

echo "client_log=$client_log, server_log=$server_log, duration=$duration, ib_flags=$ib_flags"
#'

# ib_read_bw test
: '
ib_read_bw $ib_flags > $server_log &
pid=$!
sleep 1
ib_read_bw $ib_flags $server | tee $client_log
wait $pid
#'

# ib_write_bw test
: '
ib_write_bw $ib_flags > $server_log &
pid=$!
sleep 1
ib_write_bw $ib_flags $server | tee $client_log
wait $pid
#'

# My benchmark tool
#: '
block_size=65536
count=1000
verb="write"
mode="throughput"
warmup=0
mr_count=1

cd $BENCH_EXEC_DIR
#'


# Bi-directional
: '
cd $BUILD_DIR
./rdma_exec -b $block_size -r $count -v $verb -m $mode -w $warmup --mr_count=$mr_count > $server_log 2>&1 &
pid=$!
sleep 1
./rdma_exec -b $block_size -r $count -v $verb -m $mode -w $warmup --mr_count=$mr_count $server > $client_log 2>&1 &
wait $pid
#'

# 1-to-N
# First node sends to the rest
: '
base_port=5000
if [[ $curr_index -eq 0 ]]; then
    sleep 1
    for i in "${!host_list[@]}"; do
        if [[ $i -eq 0 ]]; then
            continue
        fi

        client="${host_list[0]}"
        server="${host_list[$i]}"
        port=$(echo "$base_port + $i" | bc)
        client_log="${client}_to_${server}.$port.client.out"

        echo "Launching client: ./rdma_exec -b $block_size -r $count -v $verb -m $mode -w $warmup --mr_count=$mr_count -p $port $server > $client_log 2>&1 &"
        ./rdma_exec -b $block_size -r $count -v $verb -m $mode -w $warmup --mr_count=$mr_count -p $port $server > $client_log 2>&1 &
        pids[$i]=$!
    done

    for pid in ${pids[*]}; do
        wait $pid
    done
else
    client=${host_list[0]}
    server=${host_list[$curr_index]}
    port=$(echo "$base_port + $curr_index" | bc)
    server_log="${client}_to_${server}.$port.server.out"

    echo "Launching server: ./rdma_exec -b $block_size -r $count -v $verb -m $mode -w $warmup --mr_count=$mr_count -p $port > $server_log 2>&1"
    ./rdma_exec -b $block_size -r $count -v $verb -m $mode -w $warmup --mr_count=$mr_count -p $port > $server_log 2>&1
fi
#'

# N-to-1
# First node receive from the rest
: '
base_port=5000
if [[ $curr_index -eq 0 ]]; then
    for i in "${!host_list[@]}"; do
        if [[ $i -eq 0 ]]; then
            continue
        fi

        client="${host_list[$i]}"
        server="${host_list[0]}"
        port=$(echo "$base_port + $i" | bc)
        server_log="${client}_to_${server}.$port.server.out"

        echo "Launching server: ./rdma_exec -b $block_size -r $count -v $verb -m $mode -w $warmup --mr_count=$mr_count -p $port > $server_log 2>&1 &"
        ./rdma_exec -b $block_size -r $count -v $verb -m $mode -w $warmup --mr_count=$mr_count -p $port > $server_log 2>&1 &
        pids[$i]=$!
    done

    for pid in ${pids[*]}; do
        wait $pid
    done
else
    sleep 1

    client=${host_list[$curr_index]}
    server=${host_list[0]}
    port=$(echo "$base_port + $curr_index" | bc)
    client_log="${client}_to_${server}.$port.client.out"

    echo "Launching client: ./rdma_exec -b $block_size -r $count -v $verb -m $mode -w $warmup --mr_count=$mr_count -p $port $server > $client_log 2>&1"
    ./rdma_exec -b $block_size -r $count -v $verb -m $mode -w $warmup --mr_count=$mr_count -p $port $server > $client_log 2>&1
fi
#'

# N-to-N
# All to all traffic
: '
base_port=5000

# Launch servers
for i in "${!host_list[@]}"; do
    if [[ $curr_index -eq $i ]]; then
        continue
    fi

    client="${host_list[$i]}"
    server="${host_list[$curr_index]}"
    port=$(echo "$base_port + $i" | bc)
    server_log="${client}_to_${server}.$port.server.out"

    echo "Launching server: ./rdma_exec -b $block_size -r $count -v $verb -m $mode -w $warmup --mr_count=$mr_count -p $port > $server_log 2>&1 &"
    ./rdma_exec -b $block_size -r $count -v $verb -m $mode -w $warmup --mr_count=$mr_count -p $port > $server_log 2>&1 &
    spids[$i]=$!
done

sleep 1

# Launch clients
for i in "${!host_list[@]}"; do
    if [[ $curr_index -eq $i ]]; then
        continue
    fi

    client="${host_list[$curr_index]}"
    server="${host_list[$i]}"
    port=$(echo "$base_port + $curr_index" | bc)
    client_log="${client}_to_${server}.$port.client.out"

    echo "Launching client: ./rdma_exec -b $block_size -r $count -v $verb -m $mode -w $warmup --mr_count=$mr_count -p $port $server > $client_log 2>&1 &"
    ./rdma_exec -b $block_size -r $count -v $verb -m $mode -w $warmup --mr_count=$mr_count -p $port $server > $client_log 2>&1 &
    cpids[$i]=$!
done

# Wait for all clients and servers
for pid in ${cpids[*]}; do
    wait $pid
done
for pid in ${spids[*]}; do
    wait $pid
done
#'

