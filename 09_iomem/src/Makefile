DEBUG = y
ifeq ($(DEBUG),y)
EXTRA_CFLAGS += -O -g -DDEBUG
endif

obj-m+=mobi_module.o

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
	$(CC) main.c -lpthread -o test
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
	rm test
