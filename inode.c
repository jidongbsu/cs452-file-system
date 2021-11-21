#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "bitmap.h"
#include "booga.h"

static const struct inode_operations boogafs_inode_ops;
//static const struct inode_operations symlink_inode_ops;

/* Get inode ino from disk */
struct inode *boogafs_iget(struct super_block *sb, const struct inode *dir, dev_t dev)
{
        struct inode * inode = new_inode(sb);
        if (!inode)
        	return ERR_PTR(-ENOMEM);
        inode->i_ino = get_next_ino();
	inode->i_sb = sb;
        pr_info("allocating inode %lu\n", inode->i_ino);
        inode_init_owner(inode, dir, S_IFDIR | S_IRUGO | S_IXUGO | S_IWUSR | S_IWGRP ); /* Init uid,gid,mode for new inode according to posix standards */
//      inode->i_mapping->a_ops = &boogafs_aops;
        mapping_set_gfp_mask(inode->i_mapping, GFP_HIGHUSER);
        mapping_set_unevictable(inode->i_mapping);
        inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
//                switch (mode & S_IFMT) {
//                default:
//                        init_special_inode(inode, mode, dev);
//                        break;
//                case S_IFREG:
//                        inode->i_op = &boogafs_inode_ops;
//                        inode->i_fop = &boogafs_file_ops;
//                        break;
//                case S_IFDIR:
        inode->i_op = &boogafs_inode_ops;
        inode->i_fop = &simple_dir_operations; /* without this line, ls -a will not show the . and .. */

        /* directory inodes start off with i_nlink == 2 (for "." entry) */
        inc_nlink(inode);
        pr_info("set i nlink to 2\n");
//                        break;
//                case S_IFLNK:
//                        inode->i_op = &page_symlink_inode_operations;
//                        break;
//                }
        return inode;
}

static const struct inode_operations boogafs_inode_ops = {
    .lookup = simple_lookup, /* without this line, ls -a will not show the . and .. */
};
