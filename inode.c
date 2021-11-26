#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "bitmap.h"
#include "booga.h"

static const struct address_space_operations boogafs_aops;

/* Get inode ino from disk */
struct inode *boogafs_iget(struct super_block *sb, unsigned long ino)
{
        struct inode *inode = NULL;
	struct boogafs_inode *cinode = NULL;
        struct boogafs_inode_info *ci = NULL;
	struct boogafs_sb_info *sbi = BOOGAFS_SB(sb);
	struct buffer_head *bh = NULL;
	uint32_t inode_block = (ino / BOOGAFS_INODES_PER_BLOCK) + 3; /* inode table is located at block 3 */
	uint32_t inode_shift = ino % BOOGAFS_INODES_PER_BLOCK;
	int ret;

	/* Fail if ino is out of range */
	if (ino >= sbi->nr_inodes)
		return ERR_PTR(-EINVAL);

	inode = iget_locked(sb, ino);
        //struct inode * inode = new_inode(sb);
        if (!inode)
        	return ERR_PTR(-ENOMEM);

	    /* If inode is in cache, return it */
	if (!(inode->i_state & I_NEW))
		return inode;

	/* Read inode from disk and initialize */
	bh = sb_bread(sb, inode_block);
	if (!bh) {
		ret = -EIO;
		goto failed;
	}
	cinode = (struct boogafs_inode *) bh->b_data;
	cinode += inode_shift;

        //inode->i_ino = ino;
	inode->i_sb = sb;

	inode->i_mode = S_IFDIR | 0755;
	//inode->i_mode = le32_to_cpu(cinode->i_mode);
	i_uid_write(inode, le32_to_cpu(cinode->i_uid));
	i_gid_write(inode, le32_to_cpu(cinode->i_gid));
	inode->i_size = 4096;
	//inode->i_size = le32_to_cpu(cinode->i_size);
	//inode->i_size = cinode->i_size;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
	inode->i_mapping->a_ops = &boogafs_aops;
	inode->i_op = &boogafs_dir_inode_ops;
	inode->i_fop = &boogafs_dir_ops;
	pr_info("register boogafs_dir_ops\n");

	/* directory inodes start off with i_nlink == 2 */
	inc_nlink(inode);

	ci = BOOGAFS_INODE(inode);
        ci->data_block = le32_to_cpu(cinode->data_block);

	brelse(bh);

	/* Unlock the inode to make it usable */
	unlock_new_inode(inode);

        return inode;

failed:
	brelse(bh);
	iget_failed(inode);
	return ERR_PTR(ret);
}

/* Create a new inode in dir */
static struct inode *boogafs_new_inode(struct inode *dir, mode_t mode)
{
    struct inode *inode;
    struct boogafs_inode_info *ci;
    struct super_block *sb;
    struct boogafs_sb_info *sbi;
    uint32_t ino, bno;
    int ret;

    /* Check mode before doing anything to avoid undoing everything */
    if (!S_ISDIR(mode) && !S_ISREG(mode) ) {
        pr_err(
            "File type not supported (only directory and regular file "
            "supported)\n");
        return ERR_PTR(-EINVAL);
    }

    pr_info("creating a new inode\n");
    /* Check if inodes are available */
    sb = dir->i_sb;
    sbi = BOOGAFS_SB(sb);
    if (sbi->nr_free_inodes == 0 || sbi->nr_free_blocks == 0)
        return ERR_PTR(-ENOSPC);

    /* Get a new free inode */
    ino = get_free_inode(sbi);
    if (!ino)
        return ERR_PTR(-ENOSPC);

    inode = boogafs_iget(sb, ino);
    if (IS_ERR(inode)) {
        ret = PTR_ERR(inode);
        goto put_ino;
    }

    ci = BOOGAFS_INODE(inode);

    /* Get a free block for this new inode's index */
    bno = get_free_blocks(sbi, 1);
    if (!bno) {
        ret = -ENOSPC;
        goto put_inode;
    }

    /* Initialize inode */
    inode_init_owner(inode, dir, mode);
    if (S_ISDIR(mode)) {
        ci->data_block = bno;
        inode->i_size = BOOGAFS_BLOCK_SIZE;
        inode->i_fop = &boogafs_dir_ops;
//        inode->i_fop = &simple_dir_operations;
        set_nlink(inode, 2); /* . and .. */
    } else if (S_ISREG(mode)) {
        ci->data_block = bno;
        inode->i_size = 0;
//        inode->i_fop = &boogafs_file_ops;
//        inode->i_mapping->a_ops = &boogafs_aops;
        set_nlink(inode, 1);
    }

    inode->i_ctime = inode->i_atime = inode->i_mtime = CURRENT_TIME;

    return inode;

put_inode:
    iput(inode);
put_ino:
    put_inode(sbi, ino);

    return ERR_PTR(ret);
}

/*
 * Create a file or directory in this way:
 *   - check filename length and if the parent directory is not full
 *   - create the new inode (allocate inode and blocks)
 *   - cleanup index block of the new inode
 *   - add new file/directory in parent index
 */
static int booga_create(struct inode *dir,
                           struct dentry *dentry,
                           umode_t mode,
                           bool excl)
{
    struct super_block *sb;
    struct inode *inode;
    struct boogafs_inode_info *ci_dir;
    struct boogafs_dir_block *dblock;
    char *fblock;
    struct buffer_head *bh, *bh2;
    int ret = 0, i;

    pr_info("creating a new file or directory...\n");
    /* Check filename length */
    if (strlen(dentry->d_name.name) > BOOGAFS_FILENAME_LEN)
        return -ENAMETOOLONG;

    /* Read parent directory index */
    ci_dir = BOOGAFS_INODE(dir);
    sb = dir->i_sb;
    bh = sb_bread(sb, ci_dir->data_block); /* we assume each directoy takes one block */
    if (!bh)
        return -EIO;

    dblock = (struct boogafs_dir_block *) bh->b_data;

    /* Check if parent directory is full */
    if (dblock->files[BOOGAFS_MAX_SUBFILES - 1].inode != 0) { /* because no deletion is supported */
        ret = -EMLINK;
        goto end;
    }

    pr_info("get a new inode..\n");
    /* Get a new free inode */
    inode = boogafs_new_inode(dir, mode);
    if (IS_ERR(inode)) {
        ret = PTR_ERR(inode);
        goto end;
    }

    /*
     * Scrub ei_block/dir_block for new file/directory to avoid previous data
     * messing with new file/directory.
     */
    bh2 = sb_bread(sb, BOOGAFS_INODE(inode)->data_block); /* FIXME: right now we only scrub the first block */
    if (!bh2) {
        ret = -EIO;
        goto iput;
    }
    fblock = (char *) bh2->b_data;
    memset(fblock, 0, BOOGAFS_BLOCK_SIZE);
    mark_buffer_dirty(bh2);
    brelse(bh2);

    /* Find first free slot in parent index and register new inode */
    for (i = 0; i < BOOGAFS_MAX_SUBFILES; i++)
        if (dblock->files[i].inode == 0)
            break;
    dblock->files[i].inode = inode->i_ino;
    strncpy(dblock->files[i].name, dentry->d_name.name,
            BOOGAFS_FILENAME_LEN);
    mark_buffer_dirty(bh);
    brelse(bh);

    /* Update stats and mark dir and new inode dirty */
    mark_inode_dirty(inode);
    dir->i_mtime = dir->i_atime = dir->i_ctime = CURRENT_TIME;
    if (S_ISDIR(mode))
        inc_nlink(dir);
    mark_inode_dirty(dir);

    /* setup dentry */
    d_instantiate(dentry, inode);

    return 0;

iput:
//    put_blocks(BOOGAFS_SB(sb), BOOGAFS_INODE(inode)->ei_block, 1);
    put_inode(BOOGAFS_SB(sb), inode->i_ino);
    iput(inode);
end:
    brelse(bh);
    return ret;
}

/* when we run a command like "ls -l a.txt" to list a file, this lookup function will be called. */

static struct dentry *booga_lookup(struct inode *dir,
                struct dentry *dentry, unsigned int flags)
{

	pr_info("lookup is called...\n");
        return NULL;
}

static const struct address_space_operations boogafs_aops = {
        .readpage       = simple_readpage,
        .write_begin    = simple_write_begin,
        .write_end      = simple_write_end,
};

const struct inode_operations boogafs_dir_inode_ops = {
	.lookup = booga_lookup, /* without this line, ls -a will not show the . and .. */
	.create = booga_create,
};
