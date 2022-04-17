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
#include <linux/buffer_head.h>
#include <linux/mpage.h>

#include "audi.h"

/*
 * map the buffer_head passed in argument with the iblock-th block of the file
 * represented by inode. If the requested block is not allocated and create is
 * true, allocate a new block on disk and map it.
 */
static int audi_file_get_block(struct inode *inode, sector_t iblock, struct buffer_head *bh_result, int create)
{
	struct super_block *sb = inode->i_sb;
	/* given the standard inode, get the audi inode info */
	struct audi_inode_info *ai = AUDI_INODE(inode);
	int ret = 0, bno;

	printk(KERN_WARNING "calling audi file get block...\n");
	/* if block number exceeds filesize, fail */
	if (iblock >= AUDI_MAX_BLOCKS)
		return -EFBIG;

	bno = ai->data_block;

	/* map the physical block to the given buffer_head */
	map_bh(bh_result, sb, bno);

	return ret;
}

/*
 * called by the page cache to read a page from the physical disk and map it in
 * memory.
 */
static int audi_readpage(struct file *file, struct page *page)
{
	printk(KERN_WARNING "calling audi readpage\n");
    return mpage_readpage(page, audi_file_get_block);
}

/*
 * called by the page cache to write a dirty page to the physical disk (when
 * sync is called or when memory is needed).
 */
static int audi_writepage(struct page *page, struct writeback_control *wbc)
{
	printk(KERN_WARNING "calling audi writepage\n");
    return block_write_full_page(page, audi_file_get_block, wbc);
}

/*
 * called by the VFS when a write() syscall occurs on file before writing the
 * data in the page cache. This functions checks if the write will be able to
 * complete and allocates the necessary blocks through block_write_begin().
 */
static int audi_write_begin(struct file *file, struct address_space *mapping, loff_t pos, unsigned int len, unsigned int flags, struct page **pagep, void **fsdata)
{
    int err;

	printk(KERN_WARNING "calling audi write begin...\n");
    /* Check if the write can be completed (enough space?) */
    if (pos + len > AUDI_MAX_FILESIZE)
        return -ENOSPC;

    /* prepare the write */
    err = block_write_begin(mapping, pos, len, flags, pagep, audi_file_get_block);
    /* if this failed, reclaim newly allocated blocks */
    if (err < 0)
        printk(KERN_WARNING "newly allocated blocks reclaim not implemented yet\n");
    return err;
}

/*
 * called by the VFS after writing data from a write() syscall to the page
 * cache. This functions updates inode metadata and truncates the file if
 * necessary.
 */
static int audi_write_end(struct file *file, struct address_space *mapping, loff_t pos, unsigned int len, unsigned int copied, struct page *page, void *fsdata)
{
    struct inode *inode = file->f_inode;
	int ret;

	printk(KERN_WARNING "calling audi write end...\n");
    /* complete the write() */
    ret = generic_write_end(file, mapping, pos, len, copied, page, fsdata);
    if (ret < len) {
        printk(KERN_WARNING "wrote less than requested.");
        return ret;
    }

    /* update inode metadata */
    inode->i_mtime = inode->i_ctime = CURRENT_TIME;
    mark_inode_dirty(inode);

    return ret;
}

const struct address_space_operations audi_aops = {
	.readpage = audi_readpage,
	.writepage = audi_writepage,
	.write_begin = audi_write_begin,
	.write_end = audi_write_end,
};

const struct inode_operations audi_file_inode_ops = {
	.getattr = simple_getattr,
};

const struct file_operations audi_file_ops = {
	.read = do_sync_read,
	.aio_read = generic_file_aio_read,
	.write = do_sync_write,
	.aio_write = generic_file_aio_write,
	.mmap = generic_file_mmap,
	.fsync = noop_fsync,
	.splice_read = generic_file_splice_read,
	.splice_write = generic_file_splice_write,
	.llseek = generic_file_llseek,
};

/* vim: set ts=4: */
