#!/bin/sh
module="chardev"
device="woabchar"
mode="666"

sudo /sbin/insmod ./$module.ko $*
major=$(awk "\$2==\"$device\" {print \$1}" /proc/devices)

sudo rm -f /dev/${device}
sudo mknod /dev/${device}0 c $major 0
sudo ln -sf ${device}0 /dev/${device}
#sudo chgrp $group /dev/${device}
sudo chmod $mode  /dev/${device}
