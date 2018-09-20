#!/usr/bin/env bash

cd "$(dirname "$0")"

source ../config

LOG_DIR=~/opera.logs
ALL_LOGS=~/Source/opera.data

# Config
USE_CLOCK=false
CLOCK_FREQ=250

# Programs
SETSTATE_EXEC=/root/set_state
CLOCK_EXEC=/root/clock
PSSH_LAUNCH=$SCRIPT_DIR/pssh-launch.sh
OPERA_LAUNCH=$SCRIPT_DIR/opera/opera-ll-traffic.sh
OPERA_PLOT=$SCRIPT_DIR/opera/plot-opera-ll-traffic.py

host_count=$(cat $SCRIPT_DIR/hosts.config | wc -l)
conn_count=$(( host_count * (host_count - 1) ))

check_executable()
{
    user=$1
    executable=$2
    echo "Checking executable \"$executable\" ..."
    if [ $user = "root" ]; then
        sudo [ -x $executable ]
        success=$?
    else
        [ -x $executable ]
        success=$?
    fi

    if [ $success -ne 0 ]; then
        >&2 echo "ERROR: \"$executable\" not found."
        abort
    fi
}

check_config()
{
    >&2 echo "Checking config ..."
    if [ ! -d $LOG_DIR ]; then
        mkdir -p $LOG_DIR
    fi

    check_executable $USER $PSSH_LAUNCH
    check_executable $USER $OPERA_LAUNCH
    check_executable $USER $OPERA_PLOT
    check_executable "root" $SETSTATE_EXEC
    if $USE_CLOCK; then
        check_executable "root" $CLOCK_EXEC
    fi

    >&2 echo "Done"
    >&2 echo
}

print_config()
{
    >&2 echo "Experiment config:"
    >&2 echo
    >&2 echo "Hosts:"
    >&2 cat $SCRIPT_DIR/hosts.config
    >&2 echo
    >&2 echo "Host count:       $host_count"
    >&2 echo "Connection count: $conn_count"
    >&2 echo "Repo dir:         $REPO_DIR"
    >&2 echo "Log dir:          $LOG_DIR"
    >&2 echo "Use clock:        $USE_CLOCK"
    >&2 echo "Clock freq:       $CLOCK_FREQ"
    >&2 echo
}

get_conn_count()
{
    echo $($PSSH_LAUNCH "ps aux | grep [i]b_send_lat" | grep "ib_send_lat" | wc -l)
}

get_active_count()
{
    echo $($PSSH_LAUNCH "ps aux | grep [i]b_send_lat" | grep "ib_send_lat" | grep " R " | wc -l)
}

launch_ll_traffic()
{
    >&2 echo "Launching LL traffic ..."
    $PSSH_LAUNCH $(realpath $OPERA_LAUNCH)
}

kill_ll_traffic()
{
    >&2 echo "Killing LL traffic ..."
    $PSSH_LAUNCH "killall ib_send_lat"
}

ps_ll_traffic()
{
    echo "$($PSSH_LAUNCH "ps aux | grep [i]b_send_lat")"
}

switch_set_l3()
{
    >&2 echo "Setting switch to L3 mode ..."
    sudo $SETSTATE_EXEC 64
}

switch_run_clock_bg()
{
    >&2 echo "Running \"$CLOCK_EXEC $CLOCK_FREQ\" in background ..."
    sudo bash -c "$CLOCK_EXEC $CLOCK_FREQ &"
    pid=$(pidof $(basename $CLOCK_EXEC))
    echo $pid
}

switch_kill_clock()
{
    pid=$1
    >&2 echo "Killing clock program (pid = $pid) ..."
    sudo kill $pid
}

my_prompt()
{
    question=$1

    >&2 echo -n "$question [yn]? "
    read ans
    if [[ $ans == "y" || $ans == "Y" ]]; then
        echo true
    else
        echo false
    fi
}

abort()
{
    >&2 echo "Experiment aborted."
    exit $1
}

plot_cdf_and_save_stats()
{
    $logdir=$1
    $OPERA_PLOT -l $logdir/raw/*.log -p cdf -o $logdir/cdf.png -s > $logdir/stats.txt
}

plot_scatter()
{
    $logdir=$1
    $OPERA_PLOT -l $logdir/raw/*.log -p scatter -o $logdir/scatter.png
}

run_experiment()
{
    d=$(date +%Y_%m_%d_%H_%M_%S)

    check_config
    print_config
    ans=$(my_prompt "Continue")
    if $ans; then
        >&2 echo "Experiment started ..."
    else
        abort
    fi

    if [ ! -z "$(ls -A $LOG_DIR)" ]; then
        ans=$(my_prompt "Log directory not empty. Remove everything and continue")
        if $ans; then
            rm $LOG_DIR/*
        else
            abort
        fi
    fi

    c=$(get_conn_count)
    if [[ $c -ne 0 ]]; then
        >&2 echo -n "LL traffic found. Kill them [yn]? " && read ans
        if [[ $ans == "y" || $ans == "Y" ]]; then
            kill_ll_traffic
        else
            abort
        fi
    fi

    switch_set_l3

    launch_ll_traffic
    c=$(get_conn_count)
    if [[ $c -ne $conn_count ]]; then
        >&2 echo "Failed to launch all LL traffic. Exiting ..."
        exit 1
    fi

    c=0
    while true; do
        c=$(get_active_count)
        >&2 echo "Currently running LL traffic: $c/$conn_count"
        if [[ $c -eq $conn_count ]]; then
            break
        fi
        sleep 1
    done

    >&2 echo "All $conn_count LL traffic have launched."

    if $USE_CLOCK; then
        >&2 echo "Sleeping for 5s ..."
        sleep 5
        clock_pid=$(switch_run_clock_bg)
    else
        while true; do
            >&2 echo -n "Continue after running alternative clock source [c] "
            read ans
            if [[ $ans == "c" ]]; then
                break
            fi
        done
    fi
    ps_ll_traffic > $LOG_DIR/ps.out

    >&2 echo "Waiting for all LL traffic to finish ..."
    while [[ $c -ne 0 ]]; do
        c=$(get_active_count)
        >&2 echo "Currently running LL traffic: $c/$conn_count"
        sleep 5
    done

    >&2 echo "All LL traffic has finished."
    ls -lh $LOG_DIR > $LOG_DIR/ll-orig.out

    if $USE_CLOCK; then
        switch_kill_clock $clock_pid
    fi

    >&2 echo "Experiment finished. Processing data ..."
    perm_log_dir="$ALL_LOGS/$d"
    mkdir $perm_log_dir
    >&2 echo "Saving data to \"$perm_log_dir\"..."
    mkdir $perm_log_dir/raw
    mv $LOG_DIR/* $perm_log_dir/raw
    plot_cdf_and_save_stats $perm_log_dir
    plot_scatter $perm_log_dir
    >&2 echo "Saved graph and stats to \"$perm_log_dir\"."
    >&2 echo "Done"
    >&2 echo
}

#set -x
set -e
run_experiment

# get_run_time()
# {
#     echo $(./pssh-launch.sh "ps aux | grep [i]b_send_lat" | grep "ib_send_lat" | awk '{ print $10 }')
# }
