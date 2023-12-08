obj-m+=rzechardrv.o

KDIR = /home/rfzi/Documents/other_projets/linux_drivers_development/linux-kernel_labs-master
EXTRA_CFLAGS = -Wall -g

all:
	make -C '$(KDIR)' M=$(PWD) modules

clean:
	make -C '$(KDIR)' M=$(PWD) clean
	rm test
