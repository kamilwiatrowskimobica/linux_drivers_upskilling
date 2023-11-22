obj-m+=ttydriver.o
CFLAGS = -pthread

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
	$(CC) $(CFLAGS) test.c -o test

clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
	rm test