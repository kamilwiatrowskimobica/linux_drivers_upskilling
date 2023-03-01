#!/bin/sh
module="mobchar"
device="mobchar"
mode="666"

group="staff"
grep -q '^staff:' /etc/group || group="wheel"

sudo /sbin/insmod ./$module.ko $*
# retrieve major number
major=$(awk "\$2==\"$module\" {print \$1}" /proc/devices)

sudo rm -f /dev/${device}
sudo mknod /dev/${device}0 c $major 0
sudo ln -sf ${device}0 /dev/${device}
sudo chgrp $group /dev/${device}
sudo chmod $mode  /dev/${device}
