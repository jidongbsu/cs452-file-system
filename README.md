# Overview

In this assignment, we will write a Linux kernel module called audi. This module will serve as a file system whose layout matches 100% with the very simple file system as presented in the book chapter [File System Implementation](https://pages.cs.wisc.edu/~remzi/OSTEP/file-implementation.pdf). You should still use the cs452 VM (username:cs452, password: cs452) which you used for your tesla, lexus, infiniti, and toyota, as loading and unloading the kernel module requires the root privilege.

## Learning Objectives

- Understanding how file systems are organized on the disk.
- Understanding how file system data flows from disk to memory, and how file system data flows from memory to disk.
- Learning how to write a simple file system in a Linux system.

## Important Notes

You MUST build against the kernel version (3.10.0-1160.el7.x86_64), which is the default version of the kernel installed on the cs452 VM.

## Book References

You are recommended to read this book chapter carefully:

Operating Systems: Three Easy Pieces: [File System Implementation](https://pages.cs.wisc.edu/~remzi/OSTEP/file-implementation.pdf).

## Background

### The Linux Virtual File System (VFS) Layer

The Linux kernel supports many different file systems, thus it defines a generic interface layer called the virtual file system (VFS) layer. (credit: this picture is from the book "Managing RAID on Linux"by Derek Vadala.)

![alt text](vfs.png "VFS") 

Each file system calls *register_filesystem*() to register itself into the VFS. After registration, take the ext2 file system for example, when applications call *read*(), this *read*() will then call the system call function *sys_read*(), which will then call the VFS layer read function *vfs_read*(), which will then call the ext2 layer read function *do_sync_read*().

### Directories vs Files

Directories are considered a special type of files. In the file system you are going to implement, there are only two types of files: regular files, and directories. Every time a file is created, we allocate one data block, which is 4KB, to this file. When all 4KB are consumed, we do not allow the file size to grow. Thus each file in this file system can store at most 4KB data. Every time a directory is created, we also allocate one data block to this directory. This data block does not store the directory's data, because the directory itself does not have any data, rather, we use this data block to store the directory's dentry table. Read the README file of assignment 1 (i.e., [tesla](https://github.com/jidongbsu/cs452-system-call)) to refresh your memory on what dentries (short for directory entries) are. Each dentry contains multiple fields, but in this assignment, only two fields are relevant to us: the dentry's inode number and the file/directory's name.

### Links

- When a regular file is created, by default its link count is 1. If one creates a soft or hard link to this file, its link count will be incremented by 1. In this file system, we do not support creating links to files. Thus, a regular file in our file system will always have a link count of 1.
- When a directory is created, by default its link count is 2: for a directory, the link count means how many sub-directories the directory has. A new directory by default has two sub-directories: "." and "..". Here, "." represents the current directory, ".." represents the parent directory. **Note**: a directory which only contains these two sub-directories, is still called an empty directory - keep this in mind as you will use this fact when implementing one of the required functions in this assignment.

In the output of *ls -l* or *ls -la*, the second column is the link counts. As can be seen from the example below, files have a link count of 1. The directory *test* has a link count of 2, because it only has "." and "..", plus a regular file called .gitkeep. The directory *cs452-file-system* has a link count of 4, because it has 4 sub-directories: ., .., test, and .git. Creating files inside a directory does not affect the directory's link count.

```console
[cs452@localhost cs452-file-system]$ ls -l
total 328
-rw-rw-r-- 1 cs452 cs452   3917 Apr 17 03:16 audi.h
-rw-rw-r-- 1 cs452 cs452   2804 Apr 17 03:16 audi_main.c
-rw-rw-r-- 1 cs452 cs452   2578 Apr 17 04:06 bitmap.h
-rw-rw-r-- 1 cs452 cs452   3662 Apr 17 04:07 dir.c
-rw-rw-r-- 1 cs452 cs452   4158 Apr 17 03:16 file.c
-rw-rw-r-- 1 cs452 cs452   8103 Apr 17 03:52 inode.c
-rw-rw-r-- 1 cs452 cs452    817 Apr 17 03:16 Makefile
-rw-rw-r-- 1 cs452 cs452   8842 Apr 17 04:07 mkfs.c
-rw-rw-r-- 1 cs452 cs452  11475 Apr 17 19:16 README.md
-rw-rw-r-- 1 cs452 cs452  11606 Apr 17 03:16 super.c
drwxrwxr-x 2 cs452 cs452     22 Apr 16 01:12 test
-rw-rw-r-- 1 cs452 cs452 262144 Apr 17 16:03 test.img
[cs452@localhost cs452-file-system]$ ls -la
total 332
drwxrwxr-x 4 cs452 cs452    199 Apr 17 19:26 .
drwxrwxr-x 4 cs452 cs452     54 Apr 16 00:54 ..
-rw-rw-r-- 1 cs452 cs452   3917 Apr 17 03:16 audi.h
-rw-rw-r-- 1 cs452 cs452   2804 Apr 17 03:16 audi_main.c
-rw-rw-r-- 1 cs452 cs452   2578 Apr 17 04:06 bitmap.h
-rw-rw-r-- 1 cs452 cs452   3662 Apr 17 04:07 dir.c
-rw-rw-r-- 1 cs452 cs452   4158 Apr 17 03:16 file.c
drwxrwxr-x 8 cs452 cs452    220 Apr 17 19:14 .git
-rw-rw-r-- 1 cs452 cs452   8103 Apr 17 03:52 inode.c
-rw-rw-r-- 1 cs452 cs452    817 Apr 17 03:16 Makefile
-rw-rw-r-- 1 cs452 cs452   8842 Apr 17 04:07 mkfs.c
-rw-rw-r-- 1 cs452 cs452  12914 Apr 17 19:26 README.md
-rw-rw-r-- 1 cs452 cs452  11606 Apr 17 03:16 super.c
drwxrwxr-x 2 cs452 cs452     22 Apr 16 01:12 test
-rw-rw-r-- 1 cs452 cs452 262144 Apr 17 16:03 test.img
[cs452@localhost cs452-file-system]$ ls -l test
total 0
[cs452@localhost cs452-file-system]$ ls -la test
total 0
drwxrwxr-x 2 cs452 cs452  22 Apr 16 01:12 .
drwxrwxr-x 4 cs452 cs452 199 Apr 17 19:25 ..
-rw-rw-r-- 1 cs452 cs452   0 Apr 16 01:12 .gitkeep
```

# Specification

The provided starter code implements a very simple file system whose layout matches 100% with the example presented in the book chapter, but it currently does not support any of these operations: file creation, directory creation, directory list, file deletion, and directory deletion. In this assignment, you will extend this very simple file system so as to support these operations.

## The Starter Code

The starter code looks like this:

```console
[cs452@localhost cs452-file-system]$ ls
audi.h  audi_main.c  bitmap.h  dir.c  file.c  inode.c  Makefile  mkfs.c  README.md  super.c  test  test-audi.sh
```

You will be completing the inode.c file.

## Functions You Need to Implement

Here are the prototypes of the functions that you need to implement in inode.c.

```c
static int audi_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl);
static struct dentry *audi_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags);
static int audi_unlink(struct inode *dir, struct dentry *dentry);
static int audi_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);
static int audi_rmdir(struct inode *dir, struct dentry *dentry);
```

In the remaining of this README file, we will refer to these functions as the *create*(), *lookup*(), *unlink*(), *mkdir*(), *rmdir*(), respectively.

## Predefined Data Structures and Global Variables

- *struct audi_dir_entry* vs *struct dentry*

The VFS layer defines a generic data structure called *struct dentry*. Each instance of *struct dentry* represents one dentry.

Our file system also defines its own directory entry data structure, which is called *struct audi_dir_entry*. Each instance of *struct audi_dir_entry* also represents one dentry. However, we store information represented by *struct dentry* in memory only, but we store information represented by *struct audi_dir_entry* actually on the disk.

```c
struct audi_dir_entry {
    uint32_t inode; /* inode number */
    char name[AUDI_FILENAME_LEN];   /* file name, up to AUDI_FILENAME_LEN */
};

struct audi_dir_block {
    struct audi_dir_entry entries[AUDI_MAX_SUBFILES];
};

```

Each instance of *struct audi_dir_block* is one data block (i.e., 4KB) which stores the dentry table for a directory. The dentry table has at most 64 entries, in other words, we allow each directory to have at most 64 files (including subdirectories).

- The Linux kernel defines *struct dentry* in include/linux/dcache.h:

```c
struct dentry {
        /* RCU lookup touched fields */
        unsigned int d_flags;           /* protected by d_lock */
        seqcount_t d_seq;               /* per dentry seqlock */
        struct hlist_bl_node d_hash;    /* lookup hash list */
        struct dentry *d_parent;        /* parent directory */
        struct qstr d_name;
        struct inode *d_inode;          /* Where the name belongs to - NULL is
                                         * negative */
        unsigned char d_iname[DNAME_INLINE_LEN];        /* small names */

        /* Ref lookup also touches following */
        struct lockref d_lockref;       /* per-dentry lock and refcount */
        const struct dentry_operations *d_op;
        struct super_block *d_sb;       /* The root of the dentry tree */
        unsigned long d_time;           /* used by d_revalidate */
        void *d_fsdata;                 /* fs-specific data */

        struct list_head d_lru;         /* LRU list */
        /*
         * d_child and d_rcu can share memory
         */
        union {
                struct list_head d_child;       /* child of parent list */
                struct rcu_head d_rcu;
        } d_u;
        struct list_head d_subdirs;     /* our children */
        struct hlist_node d_alias;      /* inode alias list */
};
```

Among its fields, *struct qstr d_name* is the most relevant field, and *struct qstr* is also defined in include/linux/dcache.h:

```c
struct qstr {
        union {
                struct {
                        HASH_LEN_DECLARE;
                };
                u64 hash_len;
        };
        const unsigned char *name;
};
```

Inside *struct qstr*, *name* stores the name of the file or the directory - which we are going to create or delete. Given that *dentry* is the second argument of all of the functions you are going to implement, in order to access its corresponding file (or directory) name, you can use *dentry->d_name.name*, for example, if you want to measure the length of the file (or directory) name, you can use:

```c
int len;
len=strlen(dentry->d_name.name);
```

- *struct audi_inode* vs *struct inode*

The book chapter says:"the file system has to track information about each file. This information is a key piece of metadata, and tracks things like which data blocks (in the data region) comprise a file, the size of the file, its owner and access rights, access and modify times, and other similar kinds of information. To store this information, file systems usually have a structure called an **inode**."

The Linux VFS layer defines a generic data structure called *struct inode*. Each instance of *struct inode* represents one file (or one directory). This structure represents metadata information that is stored in memory.

Our file system also defines its own inode data structure, which is called *struct audi_inode*. Each instance of *struct audi_inode* also represents one file (or one directory). This data structure represents the metadata information that is actually stored on the disk. See **the inode table** in the book chapter. Just like the example in the book chapter, each *struct audi_inode* is 256 bytes.

```c
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
    char padding [220]; /* add padding so as to make this match with the one described in the book chapter: 256 bytes per inode. */
};
```

Note that *struct audi_inode* contains a field called *data_block*, which tells us the data block for the file (or directory) which is represented by the inode. However, *struct inode* does not have such a field. The VFS layer only knows *struct inode*, but it does not know *struct audi_inode*, oftentimes, when a *struct inode* type pointer is passed from the VFS layer to our file system, we want to find out the data block that is associated with this inode. To achieve this, a helper macro is provided:

```c
#define AUDI_INODE(inode) \
    (container_of(inode, struct audi_inode_info, vfs_inode))
```

When given an *struct inode* type pointer, for example, let's say we have *struct inode * inode*, then the following code returns the data block,
```c
int block_num;
block_num = AUDI_INODE(inode)->data_block;
```

which is an integer.

- *struct super_block* vs *struct audi_sb_info*

The VFS layers defines a generic data structure called *struct super_block*. Each instance of *struct super_block* represents the super block of a file system. Our file system, as showed in the book chapter, only has one super block. When given a *struct inode* type pointer, this is how you can get the super block:

```c
struct inode *inode;
struct super_block *sb = inode->i_sb;
```

Our file system also defines its own super block data structure, which is called *struct audi_sb_info*. Each instance of *struct audi_sb_info* also represents the super block of our file system. This data structure defines the format of the information we actually stored on the disk's super block. *struct audi_sb_info* is defined in audi.h:

```c
/* super block data, follow ext2 and ext4 naming convention. 
 * as of now, this structure is 20 bytes. */
struct audi_sb_info {
    uint32_t s_magic; /* Magic signature */
    uint32_t s_inodes_count; /* Total inodes count */
    uint32_t s_blocks_count; /* Total blocks count */
    uint32_t s_free_inodes_count; /* Free inodes count */
    uint32_t s_free_blocks_count; /* Free blocks count */
};
```

When given a *struct super_block* type pointer, for example, let's say we have *struct super_block sb*, then the following code returns its corresponding *struct audi_sb_info* type pointer.

```c
struct audi_sb_info *sbi = AUDI_SB(sb);	// you will need a line like this in your unlink() function.
```

Here, *AUDI_SB* is a helper macro defined in audi.h:

```c
#define AUDI_SB(sb) (sb->s_fs_info)
```

- The longest file name we support is 60 characters. Thus we define *AUDI_FILENAME_LEN* in audi.h:

```c
#define AUDI_FILENAME_LEN 60
```

- Each directory can have at most 64 files (including sub-directories). Thus we define *AUDI_MAX_SUBFILES* in audi.h.

```c
#define AUDI_MAX_SUBFILES 64
```

## Related Kernel APIs

- The *sb_bread*() function. This function allows you to read a data block from the disk:

```c
struct buffer_head *bh;
struct audi_dir_block *dir_block;
bh = sb_bread(sb, 40);	// this is just an example, let's say you want to read block 40.
dir_block = (struct audi_dir_block *) bh->b_data;
```

after these lines, now the data block's content is stored at the address pointed to by *dir_block*. You may want to change some part of this block, and after the change, if you want the change to be flushed back into the disk, call these two functions:

```c
mark_buffer_dirty(bh);
brelse(bh);
```

Here, *mark_buffer_dirty*() will mark the data is dirty and therefore will soon be written back to disk; *brelse*() will release the memory, after this line, you can't access *dir_block* anymore.

- The string operation functions. You may want to use:
  - strlen()
  - strncmp()
  - strncpy()

They are all available in kernel code - the Linux kernel re-implements them in the kernel space. You do not need to include any extra header files to use these functions. Use them in the kernel space the same way as you normally would in applications.

- The memory operation functions. You may want to use:
  - memset()
  - memmove()

The remaining sections of this README will tell you when you want to use these two functions.

## Implementation - *create*()

The *create*() function gets called when the user tries to create a file. The function has the following prototype:
```c
static int audi_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl);
```

The first argument *dir* represents the inode of the parent directory; the second argument *dentry* represents the dentry of the file/directory that the user wants to create; the third argument *mode* determines if the user wants to create a file or a directory; the fourth argument will not be used in this assignment.

You can follow these steps to implement *create*():

1. if the new file's filename length is larger than AUDI_FILENAME_LEN, return -ENAMETOOLONG;
2. if the parent directory is already full, return -EMLINK - indicating "too many links"; 
3. if not the above two cases, then call *audi_new_inode*() to create a new inode, which will allocate a new inode and a new block. You can call it like this:

```c
    struct inode *inode;
    /* get a new free inode, and it is initialized in this *audi_new_inode*() function. */
    inode = audi_new_inode(dir, mode);
```

**Note**: the *struct inode* has a field called *unsigned long i_ino*, that is the inode number. You will need this inode number in this right next step.

4. insert the dentry representing the new file/directory into the end of the parent directory's dentry table.
5. call *mark_inode_dirty*() to mark this inode as dirty so that the kernel will put the inode on the superblock's dirty list and write it into the disk. this function, defined in the kernel (in include/linux/fs.h), has the following prototype:

```c
void mark_inode_dirty(struct inode *inode);
```
6. update the parent directory's last modified time and last accessed time to current time, you can do it like this:

```c
    dir->i_mtime = dir->i_atime = CURRENT_TIME;
```

7. call *inc_nlink*() to increment the parent directory's link count, if the newly created item is a directory (as opposed to a file). You can do it like this:

```c
    if (S_ISDIR(mode))
        inc_nlink(dir);
```

8. call *mark_inode_dirty*() to mark the parent's inode as dirty so that the kernel will put the parent's inode on the superblock's dirty list and write it into the disk.
9. call *d_instantiate*() to fill in the inode (the newly created inode, not the parent's inode) information for a dentry. this function, defined in the kernel (in fs/dcache.c), has the following prototype:
```c
void d_instantiate(struct dentry *, struct inode *);
```
as its name suggests, this function instantiates a dentry, which means it sets up several fields of the *struct dentry* pointer. for example, it does this:

```c
dentry->d_inode = inode;
dentry->d_flags |= DCACHE_NEED_AUTOMOUNT;
```

10. you can now return 0.

## Implementation - *lookup*()

The *lookup*() function gets called when the user tries to list a file (e.g. ls -l a.txt). The function has the following prototype:
```c
static struct dentry *audi_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags);
```

The first argument *dir* represents the inode of the parent directory; the second argument *dentry* represents the dentry of the file/directory that the user wants to list; the third argument will not be used in this assignment.

If the chosen file exists in the parent directory, the *lookup*() function is expected to fill in *dentry* with necessary information (i.e., information contained in the inode of the file) that can be displayed to the user. If the chosen file does not exist in the parent directory, the *lookup*() function is expected to fill in *dentry* with NULL.

You can follow these steps to implement *lookup*():

1. if the chosen file's filename length is larger than AUDI_FILENAME_LEN, return ERR_PTR(-ENAMETOOLONG); **Note**: *lookup*() is expected to return a pointer, thus when returning an error code, we need to use *ERR_PTR*() to wrap the error code; in *create*(), *unlink*(), *rmdir*(), *mkdir*(), because they are expected to return an integer, we do not need to use *ERR_PTR*() wrap the error code.
2. search the *dentry* in the parent directory's dentry table. if found, call *audi_iget*() which returns the corresponding inode. *audi_iget*() has the following prototype:

```c
struct inode *audi_iget(struct super_block *sb, unsigned long ino);
```

The second argument here is the inode number. Note that the caller of *lookup*() does not know the inode number, which means at this moment you can not deduce the inode number from the dentry. Rather, you should find out the inode number from the parent directory's dentry table.

3. update the parent directory's last accessed time to current time. (refer to the *create*() implementation section to see how to update this).
4. call *mark_inode_dirty*() to mark the parent's inode as dirty so that the kernel will put the parent's inode on the superblock's dirty list and write it into the disk.
5. call *d_add*() to fill in the dentry with the inode's information. This function has the following prototype:
```c
void d_add(struct dentry *dentry, struct inode *inode);
```

Note, if in step 2, the result was not found, then here you should pass *NULL* as the second argument to *d_add*(). *d_add*() will call *d_instantiate*(), which, as explained above, will set up several fields of the *struct dentry* pointer. once again, for example, it does this:

```c
dentry->d_inode = inode;
dentry->d_flags |= DCACHE_NEED_AUTOMOUNT;
```

6. you can now return NULL. The return value is not important to the user, what they need is now at the memory address pointed to by *dentry*, which is the second argument of this *lookup*() function. If *dentry*'s *d_inode* is still NULL - this happens when we pass NULL as the second argument of *d_add*(), upper layer (i.e., the VFS layer) functions will return -ENOENT, and the command like "ls -l a.txt" will tell the user "No such file or directory".

## Implementation - *unlink*()

The *unlink*() function gets called when the user tries to delete a file (e.g., rm -f a.txt), or to delete a directory, e.g., rm -rf ddd: this command would call unlink() to delete files and call rmdir() to delete empty directories, rmdir() would then call unlink() to do the actual deletion. The function has the following prototype:
```c
static int audi_unlink(struct inode *dir, struct dentry *dentry);
```

The first argument *dir* represents the inode of the parent directory; the second argument *dentry* represents the dentry of the file/directory that the user wants to delete.

You can follow these steps to implement *unlink*():

1. given a *dentry*, we can get its inode like this:
```c
struct inode *inode = d_inode(dentry);
```

*d_inode*() is a helper function defined in the linux kernel:include/linux/dcache.h. This helper function just returns *dentry->d_inode* - read the above *create*() and *lookup*() section again and you will find out that *dentry->d_inode* should be pointing to the file's (or the directory's) *inode*, which is a *struct inode* type pointer.

2. search the *dentry* in the parent directory's dentry table. if found, call *memmove*() to move entries after this entry forward - like what you did in assignment 1 (i.e., [tesla](https://github.com/jidongbsu/cs452-system-call)). if not found, return -ENOENT, meaning no such entry.
3. call *memset*() to zero out the previous last entry - so that a future traverse does not count this one entry.
4. update the parent directory's last modified time and last accessed time to current time. (refer to the *create*() implementation section to see how to update these).
5. call *drop_nlink*() to decrement the parent directory's link count, if the newly deleted item is a directory (as opposed to a file). You can do it like this: You can do it like this:
```c
    if (S_ISDIR(inode->i_mode)) {
        drop_nlink(dir);
    }
```
6. call *mark_inode_dirty*() to mark the parent's inode as dirty so that the kernel will put the parent's inode on the superblock's dirty list and write it into the disk.
7. call *memset*() to zero out the data block belonging to the deleted file.
8. call *put_block*() so as to update the data bitmap to mark this data block is free. *put_block*(), defined in bitmap.h, has the following prototype:
```c
void put_block(struct audi_sb_info *sbi, uint32_t bno);
```

Here *bno* is the block number.

9. reset inode information (all to 0) and then call *mark_inode_dirty*() to mark this inode as dirty so that the kernel will put this inode on the superblock's dirty list and write it into the disk. You can do these like this:
```c
    AUDI_INODE(inode)->data_block = 0;
    inode->i_size = 0;
    i_uid_write(inode, 0);
    i_gid_write(inode, 0);
    inode->i_mode = 0;
    inode->i_ctime.tv_sec = inode->i_mtime.tv_sec = inode->i_atime.tv_sec = 0;
    drop_nlink(inode); /* drop nlink again? */
    mark_inode_dirty(inode);
```
10. call *put_inode*() so as to update the inode bitmap to mark this inode is free. *put_inode*(), defined in bitmap.h, has the following prototype:
```c
void put_inode(struct audi_sb_info *sbi, uint32_t ino);
```

Here *ino* is the inode number.

11. you can now return 0.

## Implementation - *rmdir*()

The *rmdir*() function gets called when the user tries to delete a directory (e.g., rmdir bbb). The function has the following prototype:
```c
static int audi_rmdir(struct inode *dir, struct dentry *dentry);
```

The first argument *dir* represents the inode of the parent directory; the second argument *dentry* represents the dentry of the directory that the user wants to delete. *rmdir*() will fail if the directory to be deleted is not empty.

You can follow these steps to implement *unlink*():
1. given a *dentry*, we can get its inode like this:
```c
struct inode *inode = d_inode(dentry);
```
2. if the inode's *i_nlink* field is greater than 2, we know the directory is not empty, return -ENOTEMPTY;
3. given a directory's inode, we can get its data block like this:
```c
int bno;
bno = AUDI_INODE(inode)->data_block;
```
4. read the directory's data block, which contains a dentry table, traverse this dentry table, if it contains more than 2 entries, we know its not empty, still return -ENOTEMPTY.
5. call *audi_unlink*() and return whatever *audi_unlink*() returns;

## Implementation - *mkdir*()

The *mkdir*() function gets called when the user tries to create a directory (e.g., mkdir ccc). The function has the following prototype:
```c
static int audi_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);
```

your *mkdir*() can just call *audi_create*() like this, and then returns whatever *audi_create*() returns.

```c
audi_create(dir, dentry, mode | S_IFDIR, 0);
```

## Debugging

Note that the kernel print messages will not show on the screen. The messages are, however, logged in the file /var/log/messages. You can open another terminal and watch the output to the system messages file with the command:

```console
# sudo tail -f /var/log/messages
```

Alternatively, you can use the command:

```console
# sudo dmesg --follow
```

## Testing

Before the test, run *make* to compile the files, which will generate the kernel module *audi.ko*, as well as an executable file *mkfs.audi*.

To test the file system, 

- we first create a file system image, which is a zeroed file:

```console
[cs452@localhost cs452-file-system]$ dd if=/dev/zero of=test.img bs=4K count=64
64+0 records in
64+0 records out
262144 bytes (262 kB) copied, 0.000861149 s, 304 MB/s
```
As described in the book chapter, our file system has 64 blocks, and each block is 4KB.

- we then create the file system layout (so that the above file system image will have the same layout as the chapter's **vsfs** example):

```console
[cs452@localhost cs452-file-system]$ ./mkfs.audi test.img 
superblock: (4096 bytes )
	magic=0x12345678
	s_blocks_count=64
	s_inodes_count=80
	s_free_inodes_count=78
	s_free_blocks_count=55
inode bitmap: wrote 1 block; initial inode bitmap is: 0xa000000000000000
data bitmap: wrote 1 block; initial data bitmap is: 0xff80000000000000
inode table: wrote 5 blocks
	inode size = 256 bytes
data blocks: wrote 1 block: two entries ("." and "..") for the root directory
```

- next we install the kernel module **audi.ko** with the *insmod* command.

```console
[cs452@localhost test]$ sudo insmod audi.ko
[sudo] password for cs452: 
```

- we then mount the file system onto the **test** folder - this folder is already included in the starter code.

```console
[cs452@localhost cs452-file-system]$ sudo mount -o loop -t audi test.img test
```

After the above step, you can now perform various supported file system operations inside the test folder.

- after the test, we can unmount the file system with the *umount* command (yes, the command is called *umount*, not *unmount*), and then we can remove the kernel module **audi.ko** with the *rmmod* command.

```console
[cs452@localhost cs452-file-system]$ sudo umount test
[cs452@localhost cs452-file-system]$ sudo rmmod audi
```

## Expected Results

### Current State

Before you implement anything, if you compile the starter code, load the kernel module, mount the file system, and then you run the following commands:

```console
[cs452@localhost cs452-file-system]$ cd test/
[cs452@localhost test]$ ls
[cs452@localhost test]$ ls -a
.  ..
[cs452@localhost test]$ touch abc
touch: cannot touch ‘abc’: No such file or directory
[cs452@localhost test]$ mkdir bbc
[cs452@localhost test]$ ls
[cs452@localhost test]$ ls -la
total 4
drwxr-xr-x 2 cs452 cs452 4096 Apr 17 16:01 .
drwxrwxr-x 5 cs452 cs452 4096 Apr 17 16:01 ..
```

As you can see, you can't create a file, or create a directory - the *mkdir* command does not fail, but the *ls* command does not show the created directory. After the implementation, you should be able to create files and directories, and show them via the *ls* command.

### After Implementation

- The following tests show that file creation, directory creation, and directory list works.

```console
[cs452@localhost cs452-file-system]$ cd test/
[cs452@localhost test]$ ls
[cs452@localhost test]$ touch abc
[cs452@localhost test]$ touch bbc
[cs452@localhost test]$ mkdir cdc
[cs452@localhost test]$ ls -l
total 0
-rw-rw-r-- 1 cs452 cs452    0 Apr 18 05:34 abc
-rw-rw-r-- 1 cs452 cs452    0 Apr 18 05:34 bbc
drwxrwxr-x 2 cs452 cs452 4096 Apr 18 05:34 cdc
```

- The following tests show that file creation then fails when the length of the filename is longer than 60 characters.

```console
[cs452@localhost test]$ touch mymomsaysthisfileistoolongwhydowecreateafilewithsuchalongname
touch: cannot touch ‘mymomsaysthisfileistoolongwhydowecreateafilewithsuchalongname’: File name too long
[cs452@localhost test]$ touch mymomsaysthisfileistoolongwhydowecreateafilewithsuchalongnamewhatiswrongwithyou
touch: cannot touch ‘mymomsaysthisfileistoolongwhydowecreateafilewithsuchalongnamewhatiswrongwithyou’: File name too long
[cs452@localhost test]$ touch eeff
[cs452@localhost test]$ ls -l
total 0
-rw-rw-r-- 1 cs452 cs452    0 Apr 18 05:34 abc
-rw-rw-r-- 1 cs452 cs452    0 Apr 18 05:34 bbc
drwxrwxr-x 2 cs452 cs452 4096 Apr 18 05:34 cdc
-rw-rw-r-- 1 cs452 cs452    0 Apr 18 05:38 eeff
```

- The following tests show that file deletion works with the *rm -f* command.

```console
[cs452@localhost test]$ ls -l
total 0
-rw-rw-r-- 1 cs452 cs452    0 Apr 18 05:34 abc
-rw-rw-r-- 1 cs452 cs452    0 Apr 18 05:34 bbc
drwxrwxr-x 2 cs452 cs452 4096 Apr 18 05:34 cdc
-rw-rw-r-- 1 cs452 cs452    0 Apr 18 05:38 eeff
[cs452@localhost test]$ rm -f abc
[cs452@localhost test]$ rm -f bbc
[cs452@localhost test]$ rm -f eeff
[cs452@localhost test]$ ls -l
total 0
drwxrwxr-x 2 cs452 cs452 4096 Apr 18 05:34 cdc
```

- The following tests show that directory deletion works with the *rmdir* command, if the directory is empty; when the directory is not empty, *rmdir* reports "Directory not empty".

```console
[cs452@localhost test]$ ls -l
total 0
drwxrwxr-x 2 cs452 cs452 4096 Apr 18 05:34 cdc
[cs452@localhost test]$ rmdir cdc
[cs452@localhost test]$ ls
[cs452@localhost test]$ mkdir ddd
[cs452@localhost test]$ cd ddd
[cs452@localhost ddd]$ touch lol
[cs452@localhost ddd]$ mkdir www
[cs452@localhost ddd]$ ls
lol  www
[cs452@localhost ddd]$ cd ..
[cs452@localhost test]$ ls
ddd
[cs452@localhost test]$ ls -lR
.:
total 0
drwxrwxr-x 3 cs452 cs452 4096 Apr 18 05:43 ddd

./ddd:
total 0
-rw-rw-r-- 1 cs452 cs452    0 Apr 18 05:43 lol
drwxrwxr-x 2 cs452 cs452 4096 Apr 18 05:43 www

./ddd/www:
total 0
[cs452@localhost test]$ rmdir ddd
rmdir: failed to remove ‘ddd’: Directory not empty
```
- The following tests show that even the non-empty directory can be deleted with *rm -rf* command.

```console
[cs452@localhost test]$ ls -l ddd
total 0
-rw-rw-r-- 1 cs452 cs452    0 Apr 18 06:05 lol
drwxrwxr-x 2 cs452 cs452 4096 Apr 18 06:05 www
[cs452@localhost test]$ rm ddd
rm: cannot remove ‘ddd’: Is a directory
[cs452@localhost test]$ rm -rf ddd
[cs452@localhost test]$ ls -l
total 0
```

### Testing Script

All the above tests can also be done automatically via a script, which is also provided in the starter code. The test script is *test-audi.sh*, you can run it like this (and are expected to get exactly the same results, except timestamps and numbers):

```console
[cs452@localhost cs452-file-system]$ ./test-audi.sh
run ls -la to show what we have at first:
total 8
drwxr-xr-x 2 cs452 cs452 4096 Apr 21 03:00 .
drwxrwxr-x 5 cs452 cs452 4096 Apr 21 03:01 ..

testing file creation with touch (abc and bbc) and directory creation with mkdir (cdc):
now we have:
.  ..  abc  bbc  cdc

testing long name file creation:
creating mymomsaysthisfileistoolongwhydowecreateafilewithsuchalongname:
touch: cannot touch ‘mymomsaysthisfileistoolongwhydowecreateafilewithsuchalongname’: File name too long

creating mymomsaysthisfileistoolongwhydowecreateafilewithsuchalongnamewhatiswrongwithyou:
touch: cannot touch ‘mymomsaysthisfileistoolongwhydowecreateafilewithsuchalongnamewhatiswrongwithyou’: File name too long

creating eeff
now we have:
.  ..  abc  bbc  cdc  eeff

testing file deletion:
deleting abc, bbc, and eeff
now we have:
.  ..  cdc

testing directory deletion:
deleting cdc
now we have:
.  ..

creating directory ddd with a subdirectory www, and a file lol in ddd:
now in ddd (ls -l ddd) we have:
total 0
-rw-rw-r-- 1 cs452 cs452    0 Apr 21 03:01 lol
drwxrwxr-x 2 cs452 cs452 4096 Apr 21 03:01 www

now in test (ls -l) we have:
total 0
drwxrwxr-x 3 cs452 cs452 4096 Apr 21 03:01 ddd

deleting ddd
rmdir: failed to remove ‘ddd’: Directory not empty

testing rm -rf to delete everything:
before deletion we have:
.  ..  ddd
after deletion we now have:
.  ..
```

### Special Tricks

One special way to debug this file system, is using the command *xxd*. If you run this following command,

```console
[cs452@localhost vsfs]$ xxd test.img | more
```

You will see the content of the file system on the disk image - test.img.

```console
0000000: 7856 3412 5000 0000 4000 0000 4e00 0000  xV4.P...@...N...
0000010: 3700 0000 0000 0000 0000 0000 0000 0000  7...............
0000020: 0000 0000 0000 0000 0000 0000 0000 0000  ................
0000030: 0000 0000 0000 0000 0000 0000 0000 0000  ................
0000040: 0000 0000 0000 0000 0000 0000 0000 0000  ................
0000050: 0000 0000 0000 0000 0000 0000 0000 0000  ................
0000060: 0000 0000 0000 0000 0000 0000 0000 0000  ................
0000070: 0000 0000 0000 0000 0000 0000 0000 0000  ................
0000080: 0000 0000 0000 0000 0000 0000 0000 0000  ................
0000090: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000a0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000b0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000c0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000d0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000e0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000f0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
...(omitted)
```

The above output shows the initial state of our file system - i.e., right after running the *./mkfs.audi test.img* command. In the output, each line is 16 bytes, and 0x100 is at byte 256, and 0x1000 is at byte 4KB - which is the size of one block. The above output shows the first 256 bytes of the file system, which also is the first 256 bytes of the first block. And as we know the first block of our file system is the super block. So let's try to understand the above output.

Once again, in audi.h, we define:

```c
/* super block data, follow ext2 and ext4 naming convention. 
 * as of now, this structure is 20 bytes. */
struct audi_sb_info {
    uint32_t s_magic; /* Magic signature */
    uint32_t s_inodes_count; /* Total inodes count */
    uint32_t s_blocks_count; /* Total blocks count */
    uint32_t s_free_inodes_count; /* Free inodes count */
    uint32_t s_free_blocks_count; /* Free blocks count */
};
```
And this *struct* defines our super block. In theory, we allocate one block for it, but we actually only use 20 bytes:

- the first four bytes, as we can see, are 7856 3412, are our magic number, each file system has a unique magic number, for our file system, we use 12345678 as our magic number, which is also defined in audi.h:

```c
#define AUDI_MAGIC 0x12345678
```
Note: want to know why 0x12345678 is stored as 7856 3412? do some research on big endian vs little endian.

- the next four bytes, 5000 0000: 0x50 is 80 (decimal), the reason for this is we have 80 inodes in total - see the book chapter.

- the next four bytes, 4000 0000: 0x40 is 64 (decimal), the reason for this is we have 64 blocks in total - again, see the book chapter.

- the next four bytes, 4e00 0000: 0x4e is 78 (decimal), the reason for this is, at the initial state of this file system, we reserve inode 2 for the root inode, and inode 0 in Linux is invalid. Thus, we have 78 free inodes available. (80 in total, 1 reserved, 1 invalid, thus 78 available.)

- the last four bytes, 3700 0000: 0x37 is 55 (decimal), the reason for this is, at the initial state of this file system, we reserve 1 block for the super block, 1 block for the inode bitmap, 1 block for the data bitmap, 5 blocks for the inode table, and 1 block for the root directory's data block. Thus we have (64 - 1 - 1 - 1 - 5 - 1) = 55 free blocks available.

If we keep navigating the output of the above *xxd* command, we can move on to examine block 8 - which is the root directory's data block:

```console
0008000: 0200 0000 2e00 0000 0000 0000 0000 0000  ................
0008010: 0000 0000 0000 0000 0000 0000 0000 0000  ................
0008020: 0000 0000 0000 0000 0000 0000 0000 0000  ................
0008030: 0000 0000 0000 0000 0000 0000 0000 0000  ................
0008040: ffff ffff 2e2e 0000 0000 0000 0000 0000  ................
0008050: 0000 0000 0000 0000 0000 0000 0000 0000  ................
0008060: 0000 0000 0000 0000 0000 0000 0000 0000  ................
0008070: 0000 0000 0000 0000 0000 0000 0000 0000  ................
0008080: 0000 0000 0000 0000 0000 0000 0000 0000  ................
0008090: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00080a0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00080b0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00080c0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00080d0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00080e0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00080f0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
0008100: 0000 0000 0000 0000 0000 0000 0000 0000  ................
```

As you can see, at first, the data block of the root directory contains two dentries. If we refer back to the definition of *struct audi_dir_entry*,


```c
/* structure of a directory entry, unliked the struct ext2_dir_entry, 
 * we do not store the length of this directory entry, or the name length. */
struct audi_dir_entry {
	uint32_t inode;	/* inode number */
	char name[AUDI_FILENAME_LEN];	/* file name, up to AUDI_FILENAME_LEN */
};
```

Each entry is 64 bytes.

- the first four bytes is the inode number, which for the first entry, which is ".", is 2, because we reserve inode 2 for the root inode.

- the next four bytes is the name of this entry, which is 0x2e, consulting with the ASCII table, 0x2e is ".".

- the first four bytes of the second entry, is once again the inode number, which is 0xffff ffff, which represents -1 - and that's how we represent the root directory's parent directory, which is not important to us.

- the next four bytes of the second entry, is 0x2e2e, which is "..".

If at this moment, we create a file or a directory under the root directory, we will add one more entry at the address of 0x0008080, which is 64 bytes away from the previous entry, because each entry is 64 bytes.

Therefore, throughout the development process of this assignment, you can always use the *xxd* command to monitor whether or not you are writing the right content to the right address.

## Submission

Due: 23:59pm, December 6th, 2022. Late submission will not be accepted/graded.

## Project Layout

All files necessary for compilation and testing need to be submitted, this includes source code files, header files, Makefile, and the bash script. The structure of the submission folder should be the same as what was given to you.

## Grading Rubric (Undergraduate and Graduate)

- [10 pts] Compiling:
  - Each compiler warning will result in a 3 point deduction.
  - You are not allowed to suppress warnings.

- [70 pts] Functional requirements:
  - file creation works when the file name is shorter than 60 characters (touch).			/10
  - file creation reports "File name too long" when the file name is longer than 60 characters (touch).	/10
  - directory creation works (mkdir).									/10
  - directory list works (ls -l).									/10
  - file deletion works (rm -f).									/10
  - directory deletion works when the directory is empty (rmdir).					/10
  - directory deletion reports "Directory not empty" when the directory is not empty (rmdir).		/10

- [10 pts] Module can be installed and removed without crashing the system:
  - You won't get these points if your module doesn't implement any of the above functional requirements.

- [10 pts] Documentation:
  - README.md file (rename this current README file to README.orig and rename the README.template to README.md.)
  - You are required to fill in every section of the README template, missing 1 section will result in a 2-point deduction.
