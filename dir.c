/**
 * dir.c - in this file we implement directory handlers.
 *
 * This file is derived from simplefs.
 * Original Author: 
 *   Jim Huang <jserv.tw@gmail.com>
 *
 * This modified version allows booga_iterate to be called when users run "ls -l" in the file system.
 * Author:
 *   Jidong Xiao <jidongxiao@boisestate.edu>
 */

#define pr_fmt(fmt) "boogafs: " fmt
  
#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "booga.h"

static int booga_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	pr_info("so they call this read dir...\n");
	return 0;
}

/*
 * Iterate over the files contained in dir and commit them in ctx.
 * This function is called by the VFS while ctx->pos changes.
 * Return 0 on success.
 */
static int booga_iterate(struct file *dir, struct dir_context *ctx)
{
    struct inode *inode = file_inode(dir);
    struct boogafs_inode_info *ci = BOOGAFS_INODE(inode);
    struct super_block *sb = inode->i_sb;
    struct buffer_head *bh = NULL;
    struct boogafs_dir_block *dblock = NULL;
    struct boogafs_dir_entry *dentry = NULL;
    int i;

    pr_info("read dir...checker 1\n");
    /* Check that dir is a directory */
    if (!S_ISDIR(inode->i_mode))
        return -ENOTDIR;

    pr_info("read dir...checker 2\n");
    /*
     * Check that ctx->pos is not bigger than what we can handle (including
     * . and ..)
     */
    if (ctx->pos > BOOGAFS_MAX_SUBFILES + 2)
        return 0;

    pr_info("read dir...checker 3\n");
    /* Commit . and .. to ctx */
    if (!dir_emit_dots(dir, ctx))
        return 0;

    pr_info("read dir...checker 4\n");
    /* Read the directory index block on disk */
    bh = sb_bread(sb, ci->data_block);
    if (!bh)
        return -EIO;
    dblock = (struct boogafs_dir_block *) bh->b_data;

    pr_info("read dir...checker 5\n");
    /* Iterate over the index block and commit subfiles */
    for (i = ctx->pos - 2; i < BOOGAFS_MAX_SUBFILES; i++) {
        dentry = &dblock->files[i];
        if (!dentry->inode)
            break;
        if (!dir_emit(ctx, dentry->name, BOOGAFS_FILENAME_LEN, dentry->inode,
                      DT_UNKNOWN))
            break;
        ctx->pos++;
    }

    brelse(bh);

    pr_info("so this is finally called. leaving read dir...\n");
    return 0;
}

static int booga_dir_open(struct inode *inode, struct file *file)
{
        /* Let the kernel safely know that iterate is present */
        file->f_mode |= FMODE_KABI_ITERATE;
        return 0;
}

const struct file_operations boogafs_dir_ops = {
	.open	= booga_dir_open,
	.llseek	= generic_file_llseek,
//	.read	= generic_read_dir,
	.iterate	= booga_iterate,
	.readdir	= booga_readdir, /* on CentOS 7, they check this readdir; but on newer OS, it seems they check iterate. so it's either readdir, or iterate. */
	.fsync	= generic_file_fsync,
};

