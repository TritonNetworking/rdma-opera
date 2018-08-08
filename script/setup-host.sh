#!/usr/bin/env bash

# This script is intended to set up end host for HPC environment

# Machine configuraiton
CPU_CORE_COUNT=20
NIC_IF=enp101s0
TARGET_MTU=9000

# Set CPU frequency to max
echo "Setting CPU governor to performance ..."
if ! dpkg -s cpufrequtils 2>&1 | grep -q 'installed$'; then
    echo "Package cpufrequtils not installed. Installing ..."
    sudo apt-get install -y cpufrequtils
fi
echo 'GOVERNOR="performance"' | sudo tee /etc/default/cpufrequtils > /dev/null
sudo systemctl disable ondemand
echo "Done"
echo

# Disable Hyper-Threading
echo "Disabling Hyper-Threading ..."
from=$((CPU_CORE_COUNT))
to=$((2 * CPU_CORE_COUNT - 1))
for i in $(seq $from $to); do
    echo 0 | sudo tee --append /sys/devices/system/cpu/cpu$i/online > /dev/null
    cat "/sys/devices/system/cpu/cpu$i/online"
done
max_cpu=$((CPU_CORE_COUNT - 1))
if ! lscpu | grep -q "On-line CPU(s) list:  0-$max"; then
    echo "[Warning] HT not disabled correctly."
fi
echo "Done"
echo

# Restart NIC driver to update interrupt handler mapping
echo "Restarting NIC driver to update IRQ mapping ..."
sudo /etc/init.d/openibd restart
irq_count=$(cat /proc/interrupts | grep enp101s0- | wc -l)
if [[ $irq_count -ne $CPU_CORE_COUNT ]]; then
    echo "[Warning] NIC IRQ count not equal to CPU count."
fi
echo "Done"
echo

# Set MTU on NIC interface
echo "Setting MTU on $NIC_IF to $TARGET_MTU ..."
sudo ip link set dev $NIC_IF mtu $TARGET_MTU
if ! ip link show $NIC_IF | grep -q "mtu $TARGET_MTU"; then
    echo "[Warning] MTU not set correctly."
fi
echo "Done"
echo

# Disable daemon processes
echo "Disabling daemon processes ..."
sudo service cron stop
sudo service atd stop
sudo service iscdis stop
sudo service irqbalance stop
echo "Done"
echo

