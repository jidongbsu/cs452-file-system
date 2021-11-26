KERNEL_SOURCE=/lib/modules/$(shell uname -r)/build
MY_CFLAGS += -g -DDEBUG -O0
ccflags-y += ${MY_CFLAGS}
CC += ${MY_CFLAGS}

all: boogafs mkfs.boogafs

debug:
	make -C ${KERNEL_SOURCE} M=`pwd` modules
	EXTRA_CFLAGS="$(MY_CFLAGS)"

boogafs:
	 make -C ${KERNEL_SOURCE} M=`pwd` modules

obj-m += boogafs.o
boogafs-objs := booga.o super.o inode.o dir.o

mkfs.boogafs: mkfs.c
	$(CC) -std=gnu99 -Wall -o $@ $<

clean:
	make -C $(KERNEL_SOURCE) M=$(PWD) clean
	rm -f *~ $(PWD)/*.ur-safe
	rm -f mkfs.boogafs

.PHONY: all clean
