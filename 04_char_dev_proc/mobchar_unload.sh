#!/bin/sh
module="mobchar"
device="mobchar"

# invoke rmmod with all arguments we got
sudo /sbin/rmmod $module $*

# Remove stale nodes
sudo rm -f /dev/${device} /dev/${device}[0-3]