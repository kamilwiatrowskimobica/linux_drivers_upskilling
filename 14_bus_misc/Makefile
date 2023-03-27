obj-m = woab_bus.o woab_misc.o
KDIR = /home/wa/WSL2-Linux-Kernel/

all:
	make -C $(KDIR)  M=$(shell pwd) modules

clean:
	make -C $(KDIR)  M=$(shell pwd) clean