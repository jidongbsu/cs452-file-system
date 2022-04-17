/*
 * audi_main.c -- the audi file system kernel module;
 * this file is mimicking fs/ext2/super.c in the Linux kernel.
 *
 *********/

#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* for kmalloc() */
#include <linux/version.h> /* for kmalloc() */
#include <linux/fs.h>     /* everything... */
#include <linux/file.h>     /* everything... */
#include <linux/errno.h>  /* error codes */
#include <linux/types.h>  /* size_t */
#include <linux/kmod.h>        /* for request_module */
#include <linux/init.h>

#include "audi.h"        /* local definitions */

MODULE_AUTHOR("Jidong Xiao");	// change this line to your name.
MODULE_DESCRIPTION("Audi Filesystem");
MODULE_LICENSE("GPL");

#define AUDI_DEBUG 1

/* unsigned long long has 64 bits, equal to uint64_t 
 * unsigned int - 32bits, uint32_t; 
 * unsigned short - 16 bits, uint16_t; 
 * unsigned char - 8 bits, uint8_t */
unsigned long long inode_bitmap=0;
unsigned long long data_bitmap=0; 

/* inodes are allocated/deallocated so frequently, 
 * it's better to reserve a memory pool from the slab memory system
 * so future allocation/deallocation will be faster, 
 * via kmem_cache_alloc() and kmem_cache_free(). */

struct kmem_cache * audi_inode_cachep;

static int audi_init_inodecache(void)
{
	audi_inode_cachep = kmem_cache_create("audi_inode_cache",
											sizeof(struct audi_inode_info),
											0, (SLAB_RECLAIM_ACCOUNT|
											SLAB_MEM_SPREAD),
											NULL);
	if(audi_inode_cachep == NULL)
		return -ENOMEM;
	return 0;
}

static void audi_destroy_inodecache(void)
{
	/*
	 * Make sure all delayed rcu free inodes are flushed before we
	 * destroy cache.
	 * */
	rcu_barrier();
	kmem_cache_destroy(audi_inode_cachep);
}

static struct dentry *audi_mount(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data)
{
	return mount_bdev(fs_type, flags, dev_name, data, audi_fill_super);
}

static struct file_system_type audi_fs_type = {
        .owner          = THIS_MODULE,
        .name           = "audi",
        .mount          = audi_mount,
        .kill_sb        = kill_block_super,
        .fs_flags       = FS_REQUIRES_DEV,
};

/*
 * module initialization
 */

static int __init init_audi_fs(void)
{
	int err = audi_init_inodecache();
	if (err)
		goto out;
	err = register_filesystem(&audi_fs_type);
	if (err)
		goto out;
#ifdef AUDI_DEBUG
	printk(KERN_WARNING "audi file system is loaded\n");
#endif
	return 0;
out:
	audi_destroy_inodecache();
	return err;
}

/*
 * module exit
 */

static void __exit exit_audi_fs(void)
{
	unregister_filesystem(&audi_fs_type);
	audi_destroy_inodecache();
#ifdef AUDI_DEBUG
	printk(KERN_WARNING "audi file system is unloaded\n");
#endif
}

module_init(init_audi_fs);
module_exit(exit_audi_fs);

/* vim: set ts=4: */
