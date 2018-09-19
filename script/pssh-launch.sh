#!/usr/bin/env bash

cd "$(dirname "$0")"

hosts=$(cat hosts.config | paste -s -d " " -)
DEFAULT_COMMAND='echo "Hello from $HOSTNAME"'
command=${1:-$DEFAULT_COMMAND}

command -v pssh >/dev/null 2>&1
if [ $? -ne 0 ]; then
    pssh=parallel-ssh
else
    pssh=pssh
fi

$pssh -i -H "$hosts" $command
