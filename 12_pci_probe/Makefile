obj-m	:= pci_skel.o

KERNELDIR ?= /home/wa/WSL2-Linux-Kernel/
PWD       := $(shell pwd)

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD)

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.mod modules.order *.symvers built-in.a

