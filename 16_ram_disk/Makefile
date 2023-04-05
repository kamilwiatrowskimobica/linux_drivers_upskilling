obj-m = ram-disk.o
KDIR = /home/wa/WSL2-Linux-Kernel/

all:
	make -C $(KDIR)  M=$(shell pwd) modules
	gcc ram-disk-test.c

clean:
	make -C $(KDIR)  M=$(shell pwd) clean
	rm -f a.out