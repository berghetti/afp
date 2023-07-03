#!/bin/bash

# https://wiki.archlinux.org/title/PCI_passthrough_via_OVMF#Dynamically_isolating_CPUs

# permit cpu cores to host run, others CPU are not used by host
CPUS_HOST="7"

# all cpus cores on system
CPUS_ALL="0-7"

if [ -z "$1" ]; then
  echo "Usage $0 on|off"
fi


if [ $1 = "on" ]; then
  sudo systemctl set-property --runtime -- user.slice AllowedCPUs=$CPUS_HOST
  sudo systemctl set-property --runtime -- system.slice AllowedCPUs=$CPUS_HOST
  sudo systemctl set-property --runtime -- init.scope AllowedCPUs=$CPUS_HOST
elif [ $1 = "off" ]; then
  sudo systemctl set-property --runtime -- user.slice AllowedCPUs=$CPUS_ALL
  sudo systemctl set-property --runtime -- system.slice AllowedCPUs=$CPUS_ALL
  sudo systemctl set-property --runtime -- init.scope AllowedCPUs=$CPUS_ALL
fi
