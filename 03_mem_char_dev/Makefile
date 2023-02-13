obj-m+=hello.o

all: build_modules build_tests

build_modules:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
	rm -f ./test
build_tests:
	gcc -o test test*.c
#test need to be run with su priviledges
test: build_tests
	./test
format:
	clang-format -i hello.c --assume-filename=./.clang-format
	clang-format -i hello.h --assume-filename=./.clang-format
	clang-format -i test*.c --assume-filename=./.clang-format_US
