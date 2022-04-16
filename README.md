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

## Functions You Need to Implement

Here are the prototypes of the functions that you need to implement in toyota.c.

```c
static int toyota_open (struct inode *inode, struct file *filp);
static int toyota_release (struct inode *inode, struct file *filp);
static ssize_t toyota_read (struct file *filp, char *buf, size_t count, loff_t *f_pos);
static ssize_t toyota_write (struct file *filp, const char *buf, size_t count, loff_t *f_pos);
static int __init toyota_init(void);
static void __exit toyota_exit(void);
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

To test the file system, 

- we first create a file system image, which is a zeroed file:

```console
[cs452@localhost cs452-device-driver]$ dd if=/dev/zero of=test.img bs=4K count=64
```
As described in the book chapter, our file system has 64 blocks, and each block is 4KB.

- we then mount the file system onto the **test** folder - this folder is already included in the starter code.

```console
[cs452@localhost cs452-device-driver]$ sudo mount -o loop -t audifs test.img test
```

After the above step, you can now perform various supported file system operations inside the test folder.

## Expected Results

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
