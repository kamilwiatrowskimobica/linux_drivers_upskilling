#!/bin/sh
module="chardev"
device="woabchar"

sudo /sbin/insmod ./$module.ko $*
major=$(awk "\$2==\"$device\" {print \$1}" /proc/devices)

sudo rm -f /dev/${device}*
sudo mknod /dev/${device}0 c $major 0
sudo mknod /dev/${device}1 c $major 1
sudo mknod /dev/${device}2 c $major 2
sudo mknod /dev/${device}3 c $major 3