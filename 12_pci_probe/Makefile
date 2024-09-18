obj-m:=pci_skel.o

all:
	make -C $(shell pwd)/../../WSL2-Linux-Kernel M=$(shell pwd) modules

clean:
	make -C $(shell pwd)/../../WSL2-Linux-Kernel M=$(shell pwd) clean
	rm test

