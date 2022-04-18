/*
 * super.c - in this file we implement functions to handle the superblock.
 * this file is mainly mimicking fs/ext2/super.c.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/buffer_head.h> /* so we can use sb_bread() */
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/statfs.h>

#include "audi.h"

/* either: fill_super()-> audi_iget() -> iget_locked() -> alloc_inode(sb) -> sb->s_op->alloc_inode(sb) 
 * or: fill_super() -> audi_iget() -> new_inode() -> new_inode_pseudo() -> alloc_inode(sb) -> sb->s_op->alloc_inode(sb)
 */
static struct inode *audi_alloc_inode(struct super_block *sb)
{
	struct audi_inode_info *ai = (struct audi_inode_info *)kmem_cache_alloc(audi_inode_cachep, GFP_KERNEL);
	if (!ai)
		return NULL;

	/* not sure why, but without this line the kernel crashes when mounting the file system. */
	inode_init_once(&ai->vfs_inode);
	/* note that we allocate memory for a struct audi_inode_info pointer,
	 * but we return a struct inode pointer. 
	 * plus, here we only allocate memory but we do not initialize the inode, ext2_alloc_inode() does the same. */
    return &ai->vfs_inode;
}

/* this function gets called during umount, as well as when we delete a file/directory. */
static void audi_destroy_inode(struct inode *inode)
{
	struct audi_inode_info *ai = AUDI_INODE(inode);
	pr_info("destroy inode %ld\n", inode->i_ino);
	kmem_cache_free(audi_inode_cachep, ai);
}

/* put_super: called when the VFS wishes to free the superblock (i.e. unmount);
 * but what memory do we need to release?? */
//static void audi_put_super(struct super_block *sb)
//{
//}

/* this method is called when the VFS needs to write an
 * inode to disc. The second parameter indicates whether the write
 * should be synchronous or not, not all filesystems check this flag.
 * e.g., this function gets called at runtime, likely whenever we write something into the inode, 
 * and mark it dirty. for example, when "touch abc", a few seconds later, this function gets called;
 * when "rm -f abc", a few seconds later, this function also gets called. */
static int audi_write_inode(struct inode *inode, struct writeback_control *wbc)
{
    struct audi_inode *disk_inode;
    struct audi_inode_info *ci = AUDI_INODE(inode);
    struct super_block *sb = inode->i_sb;
    struct audi_sb_info *sbi = AUDI_SB(sb);
    struct buffer_head *bh;
    uint32_t ino = inode->i_ino;
    uint32_t inode_block = (ino / AUDI_INODES_PER_BLOCK) + 3; /* inode table starts at block 3 */
    uint32_t inode_shift = ino % AUDI_INODES_PER_BLOCK;

    pr_info("writing inode %d at block %d\n", ino, inode_block);
    if (ino >= sbi->s_inodes_count)
        return 0;

	/* read the inode from the disk, update it, and write back to disk. */
    bh = sb_bread(sb, inode_block);
    if (!bh)
        return -EIO;

    disk_inode = (struct audi_inode *) bh->b_data;
    disk_inode += inode_shift;

    /* update the mode using what the generic inode has.
	 * this is the only place we actually update our audi_inode on disk. */
    disk_inode->i_mode = inode->i_mode;
    disk_inode->i_uid = i_uid_read(inode);
    disk_inode->i_gid = i_gid_read(inode);
    disk_inode->i_size = inode->i_size;
    disk_inode->i_ctime = inode->i_ctime.tv_sec;
    disk_inode->i_atime = inode->i_atime.tv_sec;
    disk_inode->i_mtime = inode->i_mtime.tv_sec;
    disk_inode->i_nlink = inode->i_nlink;
	/* this field is unique, the generic inode doesn't have this one. */
    disk_inode->data_block = ci->data_block;

    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);
    pr_info("writing inode finished\n");

    return 0;
}

/* this function is called when umount the file system,
 * and this is the moment when the super block on disk will be updated,
 * and the bitmaps will be updated on disk. */
static int audi_sync_fs(struct super_block *sb, int wait)
{
	struct audi_sb_info *sbi = AUDI_SB(sb);
	struct audi_sb_info *disk_sb;
	struct buffer_head *bh;
	unsigned long long *bitmap;

	pr_info("sync fs is called\n");
	/* flush superblock, which is block 0 */
	bh = sb_bread(sb, 0);
	if (!bh)
	return -EIO;

	disk_sb = (struct audi_sb_info *) bh->b_data;

	disk_sb->s_blocks_count = sbi->s_blocks_count;
	disk_sb->s_inodes_count = sbi->s_inodes_count;
	disk_sb->s_free_inodes_count = sbi->s_free_inodes_count;
	disk_sb->s_free_blocks_count = sbi->s_free_blocks_count;

	mark_buffer_dirty(bh);
	if (wait)
		sync_dirty_buffer(bh);
	brelse(bh);

	/* flush inode bitmap, which is block 1 */
	bh = sb_bread(sb, 1);
	if (!bh)
		return -EIO;

	/* turns out this pointer (bitmap) is needed. 
	 * if we just assign inode_bitmap to *bh->b_data, it won't work. */
	pr_info("sync fs: updating inode bitmap to 0x%llx\n", inode_bitmap);
	bitmap = (unsigned long long *) bh->b_data;
	*bitmap = inode_bitmap;

	mark_buffer_dirty(bh);
	if (wait)
		sync_dirty_buffer(bh);
	brelse(bh);

	pr_info("sync fs: updating data bitmap to 0x%llx\n", data_bitmap);
	/* flush data bitmap, which is block 2 */
	bh = sb_bread(sb, 2);
	if (!bh)
		return -EIO;

	bitmap = (unsigned long long *) bh->b_data;
	*bitmap = data_bitmap;

	mark_buffer_dirty(bh);
	if (wait)
		sync_dirty_buffer(bh);
	brelse(bh);

	pr_info("sync fs finished\n");
	return 0;
}

/* this function is called when the VFS needs to get filesystem statistics. 
 * either df command or the statfs() system call will trigger this function call. 
 * at first df -h should show that 36KB are used, because of the 8 reserved blocks and 1 block for root. */
static int audi_statfs(struct dentry *dentry, struct kstatfs *stat)
{
    struct super_block *sb = dentry->d_sb;
    struct audi_sb_info *sbi = AUDI_SB(sb);

    pr_info("statfs is called\n");
    stat->f_type = AUDI_MAGIC;
    stat->f_bsize = AUDI_BLOCK_SIZE;
    stat->f_blocks = sbi->s_blocks_count; // this is the maximum.
    stat->f_bfree = sbi->s_free_blocks_count; // this is what's remaining.
    stat->f_bavail = sbi->s_free_blocks_count;	// we consider f_bfree and f_bavail as the same.
    stat->f_files = sbi->s_inodes_count - sbi->s_free_inodes_count;
    stat->f_ffree = sbi->s_free_inodes_count;
    stat->f_namelen = AUDI_FILENAME_LEN;

    return 0;
}

static const struct super_operations audi_super_ops = {
//    .put_super = audi_put_super,
    .alloc_inode = audi_alloc_inode,
    .destroy_inode = audi_destroy_inode,
    .write_inode = audi_write_inode,
    .sync_fs = audi_sync_fs,
    .statfs = audi_statfs,
};

/* this function will be called when mounting the file system.
 * this function reads the superblock information from disk, and fill the struct super_block - this structure is defined in include/linux/fs.h.
 * if successful, return 0; 
 * when this function returns, the kernel expects sb to be filled with content, otherwise the kernel will crash - 
 * but some fields of sb is already filled prior to enter into this function, such as s_id?
 * the ext2 ext2_fill_super() calls get_sb_block() to get the super block number, we do not call that, as we know our superblock is located at block 0.
 * also, ext2 defines both struct ext2_sb_info and struct ext2_super_block, but we only need one. */
int audi_fill_super(struct super_block *sb, void *data, int silent)
{
	struct buffer_head * bh;
	struct audi_sb_info * sbi;
	/* representing the root inode */
	struct inode *root;
	long ret = -EINVAL;
	pr_info("file system mounted at %s\n", sb->s_id);
    pr_info("fill super block\n");

	/* read block 0, as that's our superblock; and we do not need to allocate memory for bh, 
	 * and sb_bread() reads the block and stores the data in bh->b_data, and the block size is stored in bh->b_size. */
	if (!(bh = sb_bread(sb, 0))) {
		pr_info("error: unable to read superblock");
		goto failed_sbi;
	}

	/* after this line, the super block information is now stored in the addressed pointed to by sbi,
	 * but this function audi_fill_super() aims to fill everything into sb. note we do not need to allocate memory for sbi. 
	 * FIXME: but then how do we free the memory?? */
	sbi = (struct audi_sb_info *) ((char *)bh->b_data);

	/* in struct super_block, there is "void  *s_fs_info;" commented as "filesystem private info". 
	 * this line must be after the above line, which sets sbi. */
	sb->s_fs_info = sbi;

	/* le32_to_cpu() converts a 32-bit little-endian integer to its 32-bit representation on the current CPU. 
	 * ext2 uses a 16-bit magic number 0xEF53, but we use a 32-bit magic number, the s_magic is an unsigned long variable. */
	sb->s_magic = le32_to_cpu(sbi->s_magic);
	if (sb->s_magic != AUDI_MAGIC)
		goto cantfind_audi;

	/* set sb->s_blocksize to AUDI_BLOCK_SIZE, which is 4KB, as defined in audi.h. 
	 * question: do we need to call this sb_set_blocksize(sb, AUDI_BLOCK_SIZE);? */
	sb->s_blocksize = AUDI_BLOCK_SIZE;
	if (sb->s_blocksize != bh->b_size) {
		if (!silent)
			pr_info("error: unsupported blocksize");
		goto failed_mount;
	}

	sb->s_maxbytes = AUDI_MAX_FILESIZE; /* as of now, we only use 1 direct pointer, which points to one block, thus the max file size is 4KB */
	sb->s_op = &audi_super_ops;
    brelse(bh); /* decrement a buffer_head's reference count */

	/* read inode_bitmap, ext2 doesn't do it here because they have a bitmap for each block group, we only have one block group. */
	/* in audi file system, the inode bitmap is right after the super block, thus it's block 1. */
	bh = sb_bread(sb, 1);
	if (!bh) {
		ret = -EIO;
		goto failed_sbi;
	}

	inode_bitmap = *(unsigned long long *)(bh->b_data);
	pr_info("inode bitmap is 0x%llx\n", inode_bitmap);
	brelse(bh); /* decrement a buffer_head's reference count */

    /* read inode_bitmap, ext2 doesn't do it here because they have a bitmap for each block group, we only have one block group. */
    /* in audi file system, the data bitmap block is right after the inode bitmap block, thus it's block 2. */
	bh = sb_bread(sb, 2);
	if (!bh) {
		ret = -EIO;
		goto failed_sbi;
	}
	data_bitmap = *(unsigned long long *)(bh->b_data);
	/* the below line should print 0x1ff, 
	 * as that's our initial data bitmap, 9 blocks reserved already. */
	pr_info("data bitmap is 0x%llx\n", data_bitmap);
	brelse(bh); /* decrement a buffer_head's reference count */

	/* create root inode: create means create its data structure in the memory, 
  	 * as opposed to on disk - the root inode is already existing on the disk, 
  	 * created when we initialize the image with mkfs. inode number can not be zero: it seems that VFS considers 0 as an invalid inode number; thus here we use 2. */
	pr_info("create root inode...\n");
	root = audi_iget(sb, AUDI_ROOT_INO);
	if (IS_ERR(root)) {
		ret = PTR_ERR(root);
		goto failed_sbi;
	}
	/* root inode must be representing a directory. its size in bytes can't be 0. */
	if (!S_ISDIR(root->i_mode) || !root->i_size) {
		iput(root);
		pr_info("error: corrupt root inode");
		goto failed_sbi;
	}

	pr_info("init root inode...\n");
	/* allocate the root dentry. struct super_block is defined in include/linux/fs.h, 
	 * it has a field called "struct dentry * s_root". d_make_root()->__d_alloc() to allocate a dentry:
	 * dentry = kmem_cache_alloc(dentry_cache, GFP_KERNEL);
	 * once a dentry is returned, then d_make_root() calls d_instantiate() to fill in inode information for this dentry.
	 * it looks like for root directory, its dentry d_iname is "/", and d_name.len is 1. */
	sb->s_root = d_make_root(root);
	if (!sb->s_root) {
		pr_info("error: get root inode failed");
		ret = -ENOMEM;
		goto failed_sbi;
	}

    pr_info("super block filled\n");

    return 0;
cantfind_audi:
	if (!silent)
		/* it appears that s_id gives the name of the mounting path? */
		pr_info("error: can't find an audi filesystem on dev %s.", sb->s_id);
failed_mount:
	brelse(bh);
failed_sbi:
	sb->s_fs_info = NULL;
	return ret;
}

/* vim: set ts=4: */
