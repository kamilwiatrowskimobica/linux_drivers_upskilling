#!/bin/sh
module="woab_kmmap" 
device="woab_map"

#invoke rmmod with all arguments we got
sudo /sbin/rmmod $module $*

#Remove stale nodes
sudo rm -f /dev/${device}*
