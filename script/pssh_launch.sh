#!/usr/bin/env bash

hosts=$(cat hosts.config | paste -s -d " " -)

pssh -i -H "$hosts" "cd ~/dccs/script; ./pssh_node.sh"

