#!/bin/sh
module="chardev"
device="woabchar"

# invoke rmmod with all arguments we got
sudo /sbin/rmmod $module $*

# Remove stale nodes
sudo rm -f /dev/${device}*
