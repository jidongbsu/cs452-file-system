/**
 * bitmap.h - header file
 *
 * This file is derived from simplefs.
 * Original Author: 
 *   Jim Huang <jserv.tw@gmail.com>
 *
 * This modified version is more like the very simple file system as described in the book "operating systems: three easy pieces".
 * Author:
 *   Jidong Xiao <jidongxiao@boisestate.edu>
 *
 */

#ifndef AUDIFS_BITMAP_H
#define AUDIFS_BITMAP_H

#include "audi.h"

/* note this functions reports the bit index counting from right most (as 0).
 * therefore for a number of 0xff0000000001ffff, this function will report bit 55. 
 * this function returns 255 if all bits are already 1. */

static unsigned char get_first_zero_bit(unsigned long long n)
{

	unsigned int ret = 0;
	n = ~n;
	while(n>0)
	{
		ret++;
		n >>= 1;
	}
 	return (ret-1);
}

/* this function will cause trouble if pass a bit index larger than the number of bits the integer has. */
static inline void audi_set_bit(int nr, void *addr)
{
        asm("btsl %1,%0" : "+m" (*(unsigned long *)addr) : "Ir" (nr));
}

/*
 * return an unused inode number and mark it used.
 * return 0 if no free inode was found.
 */
static inline unsigned int get_free_inode(struct audi_sb_info *sbi)
{
    unsigned int ret = get_first_zero_bit(inode_bitmap);
    if (ret != 255) {
    	audi_set_bit(ret, &inode_bitmap);
        sbi->s_free_inodes_count--;
		return (63-ret); // again, the bit index returned by get_first_zero_bit is counting from the right most, yet we want to count from the left most.
	}
    return 0;
}

/*
 * return a block number and mark it used.
 * return 0 if no free block was found.
 */
static unsigned int get_free_block(struct audi_sb_info *sbi)
{
    uint64_t ret = get_first_zero_bit(data_bitmap);
    if (ret != 255) {
    	audi_set_bit(ret, &data_bitmap);
        sbi->s_free_blocks_count--;
		return (63-ret); // again, the bit index returned by get_first_zero_bit is counting from the right most, yet we want to count from the left most.
	}
    return 0;
}

/* mark an inode as unused */
void put_inode(struct audi_sb_info *sbi, uint32_t ino)
{
pr_info("ino is %d, inode bitmap was 0x%llx\n", ino, inode_bitmap);
	/* clear bit ino and increment number of free inodes */
	inode_bitmap &= ~(1ULL << (63-ino)); // again, we need 63- here because that's how we use our bitmap.
    sbi->s_free_inodes_count++;
pr_info("ino is %d, inode bitmap is 0x%llx\n", ino, inode_bitmap);
}

/* mark a block as unused */
void put_block(struct audi_sb_info *sbi, uint32_t bno)
{
pr_info("data bitmap was 0x%llx\n", data_bitmap);
	/* clear bit bno and increment number of free blocks */
	data_bitmap &= ~(1ULL << (63-bno)); // again, we need 63- here because that's how we use our bitmap.
    sbi->s_free_blocks_count++;
pr_info("data bitmap is 0x%llx\n", data_bitmap);
}

#endif /* AUDIFS_BITMAP_H */

/* vim: set ts=4: */
