# Add debug flag to CFLAGS if debugging enabled (DEBUG=y)
ifeq ($(DEBUG),y)
  DEBFLAGS = -O -g -DSCULL_DEBUG # "-O" is needed to expand inlines
else
  DEBFLAGS = -O2
endif

LDDINC=$(PWD)/include

EXTRA_CFLAGS += $(DEBFLAGS)
EXTRA_CFLAGS += -I$(LDDINC)


ifneq ($(KERNELRELEASE),)
# call from kernel build system

$(info	KERNELRELEASE: $(KERNELRELEASE))

	obj-m	:= scull.o

else
# call from the console

	KERNELDIR ?= /usr/src/linux-headers-$(shell uname -r)
	PWD       := $(shell pwd)

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
	$(CC) test_scull.c -o test_scull

endif


clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.mod modules.order *.symvers
	rm test_scull
	
	
depend .depend dep:
	$(CC) $(EXTRA_CFLAGS) -M *.c > .depend
	
	
# Include the dependency information
ifeq (.depend,$(wildcard .depend))
include .depend
endif