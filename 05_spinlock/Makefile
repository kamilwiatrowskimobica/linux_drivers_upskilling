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

# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
	obj-m += mob_char.o
	mob_char-objs := src/mob_char.o src/mob_sync.o
# Otherwise we were called directly from the command
# line; invoke the kernel build system.
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD  := $(shell pwd)
all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules -Wall
	$(CC) src/testchar.c -o testchar
	$(CC) src/test2.c -o test2
clean: 
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
	rm -rf testchar test2 *.o *.ko *.elf
endif

depend .depend dep:
	$(CC) $(EXTRA_CFLAGS) -M *.c > .depend

ifeq (.depend,$(wildcard .depend))
include .depend
endif