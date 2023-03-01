# Comment/uncomment the following line to disable/enable debugging
DEBUG = y

ifeq ($(DEBUG),y)
	DEBFLAGS = -O -g -DDEBUG_MODE_PRINT 
else
	DEBFLAGS = -O2
endif

LDDINC = $(PWD)/inc

EXTRA_CFLAGS += $(DEBFLAGS)
EXTRA_CFLAGS += -I$(LDDINC)

ifneq ($(KERNELRELEASE),)
	obj-m += mobchar.o
	mobchar-objs := ./src/mobchar.o

else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD       := $(shell pwd)

endif

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules -Wall
	gcc test.c -o test

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.elf *.order *.symvers
	rm test

# depend .depend dep:
# 	$(CC) $(EXTRA_CFLAGS) -M *.c > .depend

# ifeq (.depend,$(wildcard .depend))
# include .depend
# endif