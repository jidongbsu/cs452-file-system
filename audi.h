/**
 * audi.h - the header file
 *
 * Author:
 *   Jidong Xiao <jidongxiao@boisestate.edu>
 */

#ifndef AUDI_H
#define AUDI_H

/* source: https://en.wikipedia.org/wiki/Hexspeak */
#define AUDI_MAGIC 0x12345678
/* file name can be at most 60 bytes. */
#define AUDI_FILENAME_LEN 60
/* each directory can have at most 64 files/sub directories. */
#define AUDI_MAX_SUBFILES 64
#define AUDI_ROOT_INO 2
#define AUDI_INODE_BLOCKS 5 /* reserve 5 blocks to store the inode table. */

/* 64 blocks in total, reserve one for superblock, 
 * one for inode bitmap, one for data bitmap, 5 for inode table, which matches with the book chapter.
 * And we have (64-1-1-1-5)=56 blocks for data. */

#define AUDI_MAX_BLOCKS 64
#define AUDI_DATA_BLOCKS 56

/*
 * audi file system partition layout
 * +---------------+
 * |  superblock   |  1 block
 * +---------------+
 * |  inode bitmap |  1 block
 * +---------------+
 * |  data bitmap  |  1 block
 * +---------------+
 * |  inode table  |  5 blocks
 * +---------------+
 * |    data       |
 * |    blocks     |  rest of the blocks
 * +---------------+
 */

#define AUDI_BLOCK_SIZE (1 << 12) /* each block is 4KB */
#define	AUDI_N_BLOCKS	1 /* in audi file system, we only have 1 direct pointer */
#define AUDI_MAX_FILESIZE \
    (uint64_t) AUDI_N_BLOCKS * AUDI_BLOCK_SIZE /* in our very simple file system, the max size of a file is 4KB. */

struct audi_inode {
	uint32_t i_mode;   /* File mode */
	uint32_t i_uid;    /* Owner id */
	uint32_t i_gid;    /* Group id */
	uint32_t i_size;   /* Size in bytes */
	uint32_t i_ctime;  /* Inode change time */
	uint32_t i_atime;  /* Access time */
	uint32_t i_mtime;  /* Modification time */
	uint32_t i_nlink;  /* Hard links count */
	uint32_t data_block;  /* Pointer to the block - we only support one block right now, in other words, each file/directory occupies at most one block.  */
	char padding [220]; /* add padding so as to make this matches with the one described in the book chapter: 256 bytes per inode. */
};

/* 4KB per block, 256 bytes per inode, thus, it's 4096/256=16 inodes per block. */
#define AUDI_INODES_PER_BLOCK \
    (AUDI_BLOCK_SIZE / sizeof(struct audi_inode))

/* super block data, follow ext2 and ext4 naming convention. 
 * as of now, this structure is 20 bytes. */
struct audi_sb_info {
    uint32_t s_magic; /* Magic signature */
    uint32_t s_inodes_count; /* Total inodes count */
    uint32_t s_blocks_count; /* Total blocks count */
    uint32_t s_free_inodes_count; /* Free inodes count */
    uint32_t s_free_blocks_count; /* Free blocks count */
};

extern unsigned long long inode_bitmap;
extern unsigned long long data_bitmap;

struct audi_dir_entry {
	uint32_t inode;
	char name[AUDI_FILENAME_LEN];
};

/* each dir entry is (4 bytes + 60 bytes) = 64 bytes, 
 * and we allow each directory to have 64 subdirectories/files.
 * thus, 64 entries at most.
 * 64*64=4096=4KB, therefore each dir block occupies one data block. */
struct audi_dir_block {
    struct audi_dir_entry files[AUDI_MAX_SUBFILES];
};

#ifdef __KERNEL__
 
extern struct kmem_cache * audi_inode_cachep;

struct audi_inode_info {
    uint32_t data_block;  /* pointer for this file/dir */
    struct inode vfs_inode;
};

/* superblock functions */
int audi_fill_super(struct super_block *sb, void *data, int silent);

/* inode functions */
struct inode *audi_iget(struct super_block *sb, unsigned long ino);

/* file functions */
extern const struct file_operations audi_file_ops;
extern const struct inode_operations audi_file_inode_ops;
extern const struct file_operations audi_dir_ops;
extern const struct inode_operations audi_dir_inode_ops;
extern const struct address_space_operations audi_aops;

/* Getters for superbock and inode */
#define AUDI_SB(sb) (sb->s_fs_info)
#define AUDI_INODE(inode) \
    (container_of(inode, struct audi_inode_info, vfs_inode))

#endif /* __KERNEL__ */

#endif /* AUDI_H */

/* vim: set ts=4: */
