#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "bitmap.h"
#include "audi.h"

/* if we have already allocated memory for this inode, then just return its address;
 * otherwise, call iget_locked()->alloc_inode() to allocate memory for it and then return its address. 
 * for both cases, we still need to read the actual data of this inode from disk, and fill in the information
 * into its memory counterpart. in other words, for each inode, there is a copy on disk, but there is also 
 * a copy in memory. */
struct inode *audi_iget(struct super_block *sb, unsigned long ino)
{
	struct inode *inode = NULL;
	struct audi_inode *ainode = NULL;
	struct audi_inode_info *ai = NULL;
	struct audi_sb_info *sbi = AUDI_SB(sb);
	struct buffer_head *bh = NULL;
	struct buffer_head *bh2 = NULL;
	struct audi_dir_block *dblock;
	/* inode_blocks: which block this inode is located on. 
	 * according to the book chapter, it must be between block 3 and block 7.
	 * block 0 for super block, block 1 for inode bitmap, block 2 for data bitmap.
	 * block 8 to 63 for data blocks. */
	uint32_t inode_block = (ino / AUDI_INODES_PER_BLOCK) + 3; /* inode table is located at block 3 */
	uint32_t inode_shift = ino % AUDI_INODES_PER_BLOCK;
	int ret;

	/* Fail if ino is out of range */
	if (ino >= sbi->s_inodes_count) // we can have at most 80 inodes.
		return ERR_PTR(-EINVAL);

	/* search for the inode specified by ino in the inode cache 
 	 * and if present return it with an increased reference count.
 	 * if the inode is not in cache, allocate a new inode and return it locked, 
 	 * hashed, and with the I_NEW flag set. 
 	 * the first time we come to this iget() function, we don't have the inode information in the memory, 
 	 * and thus we will call alloc_inode() allocate memory for it, alloc_inode() will call kmem_cache_alloc(). */
	inode = iget_locked(sb, ino);
	if (!inode)
		return ERR_PTR(-ENOMEM);

	/* if inode is in cache, return it */
	/* FIXME: do we need to set i_state somewhere? */
	if (!(inode->i_state & I_NEW))
		return inode;

	/* read inode from disk and use the information 
	 * we read to initialize the newly allocated inode. */
	bh = sb_bread(sb, inode_block);
	if (!bh) {
		ret = -EIO;
		goto failed;
	}
	/* the information we just read is stored in bh->b_data. given the fact that 
	 * we are reading a block belonging to the inode table, 
	 * we know the block we just read contains many inodes. so we start from the first inode, 
	 * and move forward to that desired target inode. */
	ainode = (struct audi_inode *) bh->b_data;
	ainode += inode_shift;

	inode->i_sb = sb;

	inode->i_ino = ino;
	inode->i_mode = le32_to_cpu(ainode->i_mode);
	i_uid_write(inode, le32_to_cpu(ainode->i_uid));
	i_gid_write(inode, le32_to_cpu(ainode->i_gid));
	inode->i_size = le32_to_cpu(ainode->i_size);
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
	inode->i_mapping->a_ops = &audi_aops;
	pr_info("register audi_aops\n");
	if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &audi_dir_inode_ops;
		inode->i_fop = &audi_dir_ops;
		pr_info("register audi_dir_ops\n");

		/* directory inodes start off with i_nlink == 2,
		 * for directory, i_nlink means how many sub directories this directory has,
		 * by default it's 2, representing "." and "..".*/
		inc_nlink(inode);
	}else if(S_ISREG(inode->i_mode)){
		inode->i_op = &audi_file_inode_ops;
		inode->i_fop = &audi_file_ops;
		pr_info("register audi_file_ops\n");
	}

	/* see how alloc_inode() works: we allocate memory for a struct audi_inode, 
	 * but the VFS uses struct inode; so getting one from the other is frequently happening. */
	ai = AUDI_INODE(inode);
	/* struct inode is more generic, it doesn't track data_block, which is a pointer, 
	 * but struct audi_inode does track, because this pointer is file system specific,
	 * not every file system has such a pointer. */
	ai->data_block = le32_to_cpu(ainode->data_block);
	if(ino == AUDI_ROOT_INO){
		if(!(bh2 = sb_bread(sb, ai->data_block))){
        	return ERR_PTR(-EIO);
		}
		pr_info("audi_iget: reading block %d\n", ai->data_block);
    	dblock = (struct audi_dir_block *) bh2->b_data;
		dblock->entries[0].inode = ino;
		dblock->entries[0].name[0]='.';
		dblock->entries[0].name[1]='\0';
		dblock->entries[1].inode = -1; // question: is this number correct? or do we care?
		dblock->entries[1].name[0]='.';
		dblock->entries[1].name[1]='.';
		dblock->entries[1].name[2]='\0';
		pr_info("entries[0].inode is %d, entries[1].inode is %d\n", dblock->entries[0].inode, dblock->entries[1].inode);
    	mark_buffer_dirty(bh);
	}
	/* after sb_bread, once the information is obtained, we always need to call brelse. */
	brelse(bh);

	/* unlock the inode to make it usable; after calling iget_locked, 
	 * according to https://www.kernel.org/doc/htmldocs/filesystems/API-iget-locked.html,
	 * "the file system gets to fill it in before unlocking it via unlock_new_inode". */
	unlock_new_inode(inode);

	return inode;

failed:
	iget_failed(inode);
	return ERR_PTR(ret);
}

/* create a new inode in dir */
static struct inode *audi_new_inode(struct inode *dir, mode_t mode)
{
    struct inode *inode;
    struct audi_inode_info *ai;
    struct super_block *sb;
    struct audi_sb_info *sbi;
	struct buffer_head *bh;
	struct audi_dir_block *dblock;
    uint32_t ino, bno;
    char *block;
    int ret;

    /* check mode before doing anything to avoid undoing everything */
    if (!S_ISDIR(mode) && !S_ISREG(mode) ) {
        pr_err(
            "File type not supported (only directory and regular file "
            "supported)\n");
        return ERR_PTR(-EINVAL);
    }

    pr_info("creating a new inode\n");
    /* check if inodes are available */
    sb = dir->i_sb;
	/* from a generic struct super_block to our struct audi_sb_info */
    sbi = AUDI_SB(sb);
	/* report error if all inodes or all blocks are used. */
    if (sbi->s_free_inodes_count == 0 || sbi->s_free_blocks_count == 0)
        return ERR_PTR(-ENOSPC);

    /* get a new free inode */
    ino = get_free_inode(sbi);
	/* ino 0 means invalid, thus if we get 0, we can't allocate an inode */
    if (!ino)
        return ERR_PTR(-ENOSPC);

    pr_info("new inode: we ask for inode %u, and current inode bitmap is %llx\n", ino, inode_bitmap);
    inode = audi_iget(sb, ino);
    if (IS_ERR(inode)) {
        ret = PTR_ERR(inode);
        goto put_ino;
    }

    ai = AUDI_INODE(inode);

    /* get a free block for this new inode's index */
	/* FIXME: do we really need to do this when the newly created file is just an empty file? 
	 * although such a problem isn't a real problem in real life - it's not common to create empty files in real life... */
    bno = get_free_block(sbi);
    if (!bno) {
        ret = -ENOSPC;
        goto put_inode;
    }
	/* question: we just updated inode_bitmap and data_bitmap in memory, but how do we write it back to disk? 
	 * answer: we do so in audi_sync_fs(), which at least will get called when we unmount the file system. */

    pr_info("new inode, we ask for block %u, and current data bitmap is %llx\n", bno, data_bitmap);
    /* initialize inode, this function just initializes uid, gid, mode for new inode according to posix standards */
	/* for regular inodes, we call this inode_init_owner in audi_new_inode(),
	 * for root inode, we call this inode_init_owner in audi_fill_super().*/
	/* we already initialized inode's uid, gid, mode in the above iget() function, but here we set them again if needed. */
    inode_init_owner(inode, dir, mode);
	if(!(bh = sb_bread(sb, bno))){
       	return ERR_PTR(-EIO);
	}
	pr_info("audi_new_inode: reading block %d\n", bno);
   	dblock = (struct audi_dir_block *) bh->b_data;
   	block = (char *) dblock;
	/* zero out the block so as to clear old data */
   	memset(block, 0, AUDI_BLOCK_SIZE);
    if (S_ISDIR(mode)) {
		/* we don't just set data_block here, but we also really write the block's first two entries. */
		ai->data_block = bno;
		dblock->entries[0].inode = ino;
		dblock->entries[0].name[0]='.';
		dblock->entries[0].name[1]='\0';
		dblock->entries[1].inode = dir->i_ino; // question: is this number correct? or do we care?
		dblock->entries[1].name[0]='.';
		dblock->entries[1].name[1]='.';
		dblock->entries[1].name[2]='\0';
		pr_info("entries[0].inode is %d, entries[1].inode is %d\n", dblock->entries[0].inode, dblock->entries[1].inode);

		inode->i_size = AUDI_BLOCK_SIZE;
		inode->i_op = &audi_dir_inode_ops;
		inode->i_fop = &audi_dir_ops;
		set_nlink(inode, 2); /* . and .. */
		pr_info("register audi_dir_ops\n");
    } else if (S_ISREG(mode)) {
		ai->data_block = bno;
		inode->i_size = 0;
		inode->i_op = &audi_file_inode_ops;
		inode->i_fop = &audi_file_ops;
		pr_info("register audi_file_ops\n");
		inode->i_mapping->a_ops = &audi_aops;
		pr_info("register audi_aops\n");
		set_nlink(inode, 1);
    }
   	mark_buffer_dirty(bh);
	/* after sb_bread, once the information is obtained, we always need to call brelse. */
	brelse(bh);

	inode->i_ctime = inode->i_atime = inode->i_mtime = CURRENT_TIME;
	return inode;

put_inode:
	/* update data bitmap to mark this data block is free. */
    put_block(AUDI_SB(sb), AUDI_INODE(inode)->data_block);
    /* update inode bitmap to mark this inode is free. */
    put_inode(AUDI_SB(sb), inode->i_ino);
	/* dropping an inode's usage count. if the inode's use count hits
	 * zero, the inode is then freed and may also be destroyed. iput() is defined in fs/inode.c. */
	iput(inode);
put_ino:
    /* update inode bitmap to mark this inode is free. */
	put_inode(sbi, ino);
	return ERR_PTR(ret);
}

/*
 * this function is called to created a file or a directory under the directory,
 * which is represented by the first argument: dir. here we call this dir the parent directory.
 * by the time this function is called, the dentry for the new file/directory is already created,
 * although this dentry currently only has its d_name.
 * what this function should do:
 *   - if the new file's filename length is larger than AUDI_FILENAME_LEN, return -ENAMETOOLONG,
 *   - if the parent directory is already full, return -EMLINK - indicating "too many links",
 *   - otherwise, call audi_new_inode() to create an new inode, which will allocate a new inode and a new block,
 *   - call memset() to cleanup this block - so that data belonging to other files/directories (which used this block before) do not get leaked.
 *   - insert the dentry representing the new file/directory into the end of the parent directory's dentry table.
 */
static int audi_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl)
{
    struct inode *inode;
    /* get a new free inode */
    inode = audi_new_inode(dir, mode);
    return 0;
}

/* if we just run "ls", this lookup function won't be called - rather, audi_iterate() in dir.c will be called.
 * if we just run "ls -l", or "ls -la", or "ls -lai", this lookup function still won't be called - rather, audi_iterate() in dir.c will be called.
 * when we run a command like "ls -l a.txt" to list a file, this lookup function will be called; 
 * when we run a command like "rm -f abc" to delete a file, this lookup function also gets called;
 * when we run a command like "touch abc" to create a file, this lookup function also gets called.
 * in the context of file creation, if the one we are going to create is already existing, then it
 * can't created, it will be created only if after lookup(), dentry is NULL.
 * look for dentry in dir.
 * fill dentry with NULL if not in dir, with the corresponding inode if found.
 * returns NULL on success.
 * */
static struct dentry *audi_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags)
{
    return NULL;
}

/*
 * remove a link for a file including the reference in the parent directory.
 * dir represents the parent directory, dentry represents the one we want to delete.
 * If link count is 0, destroy file in this way:
 *   - remove the file from its parent directory.
 *   - cleanup blocks containing data
 *   - cleanup file index block
 *   - cleanup inode
 */
static int audi_unlink(struct inode *dir, struct dentry *dentry)
{
    return 0;
}

static int audi_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
    return 0;
}

/* dir is the parent directory; dentry represents the directory we want to delete. */
static int audi_rmdir(struct inode *dir, struct dentry *dentry)
{
    return 0;
}

const struct inode_operations audi_dir_inode_ops = {
	.lookup = audi_lookup, /* without this line, ls -a will not show the . and .. */
	.create = audi_create,
	.unlink = audi_unlink,
	.mkdir = audi_mkdir,
	.rmdir = audi_rmdir,
};

/* vim: set ts=4: */
