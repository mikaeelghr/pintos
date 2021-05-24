#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"


#define return_null_when_null(obj)  \
  if(obj==NULL){                    \
    return NULL;                    \
  }                                 
                                                              

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (block_sector_t sector, size_t entry_cnt)
{
  return inode_create (sector, entry_cnt * sizeof (struct dir_entry), true);
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode)
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      lock_init (&dir->l);
      dir->inode = inode;
      dir->pos = 0;
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL;
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir)
{
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir)
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir)
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp)
{
  struct dir_entry e;
  size_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  lock_acquire (&dir->l);
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
    if (e.in_use && !strcmp (name, e.name))
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        lock_release (&dir->l);
        return true;
      }
  lock_release (&dir->l);
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode)
{
  struct dir_entry e;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  if (strlen (name) == 1 && name[0] == '.')
    *inode = dir->inode;
  else if (lookup (dir, name, &e, NULL))
    *inode = inode_open (e.inode_sector);
  else
    *inode = NULL;

  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector)
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX || (strlen (name) == 1 && name[0]=='.'))
    return false;

  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL))
    goto done_without_lock_release;

  lock_acquire (&dir->l);
  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.

     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
  lock_release (&dir->l);

 done_without_lock_release:
  return success;
}


/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name)
{
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs))
    goto done_without_lock_release;

  lock_acquire (&dir->l);
  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL)
    goto done;
  if (inode_isdir (inode))
  {
    struct dir *child_dir = dir_open(inode);
    bool failed = false;
    struct dir_entry child_e;
    size_t child_ofs;
    lock_acquire (&child_dir->l);
    for (child_ofs = sizeof child_e; inode_read_at (inode, &child_e, sizeof child_e, child_ofs) == sizeof child_e;
        child_ofs += sizeof child_e)
      if (child_e.in_use)
        {
          failed = true;
          break;
        }
    lock_release (&child_dir->l);
    dir_close(child_dir);
    if (failed)
      {
        goto done;
      }
  }

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e)
  {
    goto done_without_lock_release;
  }

  /* Remove inode. */
  inode_remove (inode);
  success = true;

 done:
  lock_release (&dir->l);
 done_without_lock_release:
  inode_close (inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;
  lock_acquire (&dir->l);
  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e)
    {
      dir->pos += sizeof e;
      if (e.in_use && (strlen(e.name) != 2 || e.name[0] != '.' || e.name[1] != '.'))
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          lock_release (&dir->l);
          return true;
        }
    }
  lock_release (&dir->l);
  return false;
}

char*
get_file_name(const char *full_path)
{
  char *token, *save_ptr, *last_token=NULL;
  char *copy_path = malloc(strlen(full_path) + 1);
  memcpy(copy_path, full_path, strlen(full_path) + 1);
  for (token = strtok_r (copy_path, "/", &save_ptr); token != NULL;
  token = strtok_r (NULL, "/", &save_ptr)) {
    last_token=token;
  }
  if (!last_token)
    {
      free (copy_path);
      return NULL;
    }
  char *copy_last_token = malloc(strlen(last_token) + 1);
  memcpy(copy_last_token, last_token, strlen(last_token) + 1);
  free(copy_path);
  return copy_last_token;
}

struct dir*
file_parent_dir_open_recursive(const char *full_path)
{
  struct dir *cwd = thread_current()->cwd;
  return_null_when_null(full_path);
  struct inode *pinode = NULL;
  int path_offset=0;
  if (full_path[0] == '/' || cwd==NULL)
  {
    pinode = inode_open (ROOT_DIR_SECTOR);
    if (full_path[0] == '/')
    {
      path_offset = 1;
    }
  }
  else
  {
    //bool woow2_rel=0;
    //ASSERT(woow2_rel==1);
    pinode = inode_reopen (cwd->inode);
    path_offset = 0;
  }
  struct dir *pdir = NULL;
  char *token, *save_ptr;
  char *copy_path = malloc(strlen(full_path) + 1);
  memcpy(copy_path, full_path, strlen(full_path) + 1);
  for (token = strtok_r (copy_path + path_offset, "/", &save_ptr); token != NULL;
  token = strtok_r (NULL, "/", &save_ptr))
  {
    if(pdir!=NULL){
      dir_close (pdir);
    }
    if(NULL==pinode)
    {
      free(copy_path);
      return NULL;
    }
    pdir=dir_open(pinode);
    if(pdir==NULL)
    {
      free(copy_path);
      return NULL;
    }
    dir_lookup(pdir, token, &pinode);
  }
  free(copy_path);
  return pdir;
}

struct inode* 
file_open_recursive(const char *full_path)
{
  struct dir *cwd = thread_current()->cwd;
  if (cwd && cwd->inode && cwd->inode->removed)
    return NULL;
  return_null_when_null(full_path);
  struct inode *pinode = NULL, *inode_copy=NULL;
  int path_offset=0;
  if (full_path[0] == '/' || cwd==NULL)
  {
    pinode = inode_open (ROOT_DIR_SECTOR);
    if (full_path[0] == '/')
    {
      path_offset = 1;
    }
  }
  else
  {
    //bool woow_rel=0;
    //ASSERT(woow_rel==1);
    pinode = inode_reopen (cwd->inode);
  }
  inode_copy = inode_reopen (pinode);
  struct dir *pdir = NULL;
  char *token, *save_ptr;
  char *copy_path = malloc(strlen(full_path) + 1);
  memcpy(copy_path, full_path, strlen(full_path) + 1);
  for (token = strtok_r (copy_path + path_offset, "/", &save_ptr); token != NULL;
  token = strtok_r (NULL, "/", &save_ptr))
  {
    if(NULL==pinode)
    {
      free(copy_path);
      return NULL;
    }
    if(inode_copy!=NULL)
    {
      inode_close(inode_copy);
    }
    pdir=dir_open(pinode);
    if(pdir==NULL)
    {
      free(copy_path);
      return NULL;
    }
    dir_lookup(pdir, token, &pinode);
    inode_copy = inode_reopen(pinode);
    dir_close (pdir);
  }
  free(copy_path);
  return inode_copy;
}