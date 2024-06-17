obj-m:=mk_mob.o

all:
	make -C $(shell pwd)/../../WSL2-Linux-Kernel M=$(shell pwd) modules
	$(CC) test_read.c -o test_read
	$(CC) test_write.c -o test_write

clean:
	make -C $(shell pwd)/../../WSL2-Linux-Kernel M=$(shell pwd) clean
	rm test

