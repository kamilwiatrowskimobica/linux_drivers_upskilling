Need to copy compiled vmlinux from target to the host, say to the /boot/kgdb-image/ (this is the convention)

in directory, where vmlinux is, run "gdb ./vmlinux"

if you're set up with ttyS0, then in gdb run: 
    target remote /dev/ttyS0
