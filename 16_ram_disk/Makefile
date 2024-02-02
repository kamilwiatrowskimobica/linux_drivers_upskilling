obj-m+=rzebldrv.o

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
	$(CC) $(CFLAGS) rzebl_test.c -o test

clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
	rm test
