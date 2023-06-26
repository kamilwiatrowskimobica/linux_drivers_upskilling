obj-m+=hello_world.o

modules:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
	
all: modules
	
# 	$(CC) mobchar_test.c -o test
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
# 	rm test