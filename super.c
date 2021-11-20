#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/statfs.h>

#include "booga.h"

static struct kmem_cache *boogafs_inode_cache;

int boogafs_init_inode_cache(void)
{
    boogafs_inode_cache = kmem_cache_create(
        "boogafs_cache", sizeof(struct boogafs_inode_info), 0, 0, NULL);
    if (!boogafs_inode_cache)
        return -ENOMEM;
    return 0;
}

void boogafs_destroy_inode_cache(void)
{
    kmem_cache_destroy(boogafs_inode_cache);
}

/* Fill the struct superblock from partition superblock */
int boogafs_fill_super(struct super_block *sb, void *data, int silent)
{
    struct buffer_head *bh = NULL;
    struct boogafs_sb_info *csb = NULL;
    struct boogafs_sb_info *sbi = NULL;
    struct inode *root_inode = NULL;
    int ret = 0, i;

    pr_info("fill super block\n");
    /* Init sb */
    sb->s_magic = BOOGAFS_MAGIC;
    sb_set_blocksize(sb, BOOGAFS_BLOCK_SIZE);
    sb->s_maxbytes = BOOGAFS_MAX_FILESIZE;

    /* Read sb from disk */
    bh = sb_bread(sb, BOOGAFS_SB_BLOCK_NR);
    if (!bh)
        return -EIO;

    csb = (struct boogafs_sb_info *) bh->b_data;

    /* Check magic number */
    if (csb->magic != sb->s_magic) {
        pr_err("Wrong magic number\n");
        ret = -EINVAL;
        goto release;
    }

    /* Alloc sb_info */
    sbi = kzalloc(sizeof(struct boogafs_sb_info), GFP_KERNEL);
    if (!sbi) {
        ret = -ENOMEM;
        goto release;
    }

    sbi->nr_blocks = csb->nr_blocks;
    sbi->nr_inodes = csb->nr_inodes;
    sbi->nr_itable_blocks = csb->nr_itable_blocks;
    sbi->nr_ibitmap_blocks = csb->nr_ibitmap_blocks;
    sbi->nr_dbitmap_blocks = csb->nr_dbitmap_blocks;
    sbi->nr_free_inodes = csb->nr_free_inodes;
    sbi->nr_free_blocks = csb->nr_free_blocks;
    sb->s_fs_info = sbi;

    brelse(bh);

    /* Alloc and copy ifree_bitmap */
    sbi->ifree_bitmap =
        kzalloc(sbi->nr_ibitmap_blocks * BOOGAFS_BLOCK_SIZE, GFP_KERNEL);
    if (!sbi->ifree_bitmap) {
        ret = -ENOMEM;
        goto free_sbi;
    }

    for (i = 0; i < sbi->nr_ibitmap_blocks; i++) {
        int idx = sbi->nr_itable_blocks + i + 1;

        bh = sb_bread(sb, idx);
        if (!bh) {
            ret = -EIO;
            goto free_ifree;
        }

        memcpy((void *) sbi->ifree_bitmap + i * BOOGAFS_BLOCK_SIZE, bh->b_data,
               BOOGAFS_BLOCK_SIZE);

        brelse(bh);
    }

    /* Alloc and copy bfree_bitmap */
    sbi->bfree_bitmap =
        kzalloc(sbi->nr_dbitmap_blocks * BOOGAFS_BLOCK_SIZE, GFP_KERNEL);
    if (!sbi->bfree_bitmap) {
        ret = -ENOMEM;
        goto free_ifree;
    }

    for (i = 0; i < sbi->nr_dbitmap_blocks; i++) {
        int idx = sbi->nr_itable_blocks + sbi->nr_ibitmap_blocks + i + 1;

        bh = sb_bread(sb, idx);
        if (!bh) {
            ret = -EIO;
            goto free_bfree;
        }

        memcpy((void *) sbi->bfree_bitmap + i * BOOGAFS_BLOCK_SIZE, bh->b_data,
               BOOGAFS_BLOCK_SIZE);

        brelse(bh);
    }

    /* Create root inode */
    root_inode = boogafs_iget(sb, NULL, 0);
    if (IS_ERR(root_inode)) {
        ret = PTR_ERR(root_inode);
        goto free_bfree;
    }
//    inode_init_owner(root_inode, NULL, root_inode->i_mode);
    sb->s_root = d_make_root(root_inode);
    if (!sb->s_root) {
        ret = -ENOMEM;
        goto iput;
    }

    pr_info("super block filled\n");

    return 0;

iput:
    iput(root_inode);
free_bfree:
    kfree(sbi->bfree_bitmap);
free_ifree:
    kfree(sbi->ifree_bitmap);
free_sbi:
    kfree(sbi);
release:
    brelse(bh);

    return ret;
}
