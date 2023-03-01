#!/bin/bash
sh mobchar_unload.sh
make clean
make
sh mobchar_load.sh
./a.out
sh mobchar_unload.sh
sudo tail -n 20 -f /var/log/kern.log
# sudo dmesg
