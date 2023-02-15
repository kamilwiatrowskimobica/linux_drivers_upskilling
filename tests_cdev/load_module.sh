#!/bin/bash
#
# This is to load module and test whether it is currently loaded

set -e


#printf "Help:\n\t$0 name_of_module_loaded\nThere is no need for .ko extension\nRun with su priviledges\n\n"

module_name=$1
if [[ $1 =~ ".ko" ]]; then
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

ADDRESS=$(sudo grep hello /proc/modules |awk '{print $6}')
objdump -dS --adjust-vma=$(ADDRESS) hello.ko

modinfo ${module_file}

insmod ${module_file}

#test if module is loaded
if [ $(lsmod|grep -c ${module_name} ) ] ; then
        printf "SUCCESS\n"
else
        printf "FAIL\n"
fi

echo "/PROC/MODULES"
cat /proc/modules | grep ${module_name}

echo CHECKING ${PARAM_DIR} if module does accept any params
if [[ -e ${PARAM_DIR} ]]; then
        ls -l ${PARAM_DIR}
else
        printf "${PARAM_DIR} does not exist\n"
fi

echo "UDEVADM INFO - usefull for creating /etc/udev/rules.d"
udevadm info -a -p /sys/class/hellodrv/hellodrvchar

if [[ -e /etc/udev/rules.d/99-hellodrv.rules ]]; then
        echo "UDEV RULES EXIST"
else
        echo "CREATING UDEV RULES"
        touch /etc/udev/rules.d/99-hellodrv.rules
        echo "KERNEL==\"hellodrvchar\", SUBSYSTEM==\"hellodrv\", MODE=\"0666\"" >> /etc/udev/rules.d/99-hellodrv.rules
fi

echo "STRACE TEST"
strace ./test

rmmod ${module_file}




