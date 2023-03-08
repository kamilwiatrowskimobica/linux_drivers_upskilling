DEBUG = y

# Add your debugging flag (or not) to CFLAGS
ifeq ($(DEBUG),y)
	DEBFLAGS = -O -g -DWOAB_DEBUG
endif

LDDINC=$(PWD)/inc/

EXTRA_CFLAGS += $(DEBFLAGS)
EXTRA_CFLAGS += -I$(LDDINC)

obj-m += chardev.o
chardev-objs := ./src/chardev.o ./src/woab_seq_file.o ./src/woab_sync.o ./src/woab_irq.o ./src/woab_list.o

all:
	make -C /home/wa/WSL2-Linux-Kernel M=$(shell pwd) modules

clean:
	make -C /home/wa/WSL2-Linux-Kernel M=$(shell pwd) clean
