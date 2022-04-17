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

to be added.

# Specification

to be added.

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

## Predefined Data Structures and Global Variables

to be added.

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

As you can see, you can't create a file, nor create a directory - the *mkdir* command does not fail, but ls command does not show the created directory. After the implementation, you should be able to create files and directories, and show them via the *ls* command.

### After Implemention

to be added.

## Submission

Due: 23:59pm, May 3rd, 2022. Late submission will not be accepted/graded.

## Project Layout

All files necessary for compilation and testing need to be submitted, this includes source code files, header files, and Makefile. The structure of the submission folder should be the same as what was given to you.

## Grading Rubric (Undergraduate and Graduate)

- [10 pts] Compiling
  - Each compiler warning will result in a 3 point deduction.
  - You are not allowed to suppress warnings

- [70 pts] Main driver: supports read properly, writing (to device 1 and 2) acts like /dev/null, kill process writing to /dev/toyota3.
  - toyota-test1 produces expected results /10
  - toyota-test2 produces expected results /20
  - toyota-test3 produces expected results /20
  - toyota-test4 produces expected results /20

- [10 pts] Module can be installed and removed without crashing the system:
  - You won't get these points if your module doesn't implement any of the above functional requirements.

- [10 pts] Documentation:
  - README.md file: replace this current README.md with a new one using the template. Do not check in this current README.
  - You are required to fill in every section of the README template, missing 1 section will result in a 2-point deduction.
