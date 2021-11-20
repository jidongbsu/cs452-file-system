KERNEL_SOURCE=/lib/modules/$(shell uname -r)/build

all: boogafs mkfs.boogafs

boogafs:
	 make -C ${KERNEL_SOURCE} M=`pwd` modules

obj-m += boogafs.o
boogafs-objs := booga.o super.o inode.o

mkfs.boogafs: mkfs.c
	$(CC) -std=gnu99 -Wall -o $@ $<

clean:
	make -C $(KERNEL_SOURCE) M=$(PWD) clean
	rm -f *~ $(PWD)/*.ur-safe
	rm -f mkfs.boogafs

.PHONY: all clean
