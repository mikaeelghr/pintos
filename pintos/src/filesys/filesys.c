#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/cache.h"

#define return_null_on_absolute_wrong_first_char(absolute_path) \
  if(absolute_path==NULL || absolute_path[0]!='/'){             \
    return NULL;                                                \
  }                                                             

/* Partition that contains the file system. */
struct block *fs_device;

char*
get_absolute_path(char* file_name)
{
  char * file_path = calloc(1,  strlen(file_name) + 2);
  file_path[0] = '/';
  memcpy(file_path + 1, file_name, strlen(file_name) + 1);
  return file_path;
}

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format)
{
  //lock_init(&filesys_lock);
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();
  cache_init ();

  if (format)
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void)
{
  cache_done (fs_device);
  free_map_close ();
}



/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *full_path, off_t initial_size)
{
  struct dir *dir = file_parent_dir_open_recursive (full_path);
  char* name = get_file_name(full_path);

  block_sector_t inode_sector = 0;
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size)
                  && dir_add (dir, name, inode_sector));
  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  dir_close (dir);
  return success;
}


/* Also accepts absolute paths */
bool
filesys_mkdir (const char *full_path)
{
  struct dir *dir = file_parent_dir_open_recursive (full_path);
  char* name = get_file_name(full_path);

  block_sector_t inode_sector = 0;
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && dir_create (inode_sector, 1)  // 1 parent with name ".."
                  && dir_add (dir, name, inode_sector));
  
  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  
  if (!success)
    goto done;

  struct dir *child_dir = dir_open(inode_open(inode_sector));
  if (child_dir == NULL)
  {
    success = false;
    goto done;
  }
  success = dir_add (child_dir, "..", dir->inode->sector);
  
done:
  dir_close (dir);
  return success;
}

struct file *
filesys_open_dir (const char *full_path)
{
  struct inode *inode = file_open_recursive (full_path);
  struct file *file = dir_open (inode);
  return file;
}


/*
bool
filesys_create (const char *filename, off_t initial_size)
{
  char *absolute_path=get_absolute_path(filename);
  bool success=filesys_create_with_cwd(NULL, absolute_path, initial_size);
  free(absolute_path);
  return success;
  //lock_acquire(&filesys_lock);
  /*block_sector_t inode_sector = 0;
  struct dir *dir = dir_open_root ();
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size)
                  && dir_add (dir, name, inode_sector));
  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  dir_close (dir);

  //lock_release(&filesys_lock);
  return success;
}*/



/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *full_path)
{
  struct inode *inode = file_open_recursive (full_path);
  struct file *file = file_open (inode);
  return file;
}

/*
struct file *
filesys_open (const char *filename)
{
  char *absolute_path=get_absolute_path(filename);
  struct file *file=filesys_open_with_cwd(NULL, absolute_path);
  free(absolute_path);
  return file;
  /*
  lock_acquire(&filesys_lock);
  struct dir *dir = dir_open_root ();
  struct inode *inode = NULL;

  if (dir != NULL)
    dir_lookup (dir, name, &inode);
  dir_close (dir);

  struct file *file = file_open (inode);
  lock_release(&filesys_lock);
  return file;
}*/


/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *full_path)
{
  struct dir *dir = file_parent_dir_open_recursive (full_path);
  char* name = get_file_name (full_path);
  bool success = dir != NULL && dir_remove (dir, name);
  dir_close (dir);
  return success;
}

/*
bool
filesys_remove (const char *filename)
{
  char *absolute_path=get_absolute_path(filename);
  bool success=filesys_remove_with_cwd(NULL, absolute_path);
  free(absolute_path);
  return success;
}
*/


/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
