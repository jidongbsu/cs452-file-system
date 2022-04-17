/**
 * dir.c - in this file we implement directory handlers.
 *
 * Author:
 *   Jidong Xiao <jidongxiao@boisestate.edu>
 */

#define pr_fmt(fmt) "audi: " fmt
  
#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "audi.h"

static int audi_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	pr_info("so they call this read dir...\n");
	return 0;
}

/*
 * called by the readdir() system call - which will then call getdents().
 * and getdents(), which is defined in fs/readdir.c, has this line:
 * 		iterate_dir(f.file, &buf.ctx);
 * and this iterate_dir() is also defined in fs/readdir.c, which has the following lines:
 * 		ctx->pos = file->f_pos;
 * 		res = file->f_op->iterate(file, ctx);
 * 		file->f_pos = ctx->pos;
 * the second line is actually calling our audi_iterate(),
 * which iterates over the files contained in dir and commit them in ctx.
 * return 0 on success.
 * struct dir_context is defined as (in include/linux/fs.h):
 * struct dir_context {
 * 	const filldir_t actor;
 *	loff_t pos;
 * };
 * note, here pos is just an integer; when pos is 0, it is indicating ".", 
 * when pos is 1, it is indicating "..".
 */
static int audi_iterate(struct file *dir, struct dir_context *ctx)
{
	struct inode *inode = file_inode(dir);
	struct audi_inode_info *ci = AUDI_INODE(inode);
	struct super_block *sb = inode->i_sb;
	struct buffer_head *bh = NULL;
	struct audi_dir_block *dblock = NULL;
	struct audi_dir_entry *dentry = NULL;
	int i;

	pr_info("read dir...\n");
	/* check that dir is a directory */
	if (!S_ISDIR(inode->i_mode))
		return -ENOTDIR;

	/*
	 * check that ctx->pos is not bigger than what we can handle (including
	 * . and ..)
	 */
	if (ctx->pos > AUDI_MAX_SUBFILES + 2)
		return 0;

	/* commit . and .. to ctx
	 */
	if (!dir_emit_dots(dir, ctx))
		return 0;	// question: why return 0 here?

	/* read the directory index block on disk */
	bh = sb_bread(sb, ci->data_block);
	if (!bh)
		return -EIO;
	dblock = (struct audi_dir_block *) bh->b_data;

	/* iterate over the index block and commit subfiles */
	for (i = ctx->pos - 2; i < AUDI_MAX_SUBFILES; i++) {
		dentry = &dblock->files[i];
		if (!dentry->inode)
			break;
	/* dir_emit() is defined in include/linux/fs.h as following:
	 * static inline bool dir_emit(struct dir_context *ctx, const char *name, int namelen, u64 ino, unsigned type)
	 * {
	 * 	return ctx->actor(ctx, name, namelen, ctx->pos, ino, type) == 0;
	 * }
	 * it seems this actor() is either filldir() (if users call getdents()) or filldir64() (if users call getdents64()), both defined in fs/readdir.c.
	 * so if we assume users call getdents(), then this dir_emit() will actually call filldir(), which will fill one dentry into ctx, 
	 * and then with for loop, we can fill in all valid dentries into ctx.
	 */
		if (!dir_emit(ctx, dentry->name, AUDI_FILENAME_LEN, dentry->inode, DT_UNKNOWN))
			break;
		ctx->pos++;
	}

	/* again, everytime we call sb_bread, once the result is used, we call brelse to decrement the reference count. */
	brelse(bh);

	pr_info("leaving read dir...\n");
	return 0;
}

static int audi_dir_open(struct inode *inode, struct file *file)
{
	/* let the kernel safely know that iterate is present */
	file->f_mode |= FMODE_KABI_ITERATE;
	return 0;
}

const struct file_operations audi_dir_ops = {
	.open	= audi_dir_open,
	.llseek	= generic_file_llseek,
//	.read	= generic_read_dir,
	.iterate	= audi_iterate,
	.readdir	= audi_readdir, /* on CentOS 7, they check this readdir; but on newer OS, it seems they check iterate. so it's either readdir, or iterate. */
	.fsync	= generic_file_fsync,
};

/* vim: set ts=4: */
