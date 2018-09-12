#!/usr/bin/env bash

min_host_num=30
max_host_num=36

curr_host=$(hostname -s)
curr_host_num=${curr_host:4:2}

# Run configuration
LATENCY_GAP=1000    # in µs
ITERS=$((60 * 1000))
BLOCKSIZE=2
WAIT=false
TOS=4

COMMON_FLAG="-R -U -T $TOS -s $BLOCKSIZE -n $ITERS --latency_gap $LATENCY_GAP"

# Program settings
LOG_DIR=$HOME/opera.logs
PROGRAM=~/Source/examples/perftest/ib_send_lat
PORT_PREFIX="100"
HOST_PREFIX="b09-"

# hostname to IP mapping
declare -A host2ip
host2ip["b09-30"]="10.0.0.1"
host2ip["b09-32"]="10.0.0.2"
host2ip["b09-34"]="10.0.0.7"
host2ip["b09-36"]="10.0.0.8"
host2ip["b09-38"]="10.0.0.4"
host2ip["b09-40"]="10.0.0.6"
host2ip["b09-42"]="10.0.0.3"
host2ip["b09-44"]="10.0.0.5"

#killall $(basename $PROGRAM)

# Clean log directory
mkdir -p $LOG_DIR
rm -f "$LOG_DIR/*"

# Each host connects to every host behind it and accepts connections from every host before it.

# Launch servers
echo "Launching servers ..."
for (( num = $min_host_num; num < $curr_host_num; num += 2 ))
do
    client_num=$num
    server_num=$curr_host_num
    conn_name="$server_num-$client_num"
    port="$PORT_PREFIX$client_num"
    logfile="$LOG_DIR/$conn_name.log"
    echo "Launching server $conn_name ..."
    flags="$COMMON_FLAG -p $port"
    if $WAIT; then
        $PROGRAM $flags > $logfile &
        spids[${num}]=$!
    else
        nohup $PROGRAM $flags > $logfile &
    fi
done
echo

echo "Sleeping for 1s ..."
echo
sleep 1

# Launch clients
echo "Launching clients ..."
for (( num = $((curr_host_num + 2)); num <= $max_host_num; num += 2 ))
do
    client_num=$curr_host_num
    server_num=$num
    server_name="$HOST_PREFIX$server_num"
    server=${host2ip[$server_name]}
    conn_name="$client_num-$server_num"
    port="$PORT_PREFIX$client_num"
    logfile="$LOG_DIR/$conn_name.log"
    echo "Launching client $conn_name ..."
    flags="$COMMON_FLAG -p $port $server"
    if $WAIT; then
        $PROGRAM $flags > $logfile &
        cpids[${num}]=$!
    else
        nohup $PROGRAM $flags > $logfile &
    fi
done
echo

if $WAIT; then
    for pid in ${spids[*]}; do
        echo "Waiting for server process $pid ..."
        wait $pid
    done

    for pid in ${cpids[*]}; do
        echo "Waiting for client process $pid ..."
        wait $pid
    done
fi
