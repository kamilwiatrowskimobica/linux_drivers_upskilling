obj-m+=mem_char_drv.o

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
	 $(CC) mcd_test.c -o test
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean