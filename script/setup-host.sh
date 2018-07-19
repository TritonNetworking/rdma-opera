#!/usr/bin/env bash

# Set up end host for HPC environment

# Set CPU frequency to max
echo "Setting CPU governor to performance ..."
if ! dpkg -s cpufrequtils 2>&1 | grep -q 'installed$'; then
    echo "Package cpufrequtils not installed. Installing ..."
    sudo apt-get install -y cpufrequtils
fi
echo 'GOVERNOR="performance"' | sudo tee /etc/default/cpufrequtils > /dev/null
sudo systemctl disable ondemand
echo "Done"

# Disable Hyper-Threading
echo "Disabling Hyper-Threading ..."
CPU_CORE_COUNT=20
from=$((CPU_CORE_COUNT))
to=$((2 * CPU_CORE_COUNT - 1))
for i in $(seq $from $to); do
    echo 0 | sudo tee --append /sys/devices/system/cpu/cpu$i/online > /dev/null
    cat "/sys/devices/system/cpu/cpu$i/online"
done

# Restart NIC driver to update interrupt handler mapping
echo "Restarting NIC driver to update IRQ mapping ..."
sudo /etc/init.d/openibd restart
irq_count=$(cat /proc/interrupts | grep enp101s0- | wc -l)
if [[ $irq_count -ne $CPU_CORE_COUNT ]]; then
    echo "[Warning] NIC IRQ count not equal to CPU count."
fi
echo "Done"

# Disable daemon processes
echo "Disabling daemon processes ..."
sudo service cron stop
sudo service atd stop
sudo service iscdis stop
sudo service irqbalance stop
echo "Done"

