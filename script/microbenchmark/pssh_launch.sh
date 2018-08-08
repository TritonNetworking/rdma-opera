#!/usr/bin/env bash

source ./config

hostfile=$HOSTS_PATH
hosts=$(cat $hostfile | paste -s -d " " -)

DEFAULT_COMMAND="cd $SCRIPT_DIR/microbenchmark; ./pssh_node.sh"
command=${1:-$DEFAULT_COMMAND}

pssh -i -H "$hosts" $command

