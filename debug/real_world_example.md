copied from target to the host:
	vmlinux	        		        (~/Downloads/)
	cmopiled linux sources      	(/src/usr/kernels/linux-6.1.12/)
	sources of and module itself    (~/Downloads/driver/)
		sudo ln -s /home/mtle/Downloads/driver/ /home/ml/Desktop/learning/linux_drivers_upskilling/04_char_dev_proc_test_breakpoints # gdb needs sources
target:
	sh /home/ml/Desktop/learning/linux_drivers_upskilling/gdbline hello /home/mtle/Downloads/driver/hello.ko
	# output will be copied to gdb on host (path of hello.ko need to be compiliant with the one on the host)
	echo g > /proc/sysrq-trigger	# debug mode
host:
	cd /home/mtle/Downloads/linux-6.1.12
	gdb ../vmlinux 
	(gdb) add-symbol-file /home/mtle/Downloads/driver/hello.ko 0xffffffffc0642000       -s .bss 0xffffffffc0644680       -s .data 0xffffffffc0644020       -s .exit.data 0xffffffffc06441d0       -s .exit.text 0xffffffffc0642573       -s .gnu.linkonce.this_module 0xffffffffc0644280       -s .init.data 0xffffffffc024b000       -s .note.gnu.build-id 0xffffffffc0643ba0       -s .note.gnu.property 0xffffffffc0643b70       -s .note.Linux 0xffffffffc0643bc4       -s .orc_unwind 0xffffffffc0643940       -s .orc_unwind_ip 0xffffffffc0643a90       -s .printk_index 0xffffffffc06441d8       -s .return_sites 0xffffffffc0643910       -s .rodata 0xffffffffc0643060       -s .strtab 0xffffffffc024ce70       -s .symtab 0xffffffffc024c000       -s __bug_table 0xffffffffc0644000       -s __mcount_loc 0xffffffffc0643000       -s __param 0xffffffffc0643898	# this is an output from gdbline command ran on the target
	(gdb) b ct_seq_next # set breakpoint on function within debugged module
		Breakpoint 1 at 0xffffffffc0642000: file /home/ml/Desktop/learning/linux_drivers_upskilling/04_char_dev_proc_test_breakpoints/hello.c, line 152.
	(gdb) b hello_init
	
	(gdb) target remote /dev/ttyS0
		# should return similar lines to:
			Remote debugging using /dev/ttyS0
			kgdb_breakpoint () at kernel/debug/debug_core.c:1219
				1219		wmb(); /* Sync point after breakpoint */
	(gdb) c
target:
	insmod hello.ko
	cat /dev/mtle_sequence
