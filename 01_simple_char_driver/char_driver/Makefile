obj-m+=char_drv.o

all: modules test

modules:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
	
test:
	$(CC) test.c -o test


clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
	rm -f test