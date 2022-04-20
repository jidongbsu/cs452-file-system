/**
 * this file implements the application utility which allows us to format a disk partition/image into our specified layout.
 * Author:
 *   Jidong Xiao <jidongxiao@boisestate.edu>
 */

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h> /* for uint64_t? */
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/fs.h>
#include "audi.h"

struct superblock {
    struct audi_sb_info info; /* 20 bytes */
    char padding[4076]; /* Padding to match block size: 20+4076 = 4096 bytes = 4KB  */
};

/* Returns ceil(a/b) */
static inline uint32_t idiv_ceil(uint32_t a, uint32_t b)
{
    uint32_t ret = a / b;
    if (a % b)
        return ret + 1;
    return ret;
}

static struct superblock *write_superblock(int fd, struct stat *fstats)
{
    struct superblock *sb = malloc(sizeof(struct superblock)); /* note that here struct superblock's size is also 4KB */
    if (!sb)
        return NULL;

    uint32_t nr_blocks = fstats->st_size / AUDI_BLOCK_SIZE;
    uint32_t nr_inodes = AUDI_INODES_PER_BLOCK * AUDI_INODE_BLOCKS; /* as the chapter shows, 5 blocks reserved for the inodes, thus it is 16*5=80 inodes. */
    uint32_t nr_data_blocks =
        nr_blocks - 8; /* as the chapter shows, 56 data blocks: 64 - 1 - 1 - 1 - 5 = 56 */

    memset(sb, 0, sizeof(struct superblock));
    sb->info = (struct audi_sb_info){
        .s_magic = htole32(AUDI_MAGIC),
        .s_blocks_count = htole32(nr_blocks),
        .s_inodes_count = htole32(nr_inodes),
        .s_free_inodes_count = htole32(nr_inodes - 2), /* reserve one inode for the root inode, and inode 0 in Linux indicates the inode is invalid, thus we can't use 0. */
        .s_free_blocks_count = htole32(nr_data_blocks - 1), /* -1? because the first data block is for the root inode? */
    };

    int ret = write(fd, sb, sizeof(struct superblock));
    if (ret != sizeof(struct superblock)) {
        free(sb);
        return NULL;
    }

    printf(
        "superblock: (%ld bytes )\n"
        "\tmagic=%#x\n"
        "\ts_blocks_count=%u\n"
        "\ts_inodes_count=%u\n"
        "\ts_free_inodes_count=%u\n"
        "\ts_free_blocks_count=%u\n",
        sizeof(struct superblock), sb->info.s_magic, sb->info.s_blocks_count,
        sb->info.s_inodes_count, sb->info.s_free_inodes_count,
        sb->info.s_free_blocks_count);

    return sb;
}

static int write_inode_bitmap(int fd, struct superblock *sb)
{
    char *block = malloc(AUDI_BLOCK_SIZE);
    if (!block)
        return -1;

    unsigned long long *ibitmap = (unsigned long long *) block;

    /* Set all bits to 1 */
    memset(ibitmap, 0x00, AUDI_BLOCK_SIZE);

	/* a = 1010, 0xa000 0000 0000 0000 means 1010 0000 0000 0000 ....; we go from the left most bit. */
    ibitmap[0] = htole64(0xa000000000000000); /* we understand it is wasteful to use one entire block, because all we need is just 64 bits. we use bit 2 for root inode, and bit 0 is reserved - not sure why, but it seems inode 0 is considered as invalid by the VFS? */
    int ret = write(fd, ibitmap, AUDI_BLOCK_SIZE);
    if (ret != AUDI_BLOCK_SIZE) {
        ret = -1;
        goto end;
    }

    ret = 0;

    printf("inode bitmap: wrote 1 block; initial inode bitmap is: 0x%llx\n", *ibitmap);

end:
    free(block);

    return ret;
}

static int write_data_bitmap(int fd, struct superblock *sb)
{
    char *block = malloc(AUDI_BLOCK_SIZE);
    if (!block)
        return -1;

    unsigned long long *dbitmap = (unsigned long long *) block;

    /* set all bits to 1 */
    memset(dbitmap, 0x00, AUDI_BLOCK_SIZE);

    /* the first 8 data blocks are already reserved. the 9th block also reserved for the root. ff8 means 1111 1111 1000, i.e., we have the leftmost 9 bits reserved. */
    dbitmap[0] = htole64(0xff80000000000000);
    int ret = write(fd, dbitmap, AUDI_BLOCK_SIZE); /* we only need 64 bits, but we still reserve and write one entire block. */
    if (ret != AUDI_BLOCK_SIZE) {
        ret = -1;
        goto end;
    }

    ret = 0;

    printf("data bitmap: wrote 1 block; initial data bitmap is: 0x%llx\n", *dbitmap);
end:
    free(block);
    return ret;
}

static int write_inode_table(int fd, struct superblock *sb)
{
    /* Allocate 5 zeroed blocks for the inode table */
    char *blocks = malloc(AUDI_BLOCK_SIZE*AUDI_INODE_BLOCKS);
    if (!blocks)
        return -1;

    memset(blocks, 0, AUDI_BLOCK_SIZE);

    /* Root inode (inode 2) */
    struct audi_inode *inode = ((struct audi_inode *) blocks)+2; /* move forward 2*256=512 bytes - so as to skip inode 0 and 1, and write inode 2. */
    /* the first data block is right after the superblock, the inode bitmap, the data bitmap, and the inode table */
    uint32_t first_data_block = 8; /* we start counting from 0 - as the chapter shows. */
	/*FIXME: root inode isn't the first inode, what are we doing here? */
    inode->i_mode = htole32(S_IFDIR | 0755);
    inode->i_uid = htole32(1000); /* currently uid 1000 represents user cs452, or the first user in this system. */
    inode->i_gid = htole32(1000); /* gid 1000 is group cs452 */
    inode->i_size = htole32(AUDI_BLOCK_SIZE); /* we assume every file/directory in this file system occupies one block, thus its size is always 4KB. */
    inode->i_nlink = htole32(2);
    inode->data_block = htole32(first_data_block);
    int ret = write(fd, blocks, AUDI_BLOCK_SIZE*AUDI_INODE_BLOCKS); /* write whatever in blocks into this file. the first block in inode table is non zero, because we have to fill in the information about inode 2. */
    if (ret != AUDI_BLOCK_SIZE*AUDI_INODE_BLOCKS) {
        ret = -1;
        goto end;
    }

    /* in our very simple file system, there are 5 blocks storing the inode table. */

    ret = 0;

    printf(
        "inode table: wrote 5 blocks\n"
        "\tinode size = %ld bytes\n",
        sizeof(struct audi_inode));

end:
    free(blocks);
    return ret;
}

static int write_data_blocks(int fd, struct superblock *sb)
{
    /* allocate a block for the root directory */
    struct audi_dir_block *dblock = malloc(sizeof(struct audi_dir_block));
    if (!dblock)
        return -1;
    memset(dblock, 0, sizeof(struct audi_dir_block));

    /* first entry: . */
    strncpy(dblock->entries[0].name, ".", AUDI_FILENAME_LEN);
	dblock->entries[0].inode = AUDI_ROOT_INO;

    /* second entry: .. */
    strncpy(dblock->entries[1].name, "..", AUDI_FILENAME_LEN);
	dblock->entries[1].inode = -1; // we don't really use this one, so we just set it to -1.

	/* each dir block has 64 entries, 
	 * the remaining entries (entry 2 to entry 63) do not matter
	 * at this moment. */

	/* write whatever in dblock into this file */
	int ret = write(fd, dblock, sizeof(struct audi_dir_block));
	if (ret != AUDI_BLOCK_SIZE) {
		ret = -1;
		goto end;
	}

	ret = 0;

	printf("data blocks: wrote 1 block: two entries (\".\" and \"..\") for the root directory\n");

end:
    free(dblock);
    return ret;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s disk\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* Open disk image */
    int fd = open(argv[1], O_RDWR);
    if (fd == -1) {
        perror("open():");
        return EXIT_FAILURE;
    }

    /* Get image size */
    struct stat stat_buf;
    int ret = fstat(fd, &stat_buf);
    if (ret) {
        perror("fstat():");
        ret = EXIT_FAILURE;
        goto fclose;
    }

    /* Get block device size */
    if ((stat_buf.st_mode & S_IFMT) == S_IFBLK) {
        long int blk_size = 0;
        ret = ioctl(fd, BLKGETSIZE64, &blk_size);
        if (ret != 0) {
            perror("BLKGETSIZE64:");
            ret = EXIT_FAILURE;
            goto fclose;
        }
        stat_buf.st_size = blk_size;
    }

    /* Check if image size is 256KB - 64 blocks, 4 KB each block, thus it is 64*4=256KB */
    if (stat_buf.st_size != 64*4*1024) {
        fprintf(stderr, "Please make sure your image size is %ld KB)\n",
                stat_buf.st_size/1024);
        ret = EXIT_FAILURE;
        goto fclose;
    }

    /* Write superblock (block 0) */
    struct superblock *sb = write_superblock(fd, &stat_buf);
    if (!sb) {
        perror("write_superblock():");
        ret = EXIT_FAILURE;
        goto fclose;
    }

    /* Write inode bitmap blocks */
    ret = write_inode_bitmap(fd, sb);
    if (ret) {
        perror("write_inode_bitmap()");
        ret = EXIT_FAILURE;
        goto free_sb;
    }

    /* Write block free bitmap blocks */
    ret = write_data_bitmap(fd, sb);
    if (ret) {
        perror("write_data_bitmap()");
        ret = EXIT_FAILURE;
        goto free_sb;
    }

    /* Write inode table (from block 1) */
    ret = write_inode_table(fd, sb);
    if (ret) {
        perror("write_inode_table():");
        ret = EXIT_FAILURE;
        goto free_sb;
    }

    /* Write data blocks */
    ret = write_data_blocks(fd, sb);
    if (ret) {
        perror("write_data_blocks():");
        ret = EXIT_FAILURE;
        goto free_sb;
    }

free_sb:
    free(sb);
fclose:
    close(fd);

    return ret;
}

/* vim: set ts=4: */
