obj-m+=rzettydrv.o

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
	$(CC) rzetty_test.c -o test

clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
	rm test
