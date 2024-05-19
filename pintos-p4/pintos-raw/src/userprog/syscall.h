#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "userprog/syscall.h"
#include "threads/vaddr.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "devices/input.h"

//a global lock for file related syscall
static struct lock file_lock;

//since pid and tid is 1-1 mapping, let pid = tid
typedef tid_t pid_t;

//file descriptor struct
struct file_descriptor 
{
  int fd;                          //for identifying file
  struct file *file;               //file struct
  struct list_elem elem;           //for storing file descriptor
  struct dir *dir;                 //for identifying direcotry
};
#define STDIN_FILENO  0  /*standard input fd number*/
#define STDOUT_FILENO 1  /*standard output fd number*/

void syscall_init (void);
void halt(void);
void exit(int status);
int wait (pid_t pid);
pid_t exec (const char *cmd_line);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
static int read (int fd, const void *buffer, unsigned size);
static int write (int handle, const void *usrc_, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);
bool chdir (const char *dir);
int mkdir (const char *dir);
bool readdir (int fd, char *name);
bool isdir (int fd);
int inumber (int fd);


#endif /* userprog/syscall.h */
