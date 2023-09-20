obj-m+=mysimplechardev.o
mysimplechardev-objs := mysimplechardev_main.o mysimplechardev_proc.o

all: module test
module:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
test:
	$(CC) -Wunused-function mysimplechardev_test.c -o test
#	$(CC) -Wall -Wextra mysimplechardev_test.c -o test
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
	rm -rf test