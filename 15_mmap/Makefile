CONFIG_MODULE_SIG=n
DEBUG = y

ifeq ($(DEBUG),y)
	DEBFLAGS = -O -g -DMOB_DEBUG -std=gnu17
else
	DEBFLAGS = -O2 -std=gnu17
endif

LDDINC=$(PWD)/inc
EXTRA_CFLAGS += $(DEBFLAGS)
EXTRA_CFLAGS += -I$(LDDINC)

ifneq ($(KERNELRELEASE),)
	obj-m += mob_kmmap.o
	mob_kmmap-objs := kmap/mob_kmmap.o
else
	KERNELDIR ?= /home/$(shell whoami)/WSL2-Linux-Kernel
	PWD  := $(shell pwd)

all:
	make -C $(KERNELDIR) M=$(PWD) modules 
	$(CC) mmap-test.c -o mmap-test

clean:
	make -C $(KERNELDIR)  M=$(shell pwd) clean
	rm -rf mmap-test *.o *.ko *.elf
endif

depend .depend dep:
	$(CC) $(EXTRA_CFLAGS) -M *.c > .depend

ifeq (.depend,$(wildcard .depend))
include .depend
endif

