#!/bin/sh
module="ram-disk"
device="woabdev"

sudo /sbin/insmod ./$module.ko $*

sudo rm -f /dev/${device}
sudo mknod /dev/${device} b 240 0
