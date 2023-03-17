DEBUG = y
ifeq ($(DEBUG),y)
EXTRA_CFLAGS += -O -g -DDEBUG
endif

obj-m+=pci_skel.o

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
