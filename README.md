# Overview

In this assignment, we will write a Linux kernel module called audi. This module will serve as a file system whose layout matches 100% with the very simple file system as presented in the book chapter [File System Implementation](https://pages.cs.wisc.edu/~remzi/OSTEP/file-implementation.pdf). You should still use the cs452 VM (username:cs452, password: cs452) which you used for your tesla, lexus, infiniti, and toyota, as loading and unloading the kernel module requires the root privilege.

## Learning Objectives

- Understanding how file systems are organized.
- Learning how to write a simple file system in a Linux system.

## Important Notes

You MUST build against the kernel version (3.10.0-1160.el7.x86_64), which is the default version of the kernel installed on the cs452 VM.

## Book References

You are recommended to read this book chapter carefully:

Operating Systems: Three Easy Pieces: [File System Implementation](https://pages.cs.wisc.edu/~remzi/OSTEP/file-implementation.pdf).

## Background

### Directories vs Files

Directories are considered a special type of files. In the file system you are going to implement, there are only two types of files: regular files, and directories. Every time a file is created, we allocate one data block, which is 4KB, to this file. Thus each file can store at most 4KB data. Every time a directory is created, we also allocate one data block to this directory. This data block does not store the directory's data, because directory itself does not have any data, rather, we use this data block to store the directory's dentry table. Read the README file of assignment 1 (i.e., [tesla](https://github.com/jidongbsu/cs452-system-call)) to refresh your memory on what directory entries (or dentries for short) are. In this assignment, we only care about the dentry's inode number and the file/directory's name.

### Links

- When a file is created, by default its link count is 1. If one creates a soft or hard link to this file, its link count will be incremented by 1. In this assignment, we do not consider links to files.
- When a directory is created, by default its link count is 2: for a directory, the link count means how many sub-directories the directory has. A new directory by default has two sub-directories: "." and "..". Here, "." represents the current directory, ".." represents the parent directory. **Note**: a directory which only has these two sub-directories, is still called an empty directory - keep this in mind as you will use this fact when implementing one of the required functions in this assignment.

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

### Directory Entries (struct audi_dir_entry vs struct dentry)


### struct audi_inode vs struct inode

# Specification

The provided starter code implements a very simple file system whose layout matches 100% with the example presented in the book chapter, but it currently does not support any of these operations: file creation, directory creation, directory list, file deletion, and directory deletion. In this assignment, you will extend this very simple file system so as to support these operations.

## The Starter Code

The starter code looks like this:

```console
[cs452@localhost cs452-file-system]$ ls
audi.h  audi_main.c  bitmap.h  dir.c  file.c  inode.c  Makefile  mkfs.c  README.md  super.c  test
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
strlen(dentry->d_name.name)
```

- The longest file name we support is 60 bytes. Thus we define *AUDI_FILENAME_LEN* in audi.h:

```c
#define AUDI_FILENAME_LEN 60
```

- Each directory can have at most 64 files (including sub-directories). Thus we define *AUDI_MAX_SUBFILES* in audi.h.

```c
#define AUDI_MAX_SUBFILES 64
```

## Implementation - *create*()

The *create*() function gets called when the user tries to create a file. The function has the following prototype:
```c
static int audi_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl);
```

The first argument *dir* represents the inode of the parent directory; the second argument *dentry* represents the dentry of the file/directory that the user wants to create; the third argument *mode* determines if the user wants to create a file or a directory; the fourth argument will not be used in this assignment.

You can follow these steps to implement *create*():

1. if the new file's filename length is larger than AUDI_FILENAME_LEN, return -ENAMETOOLONG;
2. if the parent directory is already full, return -EMLINK - indicating "too many links"; 
3. if not the above two cases, then call *audi_new_inode*() to create an new inode, which will allocate a new inode and a new block. You can call it like this:

```c
    struct inode *inode;
    /* get a new free inode, and it is initialized in this *audi_new_inode*() function. */
    inode = audi_new_inode(dir, mode);
```

4. call *memset*() to zero out this new block - so that data belonging to other files/directories (which used this block before) do not get leaked.
5. insert the dentry representing the new file/directory into the end of the parent directory's dentry table.
6. call *mark_inode_dirty*() to mark this inode as dirty so that the kernel will put the inode on the superblock's dirty list and write it into the disk. this function, defined in the kernel (in include/linux/fs.h), has the following prototype:

```c
void mark_inode_dirty(struct inode *inode);
```
7. update the parent directory's last modified time and last accessed time to current time, you can do it like this:

```c
    dir->i_mtime = dir->i_atime = CURRENT_TIME;
```

8. call *inc_nlink*() to increment the parent directory's link count, if the newly created item is a directory (as opposed to a file). You can do it like this:

```c
    if (S_ISDIR(mode))
        inc_nlink(dir);
```

9. call *mark_inode_dirty*() to mark the parent's inode as dirty so that the kernel will put the parent's inode on the superblock's dirty list and write it into the disk.
10. call *d_instantiate*() to fill in the inode (the newly created inode, not the parent's inode) information for a dentry. this function, defined in the kernel (in fs/dcache.c), has the following prototype:
```c
void d_instantiate(struct dentry *, struct inode *);
```
11. you can now return 0.

## Implementation - *lookup*()

The *lookup*() function gets called when the user tries to list a file (e.g. ls -l a.txt). The function has the following prototype:
```c
static struct dentry *audi_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags);
```

The first argument *dir* represents the inode of the parent directory; the second argument *dentry* represents the dentry of the file/directory that the user wants to list; the third argument will not be used in this assignment.

If the chosen file exists in the parent directory, the *lookup*() function is expected to fill in *dentry* with necessary information (i.e., information contained in the inode of the file) that can be displayed to the user. If the chosen file does not exist in the parent directory, the *lookup*() function is expected to fill in *dentry* with NULL.

You can follow these steps to implement *lookup*():

1. if the chosen file's filename length is larger than AUDI_FILENAME_LEN, return -ENAMETOOLONG;
2. search the *dentry* in the parent directory's dentry table. if found, call *audi_iget*() which returns the corresponding inode. *audi_iget*() has the following prototype:

```c
struct inode *audi_iget(struct super_block *sb, unsigned long ino);
```

The second argument here is the inode number.

3. update the parent directory's last accessed time to current time. (refer to the *create*() implementation section to see how to update this).
4. call *mark_inode_dirty*() to mark the parent's inode as dirty so that the kernel will put the parent's inode on the superblock's dirty list and write it into the disk.
5. call *d_add*() to fill in the dentry with the inode's information. This function has the following prototype:
```c
void d_add(struct dentry *entry, struct inode *inode);
```

Note, if in step 2, the result was not found, then here you should pass *NULL* as the second argument to *d_add()*.

6. you can now return NULL. The return value is important to the user, what they need is now at the memory address pointed to by *dentry*, which is the second argument of this *lookup*() function.


## Implementation - *unlink*()

The *unlink*() function gets called when the user tries to delete a file (e.g., rm -f a.txt). The function has the following prototype:
```c
static int audi_unlink(struct inode *dir, struct dentry *dentry);
```

The first argument *dir* represents the inode of the parent directory; the second argument *dentry* represents the dentry of the file/directory that the user wants to delete.

You can follow these steps to implement *unlink*():

1. search the *dentry* in the parent directory's dentry table. if found, call *memmove*() to move entries after this entry forward - like what you did in assignment 1 (i.e., [tesla](https://github.com/jidongbsu/cs452-system-call)). if not found, return -ENOENT, meaning no such entry.
2. call *memset*() to zero out the previous last entry - so that a future traverse does not count this one entry.
3. update the parent directory's last modified time and last accessed time to current time. (refer to the *create*() implementation section to see how to update these).
4. call *drop_nlink*() to decrement the parent directory's link count, if the newly deleted item is a directory (as opposed to a file). You can do it like this: You can do it like this:
```c
    if (S_ISDIR(inode->i_mode)) {
        drop_nlink(dir);
    }
```
5. call *mark_inode_dirty*() to mark the parent's inode as dirty so that the kernel will put the parent's inode on the superblock's dirty list and write it into the disk.
6. call *memset*() to zero out the data block belonging to the deleted file.
7. call *put_block*() so as to update the data bitmap to mark this data block is free. *put_block*(), defined in bitmap.h, has the following prototype:
```c
void put_block(struct audi_sb_info *sbi, uint32_t bno);
```

Here *bno* is the block number.

8. reset inode information (all to 0) and then call *mark_inode_dirty*() to mark this inode as dirty so that the kernel will put this inode on the superblock's dirty list and write it into the disk. You can do these like this:
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
9. call *put_inode*() so as to update the inode bitmap to mark this inode is free. *put_inode*(), defined in bitmap.h, has the following prototype:
```c
void put_inode(struct audi_sb_info *sbi, uint32_t ino);
```

Here *ino* is the inode number.

10. you can now return 0.

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

### After Implemention

to be added.

## Submission

Due: 23:59pm, May 3rd, 2022. Late submission will not be accepted/graded.

## Project Layout

All files necessary for compilation and testing need to be submitted, this includes source code files, header files, and Makefile. The structure of the submission folder should be the same as what was given to you.

## Grading Rubric (Undergraduate and Graduate)

- [10 pts] Compiling:
  - Each compiler warning will result in a 3 point deduction.
  - You are not allowed to suppress warnings.

- [70 pts] Functional requirements:
  - file creation works (touch).	/10
  - file deletion works (rm -f).	/10
  - directory creation works (mkdir).	/10
  - directory display works (ls -l).	/20
  - directory deletion works (rmdir).	/20

- [10 pts] Module can be installed and removed without crashing the system:
  - You won't get these points if your module doesn't implement any of the above functional requirements.

- [10 pts] Documentation:
  - README.md file: replace this current README.md with a new one using the template. Do not check in this current README.
  - You are required to fill in every section of the README template, missing 1 section will result in a 2-point deduction.
