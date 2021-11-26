/**
 * This file is derived from simplefs.
 * Original Author: 
 * Jim Huang <jserv.tw@gmail.com>
 *
 * This modified version is more like the very simple file system as described in the book "operating systems: three easy pieces".
 * Author:
 *   Jidong Xiao <jidongxiao@boisestate.edu>
 */

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/fs.h>
#include "booga.h"

struct superblock {
    struct boogafs_sb_info info;
    char padding[4064]; /* Padding to match block size */
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

    uint32_t nr_blocks = fstats->st_size / BOOGAFS_BLOCK_SIZE;
    uint32_t nr_inodes = BOOGAFS_INODES_PER_BLOCK;
    uint32_t nr_ibitmap_blocks = 1; /* reserve one block to store inode bitmap */
    uint32_t nr_dbitmap_blocks = 1; /* reserve one block to store data bitmap */
    uint32_t nr_itable_blocks = 1; /* reserve one block to store inode table */
    uint32_t nr_data_blocks =
        nr_blocks - 1 - nr_itable_blocks - nr_ibitmap_blocks - nr_dbitmap_blocks; /* 60 blocks */

    memset(sb, 0, sizeof(struct superblock));
    sb->info = (struct boogafs_sb_info){
        .magic = htole32(BOOGAFS_MAGIC),
        .nr_blocks = htole32(nr_blocks),
        .nr_inodes = htole32(nr_inodes),
        .nr_itable_blocks = htole32(nr_itable_blocks),
        .nr_ibitmap_blocks = htole32(nr_ibitmap_blocks),
        .nr_dbitmap_blocks = htole32(nr_dbitmap_blocks),
        .nr_free_inodes = htole32(nr_inodes - 1), /* reserve one inode for the root inode? */
        .nr_free_blocks = htole32(nr_data_blocks - 1), /* -1? because the first data block is for the root inode? */
    };

    int ret = write(fd, sb, sizeof(struct superblock));
    if (ret != sizeof(struct superblock)) {
        free(sb);
        return NULL;
    }

    printf(
        "Superblock: (%ld)\n"
        "\tmagic=%#x\n"
        "\tnr_blocks=%u\n"
        "\tnr_inodes=%u (inode table=%u blocks)\n"
        "\tnr_ibitmap_blocks=%u\n"
        "\tnr_dbitmap_blocks=%u\n"
        "\tnr_free_inodes=%u\n"
        "\tnr_free_blocks=%u\n",
        sizeof(struct superblock), sb->info.magic, sb->info.nr_blocks,
        sb->info.nr_inodes, sb->info.nr_itable_blocks, sb->info.nr_ibitmap_blocks,
        sb->info.nr_dbitmap_blocks, sb->info.nr_free_inodes,
        sb->info.nr_free_blocks);

    return sb;
}

static int write_inode_bitmap(int fd, struct superblock *sb)
{
    char *block = malloc(BOOGAFS_BLOCK_SIZE);
    if (!block)
        return -1;

    uint64_t *ifree = (uint64_t *) block;

    /* Set all bits to 1 */
    memset(ifree, 0xff, BOOGAFS_BLOCK_SIZE);

    /* First ifree block, containing first used inode */
    ifree[0] = htole64(0xfffffffffffffffd);
    int ret = write(fd, ifree, BOOGAFS_BLOCK_SIZE);
    if (ret != BOOGAFS_BLOCK_SIZE) {
        ret = -1;
        goto end;
    }

    ret = 0;

    printf("inode bitmap: wrote 1 block(s)\n");

end:
    free(block);

    return ret;
}

static int write_data_bitmap(int fd, struct superblock *sb)
{
    char *block = malloc(BOOGAFS_BLOCK_SIZE);
    if (!block)
        return -1;

    uint64_t *bfree = (uint64_t *) block;

    /* set all bits to 1 */
    memset(bfree, 0xff, BOOGAFS_BLOCK_SIZE);

    /* First bfree block, containing first used blocks */
    bfree[0] = htole64(0xffffffffffffffe0);
    int ret = write(fd, bfree, BOOGAFS_BLOCK_SIZE);
    if (ret != BOOGAFS_BLOCK_SIZE) {
        ret = -1;
        goto end;
    }

    ret = 0;

    printf("data bitmap: wrote 1 block(s)\n");
end:
    free(block);
    return ret;
}

static int write_inode_table(int fd, struct superblock *sb)
{
    /* Allocate a zeroed block for inode table */
    char *block = malloc(BOOGAFS_BLOCK_SIZE);
    if (!block)
        return -1;

    memset(block, 0, BOOGAFS_BLOCK_SIZE);

    /* Root inode (inode 2) */
    struct boogafs_inode *inode = (struct boogafs_inode *) block;
    /* the first data block is right after the superblock, the inode bitmap, the data bitmap, and the inode table */
    uint32_t first_data_block = 4;
    inode->i_mode = htole32(S_IFDIR | 0755);
    inode->i_uid = htole32(1000); /* currently uid 1000 represents user cs453, or the first user in this system. */
    inode->i_gid = htole32(1000); /* gid 1000 is group cs453 */
    inode->i_size = htole32(BOOGAFS_BLOCK_SIZE);
    inode->i_nlink = htole32(2);
    inode->data_block = htole32(first_data_block);
    lseek(fd, BOOGAFS_ROOT_INO * sizeof(struct boogafs_inode), SEEK_CUR);
    int ret = write(fd, block, BOOGAFS_BLOCK_SIZE); /* the first block is non zero, because we have to fill in the information about inode 2. */
    if (ret != BOOGAFS_BLOCK_SIZE) {
        ret = -1;
        goto end;
    }

    /* in our very simple file system, there is only one block storing the inode table. */

    ret = 0;

    printf(
        "inode table: wrote 1 block(s)\n"
        "\tinode size = %ld bytes\n",
        sizeof(struct boogafs_inode));

end:
    free(block);
    return ret;
}

static int write_data_blocks(int fd, struct superblock *sb)
{
    /* allocate a block for the root directory */
    struct boogafs_dir_block *dblock = malloc(sizeof(struct boogafs_dir_block));
    if (!dblock)
        return -1;
    memset(dblock, 0, sizeof(struct boogafs_dir_block));

    /* first entry: . */
    dblock->files[0].inode = 0;
    strncpy(dblock->files[0].name, ".", BOOGAFS_FILENAME_LEN);

    /* second entry: .. */
    dblock->files[1].inode = 1000; /* it is not unknown at the time of creating the image, use 1000 temporarily */
    strncpy(dblock->files[1].name, "..", BOOGAFS_FILENAME_LEN);

    int ret = write(fd, dblock, sizeof(struct boogafs_dir_block));
    if (ret != BOOGAFS_BLOCK_SIZE) {
        ret = -1;
        goto end;
    }

    ret = 0;

    printf("data blocks: wrote 1 block(s): two entries for the root directory\n");

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

    /* Check if image is large enough */
    long int min_size = 6 * BOOGAFS_BLOCK_SIZE; /* the minimum size of our file system is 6 blocks, i.e., 6*4KB = 24KB */
    if (stat_buf.st_size < min_size) {
        fprintf(stderr, "File is not large enough (size=%ld, min size=%ld)\n",
                stat_buf.st_size, min_size);
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

    /* Write inode store blocks (from block 1) */
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
