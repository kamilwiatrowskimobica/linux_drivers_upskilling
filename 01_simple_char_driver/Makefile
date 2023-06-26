obj-m+=mysimplechardev.o

all: module test
module:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
test:
	$(CC) mysimplechardev_test.c -o test
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
	rm -rf test