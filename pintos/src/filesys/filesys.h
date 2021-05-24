#ifndef FILESYS_FILESYS_H
#define FILESYS_FILESYS_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "threads/synch.h"
#include "filesys/directory.h"

/* Sectors of system file inodes. */
#define FREE_MAP_SECTOR 0       /* Free map file inode sector. */

/* Block device that contains the file system. */
struct block *fs_device;
//static struct lock filesys_lock;

void filesys_init (bool format);
void filesys_done (void);

bool filesys_create (const char *absolute_path, off_t initial_size);
//bool filesys_create_with_cwd (struct dir *cwd, const char *full_path, off_t initial_size);

struct file *filesys_open (const char *absolute_path);
//struct file *filesys_open_with_cwd (struct dir *cwd, const char *full_path);

bool filesys_remove (const char *absolute_path);
//bool filesys_remove_with_cwd (struct dir *cwd, const char *full_path);

bool filesys_mkdir (const char *full_path);
struct dir *filesys_open_dir (const char *full_path);

char* get_absolute_path(char* file_name);

#endif /* filesys/filesys.h */
