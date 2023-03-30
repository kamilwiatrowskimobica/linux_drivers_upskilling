#!/bin/sh
module="woab_kmmap"
device="woab_map"

sudo /sbin/insmod ./$module.ko $*
major=$(awk "\$2==\"$device\" {print \$1}" /proc/devices)

sudo rm -f /dev/${device}
sudo mknod /dev/${device} c $major 0
