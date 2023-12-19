obj-m+=pciprobe.o

KDIR = /lib/modules/$(shell uname -r)/build/
EXTRA_CFLAGS = -Wall -g

all:
	make -C '$(KDIR)' M=$(PWD) modules

clean:
	make -C '$(KDIR)' M=$(PWD) clean
	rm test
