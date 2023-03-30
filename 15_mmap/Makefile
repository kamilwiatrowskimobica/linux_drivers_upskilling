obj-m = woab_kmmap.o
KDIR = /home/wa/WSL2-Linux-Kernel/

all:
	make -C $(KDIR)  M=$(shell pwd) modules
	gcc mmap-test.c

clean:
	make -C $(KDIR)  M=$(shell pwd) clean
	rm -f a.out