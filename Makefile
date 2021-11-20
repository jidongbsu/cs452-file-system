obj-m += boogafs.o
boogafs-objs := boogafs.o super.o inode.o

KDIR ?= /lib/modules/$(shell uname -r)/build

MKFS = mkfs.boogafs

all: $(MKFS)
	make -C $(KDIR) M=$(PWD) modules

IMAGE ?= test.img
IMAGESIZE ?= 50

$(MKFS): mkfs.c
	$(CC) -std=gnu99 -Wall -o $@ $<

$(IMAGE): $(MKFS)
	dd if=/dev/zero of=${IMAGE} bs=1M count=${IMAGESIZE}
	./$< $(IMAGE)

check: all
	script/test.sh $(IMAGE) $(IMAGESIZE) $(MKFS)

clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -f *~ $(PWD)/*.ur-safe
	rm -f $(MKFS) $(IMAGE)

.PHONY: all clean
