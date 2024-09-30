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