#!/bin/bash

dev=enp101s0

# brind up interface
# ifconfig $dev 10.1.100.15 netmask 255.255.255.0
# disable pause frames
ethtool -A $dev rx off tx off

# setpci -s 04:00.0 68.w=3fff

# disable mem compaction:
echo never > /sys/kernel/mm/transparent_hugepage/defrag
echo never > /sys/kernel/mm/transparent_hugepage/enabled
echo 0 > /sys/kernel/mm/transparent_hugepage/khugepaged/defrag
sysctl -w vm.swappiness=0
sysctl -w vm.zone_reclaim_mode=0

