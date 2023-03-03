#!/bin/bash
#
# This is to load module and test whether it is currently loaded

set -e

module_name="hello"
device="hellodrvchar"
mode="664"


#printf "Help:\n\t$0 [options_to_load_module]\nRun with su priviledges\n\n"

if [[ ${module_name} =~ ".ko" ]]; then
        module_file="${module_name}"
else
        module_file="${module_name}.ko"
fi
PARAM_DIR=" /sys/module/${module_name}/parameters"

#WHO="$(whoami)"
#if [[ $WHO != "root" ]]; then
#        printf "run this script with su priviledges!\n"
#        exit
#fi

#ADDRESS=$(sudo grep hello /proc/modules |awk '{print $6}')
ADDRESS=`sudo grep hello /proc/modules |awk '{print $6}'`
echo "ADDRESS=${ADDRESS} (taken from /proc/modules)"
echo "OBJDUMP"
objdump -dS --adjust-vma=${ADDRESS} hello.ko

modinfo ${module_file}


#reload module
if [[ $(lsmod|grep -c ${module_name} ) == 1 ]] ; then
	rmmod ${module_name}
	rm -fvI /dev/${device}[0-3]
fi
insmod ${module_file} $* || exit 1

#test if module is loaded
if [[ $(lsmod|grep -c ${module_name} ) == 1 ]] ; then
        printf "SUCCESS\n"
else
        printf "FAIL - no module of that name\n"
fi


echo "/PROC/DEVICES allows retrieving major number of a device"
cat /proc/devices|grep ${device}
# retrieve major number (returns multiple, if more than one)
major=$(awk "\$2==\"${device}\" {print \$1}" /proc/devices)
# retrieve major number (returns random, not necessarily the last assigned)
#major=$(awk "\$2==\"${device}\" {print \$1; exit}" /proc/devices)
# create a char device with og+rw permissions
mknod -m og+rw /dev/${device}0 c ${major} 0
##mknod /dev/${device}0 c ${major} 0
##mknod /dev/${device}1 c ${major} 1
##mknod /dev/${device}2 c ${major} 2
##mknod /dev/${device}3 c ${major} 3
ln -sf /dev/${device}0 /dev/${device}
##chgrp $group /dev/${device}[0-3]
##chmod $mode  /dev/${device}[0-3]

echo "/PROC/MODULES"
cat /proc/modules | grep ${module_name}

echo CHECKING ${PARAM_DIR} if module does accept any params
if [[ -e ${PARAM_DIR} ]]; then
        ls -l ${PARAM_DIR}
else
        printf "${PARAM_DIR} does not exist\n"
fi

#echo "UDEVADM INFO - usefull for creating /etc/udev/rules.d"
#udevadm info -a -p /sys/class/hellodrv/hellodrvchar
#
#if [[ -e /etc/udev/rules.d/99-hellodrv.rules ]]; then
#        echo "UDEV RULES EXIST"
#else
#        echo "CREATING UDEV RULES"
#        touch /etc/udev/rules.d/99-hellodrv.rules
#        echo "KERNEL==\"hellodrvchar\", SUBSYSTEM==\"hellodrv\", MODE=\"0666\"" >> /etc/udev/rules.d/99-hellodrv.rules
#fi

echo "STRACE TEST"
strace ./test

#rmmod ${module_file}




