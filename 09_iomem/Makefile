obj-m += mk_mob.o

KDIR = /lib/modules/$(shell uname -r)/build

all:
	make -C $(KDIR) M=$(shell pwd) modules
	$(CC) test.c -o test

clean:
	make -C $(KDIR) M=$(shell pwd) clean