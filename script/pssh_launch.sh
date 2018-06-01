#!/usr/bin/env bash

hosts=$(cat hosts.config | paste -s -d " " -)

DEFAULT_COMMAND="cd ~/dccs/script; ./pssh_node.sh"
command=${1:-$DEFAULT_COMMAND}

pssh -i -H "$hosts" $command

