/**
* boogafs.c - the start file
 *
 * This file is derived from simplefs.
 * Original Author: 
 *   Jim Huang <jserv.tw@gmail.com>
 *
 * This modified version is more like the very simple file system as described in the book "operating systems: three easy pieces".
 * Author:
 *   Jidong Xiao <jidongxiao@boisestate.edu>
 */


#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "boogafs.h"

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Jidong Xiao");
MODULE_DESCRIPTION("a very simple file system");

/* Mount a boogafs partition */
struct dentry *boogafs_mount(struct file_system_type *fs_type,
                              int flags,
                              const char *dev_name,
                              void *data)
{
    struct dentry *dentry =
        mount_bdev(fs_type, flags, dev_name, data, boogafs_fill_super);
    if (IS_ERR(dentry))
        pr_err("'%s' mount failure\n", dev_name);
    else
        pr_info("'%s' mount success\n", dev_name);

    return dentry;
}

/* Unmount a boogafs partition */
void boogafs_kill_sb(struct super_block *sb)
{
    kill_block_super(sb);

    pr_info("unmounted disk\n");
}

static struct file_system_type boogafs_file_system_type = {
    .owner = THIS_MODULE,
    .name = "boogafs",
    .mount = boogafs_mount,
    .kill_sb = boogafs_kill_sb,
    .fs_flags = FS_REQUIRES_DEV,
    .next = NULL,
};

static int __init boogafs_init(void)
{
    int ret = boogafs_init_inode_cache();
    if (ret) {
        pr_err("inode cache creation failed\n");
        goto end;
    }

    ret = register_filesystem(&boogafs_file_system_type);
    if (ret) {
        pr_err("register_filesystem() failed\n");
        goto end;
    }

    pr_info("module loaded\n");
end:
    return ret;
}

static void __exit boogafs_exit(void)
{
    int ret = unregister_filesystem(&boogafs_file_system_type);
    if (ret)
        pr_err("unregister_filesystem() failed\n");

    boogafs_destroy_inode_cache();

    pr_info("module unloaded\n");
}

module_init(boogafs_init);
module_exit(boogafs_exit);
