sudo apt install make gcc file bzip2 build-essential wget cpio python unzip rsync bc

git clone https://github.com/buildroot/buildroot
cd buildroot
git checkout 2017.02
make BR2_EXTERNAL="$(pwd)/../kernel_module" qemu_x86_64_defconfig
echo '
BR2_PACKAGE_KERNEL_MODULE=y
BR2_TARGET_ROOTFS_EXT2_EXTRA_BLOCKS=1024
BR2_ROOTFS+OVERLAY="overlay"
' >> .config
make olddefconfig
make BR2_JLEVEL="$(nproc)" all

qemu-system-x86_64 -M pc -kernel output/images/bzImage -drive file=output/images/rootfs.ext2,if=virtio,format=raw -append 'root=/dev/vda console=ttyS0' -net nic,model=virtio -nographic -serial mon:stdio -net user
