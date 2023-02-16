obj-m+=hello.o

DEBUG=y


# Add debugging flags (or not) to EXTRA_CFLAGS
ifeq ($(DEBUG),y)
  DEBFLAGS = -O -g -DMTLE_DEBUG # "-O" is needed to expand inlines
else
  DEBFLAGS = -O2
endif

EXTRA_CFLAGS += $(DEBFLAGS)


all: build_modules build_tests
	echo "Using following EXTRA_CFLAGS: ${EXTRA_CFLAGS}"

build_modules:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
	rm -f ./test
build_tests:
	gcc -o test test.c
#test need to be run with su priviledges
test: build_tests
	./test
format:
	clang-format -i hello.c --assume-filename=./.clang-format
	clang-format -i test.c --assume-filename=./.clang-format_US
