/**
 * boogafs.h - the header file
 *
 * This file is derived from simplefs.
 * Original Author: 
 *   Jim Huang <jserv.tw@gmail.com>
 *
 * This modified version is more like the very simple file system as described in the book "operating systems: three easy pieces".
 * Author:
 *   Jidong Xiao <jidongxiao@boisestate.edu>
 */

#ifndef BOOGAFS_H
#define BOOGAFS_H

/* source: https://en.wikipedia.org/wiki/Hexspeak */
#define BOOGAFS_MAGIC 0xDEADCELL
#define BOOGAFS_SB_BLOCK_NR 0
#define BOOGAFS_BLOCK_SIZE (1 << 12) /* 4 KiB */
#define BOOGAFS_MAX_FILESIZE                                      \
    (uint64_t) BOOGAFS_N_BLOCKS * BOOGAFS_BLOCK_SIZE

/*
 * boogafs partition layout
 * +---------------+
 * |  superblock   |  1 block
 * +---------------+
 * |  inode bitmap |  sb->nr_ifree_blocks blocks
 * +---------------+
 * |  data bitmap  |  sb->nr_bfree_blocks blocks
 * +---------------+
 * |  inode table  |  sb->nr_istore_blocks blocks
 * +---------------+
 * |    data       |
 * |    blocks     |  rest of the blocks
 * +---------------+
 */


/**
 * Constants relative to the data blocks
 */
#define	BOOGAFS_NDIR_BLOCKS		12
#define	BOOGAFS_N_BLOCKS		BOOGAFS_NDIR_BLOCKS /* in boogafs, we only have direct pointers */

struct boogafs_inode {
    uint32_t i_mode;   /* File mode */
    uint32_t i_uid;    /* Owner id */
    uint32_t i_gid;    /* Group id */
    uint32_t i_size;   /* Size in bytes */
    uint32_t i_ctime;  /* Inode change time */
    uint32_t i_atime;  /* Access time */
    uint32_t i_mtime;  /* Modification time */
    uint32_t i_blocks; /* Block count */
    uint32_t i_nlink;  /* Hard links count */
    __le32  i_block[BOOGAFS_N_BLOCKS];  /* Pointers to blocks */
};

#define BOOGAFS_INODES_PER_BLOCK \
    (BOOGAFS_BLOCK_SIZE / sizeof(struct boogafs_inode))

struct boogafs_sb_info {
    uint32_t magic; /* Magic number */

    uint32_t nr_blocks; /* Total number of blocks */
    uint32_t nr_inodes; /* Total number of inodes */

    uint32_t nr_itable_blocks; /* Number of inode table blocks */
    uint32_t nr_ibitmap_blocks;  /* Number of inode bitmap blocks */
    uint32_t nr_dbitmap_blocks;  /* Number of data bitmap blocks */

    uint32_t nr_free_inodes; /* Number of free inodes */
    uint32_t nr_free_blocks; /* Number of free blocks */

#ifdef __KERNEL__
    unsigned long *ifree_bitmap; /* In-memory free inodes bitmap */
    unsigned long *bfree_bitmap; /* In-memory free blocks bitmap */
#endif
};

#ifdef __KERNEL__

struct boogafs_inode_info {
    __le32 i_data[12];
    struct inode vfs_inode;
};

/* superblock functions */
int boogafs_fill_super(struct super_block *sb, void *data, int silent);

/* inode functions */
int boogafs_init_inode_cache(void);
void boogafs_destroy_inode_cache(void);
struct inode *boogafs_iget(struct super_block *sb, const struct inode *dir, dev_t dev);

/* file functions */
extern const struct file_operations boogafs_file_ops;
extern const struct address_space_operations boogafs_aops;

/* Getters for superbock and inode */
/* #define BOOGAFS_SB(sb) (sb->s_fs_info) */
/* #define BOOGAFS_INODE(inode) \
    (container_of(inode, struct boogafs_inode_info, vfs_inode)) */

#endif /* __KERNEL__ */

#endif /* BOOGAFS_H */
