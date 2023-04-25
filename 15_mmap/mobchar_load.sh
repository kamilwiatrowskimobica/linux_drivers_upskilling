#!/bin/sh

module="mob_kmmap"
device="mob_map"

mode="666"
# Group: since distributions do it differently, look for wheel or use staff
if grep -q '^staff:' /etc/group; then
    group="staff"
else
    group="wheel"
fi

# invoke insmod with all arguments we got
# and use a pathname, as insmod doesn't look in . by default

sudo /sbin/insmod ./$module.ko $*
# retrieve major number
major=$(awk "\$2==\"$device\" {print \$1}" /proc/devices)
echo "major='$major'"

# Remove stale nodes and replace them, then give gid and perms
# Usually the script is shorter, it's scull that has several devices in it.

sudo rm -f /dev/${device}*
sudo mknod /dev/${device} c $major 0
##sudo ln -sf ${device}0 /dev/${device}