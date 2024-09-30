obj-m+=CharDevEx.o

all:
	make -C /usr/src/linux-headers-$(shell uname -r) M=$(shell pwd) modules
	$(CC) testCharDevEx.c -o test

clean:
	make -C /usr/src/linux-headers-$(shell uname -r) M=$(shell pwd) clean
	rm test
