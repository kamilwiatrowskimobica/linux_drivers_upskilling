#!/bin/sh
module="ram-disk" 
device="woabdev"

#invoke rmmod with all arguments we got
sudo /sbin/rmmod $module $*

#Remove stale nodes
sudo rm -f /dev/${device}*
