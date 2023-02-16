#!/bin/bash
#
# BASED on 
# https://wiki.linuxquestions.org/wiki/How_to_build_and_install_your_own_Linux_kernel
printf "Most of this SHOULD be run as a non-root user!\n\
        This are just notes!\n\
        This code was never tested.\n\
        However, the commands here were executed in the same order\n
        These might work on Fedora 37\n\n"

COPIED_LINUX_VERSION="linux-6.1.12"
MTLE_APPENDIX=".MTLE" # to distinguish our files

HOME="home/ml/"
ROOT=${HOME}/${COPIED_LINUX_VERSION}

cd ${HOME}
wget https://cdn.kernel.org/pub/linux/kernel/v6.x/${COPIED_LINUX_VERSION}.tar.xz
tar -x ${COPIED_LINUX_VERSION}
cd ${ROOT}

#git clone git://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git

sudo yum groupinstall "Development Tools"
sudo yum install ncurses-devel bison flex elfutils-libelf-devel openssl-devel openssl.x86_64

make menuconfig
# CONFIG_GDB_SCRIPTS=y
# CONFIG_KGDB=y
# CONFIG_KGDB_SERIAL_CONSOLE=y
# CONFIG_KGDB_KDB=y
# CONFIG_KDB_DEFAULT_ENABLE=0x1 to allow most KGDB functions


#THIS NEEDS TO BE RUN AS A NORMAL USER!!!
# if returns 137 error (out of mem) on VM, just increase RAM
make -j

#THIS NEEDS TO BE RUN AS A SUPER USER!!!
sudo make -j modules_install

# need to make this kernel bootable now
# this may overwrite your previous settings
sudo make install
# 2nd method is to install new(ly compiled) kernel, copy the bzImage file and the system map file by hand to the /boot directory of your system, using:
#sudo cp arch/x86/boot/bzImage  /boot/bzImage-${COPIED_LINUX_VERSION}${MTLE_APENDIX} #(replace x86 if necessary by your actual CPU architecture)
#sudo cp System.map /boot/System.map-${COPIED_LINUX_VERSION}${MTLE_APENDIX}
# copy the final .config file to /boot (for storage and future reference) :
#sudo cp .config /boot/config-${COPIED_LINUX_VERSION}${MTLE_APENDIX}



# vmlinux can be built by:
#make -j vmlinux



