obj-m:=chardev.o

all:
	make -C /home/wa/WSL2-Linux-Kernel M=$(shell pwd) modules
	$(CC) testchardev.c -o test

clean:
	make -C /home/wa/WSL2-Linux-Kernel M=$(shell pwd) clean
	rm test
