obj-m:=mk_mob.o

all:
	make -C $(shell pwd)/../../WSL2-Linux-Kernel M=$(shell pwd) modules
	$(CC) testsimplechar.c -o test

clean:
	make -C $(shell pwd)/../../WSL2-Linux-Kernel M=$(shell pwd) clean
	rm test

