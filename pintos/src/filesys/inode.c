#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "cache.h"


/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}


/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos)
{
  ASSERT (inode != NULL);
  if (pos >= inode->data.length)
    return -1;

  if (pos < INODE_INSTANT_CHILDREN_COUNT * BLOCK_SECTOR_SIZE)
    return inode->data.children[pos / BLOCK_SECTOR_SIZE];
//  else if (post < INODE_INDIRECT_INSTANT_CHILDREN_COUNT * BLOCK_SECTOR_SIZE)
//    return inode->data.indirect
//  return inode->data.start + pos / BLOCK_SECTOR_SIZE;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void)
{
  list_init (&open_inodes);
}

static bool inode_allocate_sector (block_sector_t *sector)
{
  static char zeros[BLOCK_SECTOR_SIZE];
  if (!free_map_allocate (1, sector))
    return 0;
  cache_write (fs_device, *sector, zeros, BLOCK_SECTOR_SIZE, 0);
  return 1;
}


void inode_release_sectors (block_sector_t children[], size_t sectors)
{
  size_t i;
  for (i = 0; i < sectors; i++)
    free_map_release (children[i], 1);
}

void inode_release_indirect (block_sector_t sector, size_t sectors)
{
  struct indirect_node *indirect = calloc (1, sizeof (struct indirect_node));
  cache_read (fs_device, sector, indirect, BLOCK_SECTOR_SIZE, 0);
  inode_release_sectors (indirect->children, sectors);
  free_map_release (sector, 1);
}


bool inode_try_allocating_sectors (block_sector_t children[], size_t sectors)
{
  bool success = true;
  size_t i;
  for (i = 0; i < sectors && success; i++)
    success &= inode_allocate_sector (&children[i]);
  if (!success)
    {
      inode_release_sectors (children, i);
      return success;
    }
  return success;
}


bool inode_try_creating_indirect (block_sector_t *sector, size_t sectors, bool create_sector)
{
  if (create_sector && !inode_allocate_sector (sector))
    return 0;
  struct indirect_node *indirect = calloc (1, sizeof (struct indirect_node));
  if (!indirect)
    return 0;
  if (!create_sector)
    cache_read (fs_device, *sector, indirect, BLOCK_SECTOR_SIZE, 0);
  if (!inode_try_allocating_sectors (indirect->children, sectors))
    {
      free (indirect);
      free_map_release (*sector, 1);
      return false;
    }
  cache_write (fs_device, *sector, indirect, BLOCK_SECTOR_SIZE, 0);
  free (indirect);
  return 1;
}


bool inode_try_creating_double_indirect (block_sector_t *sector, size_t sectors)
{
  if (!inode_allocate_sector (sector))
    return 0;
  struct indirect_node *double_indirect = calloc (1, sizeof (struct indirect_node));
  if (!double_indirect)
    return 0;
  size_t indirect = DIV_ROUND_UP (sectors, INODE_INDIRECT_INSTANT_CHILDREN_COUNT);
  if (!inode_try_allocating_sectors (double_indirect->children, indirect))
    {
      free (double_indirect);
      return false;
    }
  size_t i;
  bool success = true;
  for (i = 0; i < indirect && success; i++)
    {
      size_t to_allocate_sectors;
      if (i + 1 < indirect)
        to_allocate_sectors = INODE_INDIRECT_INSTANT_CHILDREN_COUNT;
      else
        to_allocate_sectors = sectors % INODE_INDIRECT_INSTANT_CHILDREN_COUNT;
      success &= inode_try_creating_indirect (&double_indirect->children[i], to_allocate_sectors, 0);
    }

  if (!success)
    {
      size_t j;
      for (j = 0; j < i; j++)
        {
          inode_release_indirect (double_indirect->children[j], INODE_INDIRECT_INSTANT_CHILDREN_COUNT);
        }
      inode_release_sectors (double_indirect->children, indirect);
      free (double_indirect);
      return 0;
    }
  else
    cache_write (fs_device, *sector, double_indirect, BLOCK_SECTOR_SIZE, 0);
  free (double_indirect);
  return 1;
}


bool inode_try_growing_indirect (block_sector_t sector, size_t sectors, size_t current_sectors)
{
  ASSERT (sectors >= current_sectors);
  if (sectors == current_sectors)
    return 1;
  struct indirect_node *node = malloc (sizeof (struct indirect_node));
  if (!node)
    return 0;
  cache_read (fs_device, sector, node, BLOCK_SECTOR_SIZE, 0);
  if (!inode_try_allocating_sectors (node->children + current_sectors, sectors - current_sectors))
    {
      free (node);
      return false;
    }
  cache_write (fs_device, sector, node, BLOCK_SECTOR_SIZE, 0);
  free (node);
  return true;
}

bool inode_try_growing_doubly_indirect (block_sector_t sector, size_t sectors, size_t current_sectors)
{
  ASSERT (sectors >= current_sectors);
  if (sectors == current_sectors)
    return 1;
  struct indirect_node *double_indirect = calloc (1, sizeof (struct indirect_node));
  if (!double_indirect)
    return 0;
  cache_read (fs_device, sector, double_indirect, BLOCK_SECTOR_SIZE, 0);
  size_t indirect = DIV_ROUND_UP (sectors, INODE_INDIRECT_INSTANT_CHILDREN_COUNT);
  size_t current_indirect = DIV_ROUND_UP (current_sectors, INODE_INDIRECT_INSTANT_CHILDREN_COUNT);
  if (!inode_try_allocating_sectors (double_indirect->children + current_indirect, indirect - current_indirect))
    {
      free (double_indirect);
      return false;
    }
  size_t i;
  bool success = true;
  for (i = 0; i < indirect && success; i++)
    {
      size_t to_allocate_sectors;
      if (i + 1 < indirect)
        to_allocate_sectors = INODE_INDIRECT_INSTANT_CHILDREN_COUNT;
      else
        to_allocate_sectors = sectors % INODE_INDIRECT_INSTANT_CHILDREN_COUNT;
      size_t current_to_allocate_sectors = 0;
      if (i + 1 < current_indirect)
        current_to_allocate_sectors = INODE_INDIRECT_INSTANT_CHILDREN_COUNT;
      else if (i + 1 == current_indirect)
        current_to_allocate_sectors = sectors % INODE_INDIRECT_INSTANT_CHILDREN_COUNT;
      if (current_to_allocate_sectors != to_allocate_sectors)
        success &= inode_try_growing_indirect (double_indirect->children[i], to_allocate_sectors,
                                               current_to_allocate_sectors);
    }

  free (double_indirect);
  return success;
}

/*
 * make size of inode disk sectors
 */
bool inode_grow (struct inode_disk *disk_inode, size_t sectors)
{
  size_t current_sectors = bytes_to_sectors (disk_inode->length);
  ASSERT (sectors >= current_sectors);
  if (sectors == current_sectors)
    return 1;

  if (sectors > INODE_INSTANT_CHILDREN_COUNT + INODE_INDIRECT_INSTANT_CHILDREN_COUNT +
                INODE_INDIRECT_INSTANT_CHILDREN_COUNT * INODE_INDIRECT_INSTANT_CHILDREN_COUNT)
    return 0;
  size_t instant_sectors =
          min(sectors, INODE_INSTANT_CHILDREN_COUNT) - min(current_sectors, INODE_INSTANT_CHILDREN_COUNT);
  if (!inode_try_allocating_sectors (disk_inode->children + current_sectors, instant_sectors))
    return 0;
  sectors -= instant_sectors + min(current_sectors, INODE_INSTANT_CHILDREN_COUNT);
  current_sectors -= min(current_sectors, INODE_INSTANT_CHILDREN_COUNT);
  if (!sectors)
    return 1;
  size_t current_indirect_sectors = min(current_sectors, INODE_INDIRECT_INSTANT_CHILDREN_COUNT);
  size_t indirect_sectors = min(sectors, INODE_INDIRECT_INSTANT_CHILDREN_COUNT);
  if (!current_indirect_sectors && !inode_try_creating_indirect (&disk_inode->indirect, indirect_sectors, 1))
    {
      inode_release_sectors (disk_inode->children, instant_sectors);
      return 0;
    }
  else if (!inode_try_growing_indirect (disk_inode->indirect, indirect_sectors, current_indirect_sectors))
    return 0;
  sectors -= indirect_sectors;
  if (!sectors)
    return 1;

  size_t double_indirect_sectors = sectors;

  if (!inode_try_creating_double_indirect (&disk_inode->double_indirect, double_indirect_sectors))
    {
      inode_release_sectors (disk_inode->children, instant_sectors);
      inode_release_indirect (disk_inode->indirect, indirect_sectors);
      return 0;
    }
  return 1;
}


bool inode_try_creating_inode (struct inode_disk *disk_inode, size_t sectors)
{
  if (sectors > INODE_INSTANT_CHILDREN_COUNT + INODE_INDIRECT_INSTANT_CHILDREN_COUNT +
                INODE_INDIRECT_INSTANT_CHILDREN_COUNT * INODE_INDIRECT_INSTANT_CHILDREN_COUNT)
    return 0;
  size_t instant_sectors = min(sectors, INODE_INSTANT_CHILDREN_COUNT);
  if (!inode_try_allocating_sectors (disk_inode->children, instant_sectors))
    return 0;
  sectors -= instant_sectors;
  if (!sectors)
    return 1;
  size_t indirect_sectors = min(sectors, INODE_INDIRECT_INSTANT_CHILDREN_COUNT);
  if (!inode_try_creating_indirect (&disk_inode->indirect, indirect_sectors, 1))
    {
      inode_release_sectors (disk_inode->children, instant_sectors);
      return 0;
    }
  sectors -= indirect_sectors;
  if (!sectors)
    return 1;

  size_t double_indirect_sectors = sectors;

  if (!inode_try_creating_double_indirect (&disk_inode->double_indirect, double_indirect_sectors))
    {
      inode_release_sectors (disk_inode->children, instant_sectors);
      inode_release_indirect (disk_inode->indirect, indirect_sectors);
      return 0;
    }
  return 1;
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      success = inode_try_creating_inode (disk_inode, sectors);
      if (success)
        cache_write (fs_device, sector, disk_inode, BLOCK_SECTOR_SIZE, 0);
      free (disk_inode);
    }

  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e))
    {
      inode = list_entry (e,
      struct inode, elem);
      if (inode->sector == sector)
        {
          inode_reopen (inode);
          return inode;
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  lock_init(&inode->l);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  cache_read (fs_device, inode->sector, &inode->data, BLOCK_SECTOR_SIZE, 0);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode)
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);

      /* Deallocate blocks if removed. */
      if (inode->removed)
        {
          size_t sectors = bytes_to_sectors (inode->data.length);

          size_t instant_sectors = min(sectors, INODE_INSTANT_CHILDREN_COUNT);
          inode_release_sectors (inode->data.children, instant_sectors);
          sectors -= instant_sectors;

          size_t indirect_sectors = min(sectors, INODE_INDIRECT_INSTANT_CHILDREN_COUNT);
          if (indirect_sectors)
            {
              inode_release_indirect (inode->data.indirect, indirect_sectors);
              free_map_release (inode->data.indirect, 1);
            }
          sectors -= indirect_sectors;

          size_t i, indirect_nodes = DIV_ROUND_UP (sectors, INODE_INDIRECT_INSTANT_CHILDREN_COUNT);
          if (indirect_nodes)
            {
              struct indirect_node *double_indirect_node = calloc (1, sizeof (struct indirect_node));
              cache_read (fs_device, inode->data.double_indirect, double_indirect_node, BLOCK_SECTOR_SIZE, 0);
              for (i = 0; i < indirect_nodes; i++)
                {
                  size_t to_release_sectors;
                  if (i + 1 < indirect_nodes)
                    to_release_sectors = INODE_INDIRECT_INSTANT_CHILDREN_COUNT;
                  else
                    to_release_sectors = sectors % INODE_INDIRECT_INSTANT_CHILDREN_COUNT;
                  inode_release_indirect (double_indirect_node->children[i], to_release_sectors);
                }
              free_map_release (inode->data.double_indirect, 1);
              free (double_indirect_node);
            }
        }

      free (inode);
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode)
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

off_t inode_read_at_indirect (block_sector_t children[], uint8_t *buffer, off_t size, off_t offset)
{
  off_t bytes_read = 0;
  off_t node_size = BLOCK_SECTOR_SIZE;

  off_t first_node_index = offset / node_size;
  block_sector_t first_node = children[first_node_index];
  off_t from_first_size = min(size, node_size - (offset % node_size));
  cache_read (fs_device, first_node, buffer, from_first_size, offset % BLOCK_SECTOR_SIZE);
  bytes_read += from_first_size;

  size_t i;
  for (i = first_node_index + 1; bytes_read < size; i++)
    {
      cache_read (fs_device, children[i], buffer + bytes_read, min(size - bytes_read, node_size), 0);
      bytes_read += min(size - bytes_read, node_size);
    }
  return bytes_read;
}

off_t inode_read_at_double_indirect (struct indirect_node *node, uint8_t *buffer, off_t size, off_t offset)
{
  off_t bytes_read = 0;
  off_t node_size = BLOCK_SECTOR_SIZE * INODE_INSTANT_CHILDREN_COUNT;

  off_t first_node_index = offset / node_size;
  block_sector_t first_node = node->children[first_node_index];
  off_t from_first_size = min(size, node_size - (offset % node_size));
  struct indirect_node *indirect = malloc (sizeof (struct indirect_node));
  if (!indirect)
    return 0;
  cache_read (fs_device, first_node, indirect, BLOCK_SECTOR_SIZE, 0);
  bytes_read += inode_read_at_indirect (indirect->children, buffer, from_first_size, offset % node_size);

  size_t i;
  for (i = first_node_index + 1; bytes_read < size; i++)
    {
      cache_read (fs_device, node->children[i], indirect, BLOCK_SECTOR_SIZE, 0);
      inode_read_at_indirect (indirect->children, buffer + bytes_read, min(size - bytes_read, node_size), 0);
      bytes_read += min(size - bytes_read, node_size);
    }

  free (indirect);
  return bytes_read;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset)
{
  ASSERT(inode != NULL);
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;

  size = min(size, inode->data.length - offset);
  if (size <= 0)
    return 0;

  off_t to_read_from_children = min(size, INODE_INSTANT_CHILDREN_COUNT * BLOCK_SECTOR_SIZE - offset);
  if (to_read_from_children > 0)
    {
      bytes_read += inode_read_at_indirect (inode->data.children, buffer + bytes_read, to_read_from_children, offset);
      offset = 0;
    }
  else
    offset -= INODE_INSTANT_CHILDREN_COUNT * BLOCK_SECTOR_SIZE;

  if (bytes_read == size)
    return bytes_read;

  off_t to_read_from_indirect = min(size - bytes_read,
                                    INODE_INDIRECT_INSTANT_CHILDREN_COUNT * BLOCK_SECTOR_SIZE - offset);

  struct indirect_node *node = malloc (sizeof (struct indirect_node));
  if (!node)
    return bytes_read;
  if (to_read_from_indirect > 0)
    {
      cache_read (fs_device, inode->data.indirect, node, BLOCK_SECTOR_SIZE, 0);
      bytes_read += inode_read_at_indirect (node->children, buffer + bytes_read, to_read_from_indirect, offset);
      offset = 0;
    }
  else
    offset -= INODE_INDIRECT_INSTANT_CHILDREN_COUNT * BLOCK_SECTOR_SIZE;

  off_t to_read_from_double_indirect = size - bytes_read;
  if (to_read_from_double_indirect > 0)
    {
      cache_read (fs_device, inode->data.double_indirect, node, BLOCK_SECTOR_SIZE, 0);
      bytes_read += inode_read_at_double_indirect (node, buffer + bytes_read, to_read_from_double_indirect,
                                                   offset);
    }

  free (node);
  return bytes_read;
}


off_t inode_write_at_indirect (block_sector_t children[], uint8_t *buffer, off_t size, off_t offset)
{
  off_t bytes_written = 0;
  off_t node_size = BLOCK_SECTOR_SIZE;

  off_t first_node_index = offset / node_size;
  block_sector_t first_node = children[first_node_index];
  off_t from_first_size = min(size, node_size - (offset % node_size));
  cache_write (fs_device, first_node, buffer, from_first_size, offset % BLOCK_SECTOR_SIZE);
  bytes_written += from_first_size;

  size_t i;
  for (i = first_node_index + 1; bytes_written < size; i++)
    {
      cache_write (fs_device, children[i], buffer + bytes_written, min(size - bytes_written, node_size), 0);
      bytes_written += min(size - bytes_written, node_size);
    }
  return bytes_written;
}

off_t inode_write_at_double_indirect (struct indirect_node *node, uint8_t *buffer, off_t size, off_t offset)
{
  off_t bytes_written = 0;
  off_t node_size = BLOCK_SECTOR_SIZE * INODE_INSTANT_CHILDREN_COUNT;

  off_t first_node_index = offset / node_size;
  block_sector_t first_node = node->children[first_node_index];
  off_t from_first_size = min(size, node_size - (offset % node_size));
  struct indirect_node *indirect = malloc (sizeof (struct indirect_node));
  if (!indirect)
    return 0;
  cache_read (fs_device, first_node, indirect, BLOCK_SECTOR_SIZE, 0);
  bytes_written += inode_write_at_indirect (indirect->children, buffer, from_first_size, offset % node_size);

  size_t i;
  for (i = first_node_index + 1; bytes_written < size; i++)
    {
      cache_read (fs_device, node->children[i], indirect, BLOCK_SECTOR_SIZE, 0);
      inode_write_at_indirect (indirect->children, buffer + bytes_written, min(size - bytes_written, node_size), 0);
      bytes_written += min(size - bytes_written, node_size);
    }

  free (indirect);
  return bytes_written;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset)
{
  uint8_t *buffer = buffer_;
  off_t bytes_written = 0;

  if (offset + size > inode->data.length)
    {
      if (!inode_grow (&inode->data, bytes_to_sectors (offset + size)))
        return 0;
      inode->data.length = offset + size;
      cache_write (fs_device, inode->sector, &inode->data, BLOCK_SECTOR_SIZE, 0);
    }
  size = min(size, inode->data.length - offset);
  if (size <= 0)
    return 0;

  if (inode->deny_write_cnt)
    return 0;

  off_t to_read_from_children = min(size, INODE_INSTANT_CHILDREN_COUNT * BLOCK_SECTOR_SIZE - offset);
  if (to_read_from_children > 0)
    {
      bytes_written += inode_write_at_indirect (inode->data.children, buffer + bytes_written, to_read_from_children,
                                                offset);
      offset = 0;
    }
  else
    offset -= INODE_INSTANT_CHILDREN_COUNT * BLOCK_SECTOR_SIZE;

  if (bytes_written == size)
    return bytes_written;

  off_t to_read_from_indirect = min(size - bytes_written,
                                    INODE_INDIRECT_INSTANT_CHILDREN_COUNT * BLOCK_SECTOR_SIZE - offset);

  struct indirect_node *node = malloc (sizeof (struct indirect_node));
  if (!node)
    return bytes_written;
  if (to_read_from_indirect > 0)
    {
      cache_read (fs_device, inode->data.indirect, node, BLOCK_SECTOR_SIZE, 0);
      bytes_written += inode_write_at_indirect (node->children, buffer + bytes_written, to_read_from_indirect, offset);
      offset = 0;
    }
  else
    offset -= INODE_INDIRECT_INSTANT_CHILDREN_COUNT * BLOCK_SECTOR_SIZE;

  off_t to_read_from_double_indirect = size - bytes_written;
  if (to_read_from_double_indirect > 0)
    {
      cache_read (fs_device, inode->data.double_indirect, node, BLOCK_SECTOR_SIZE, 0);
      bytes_written += inode_write_at_double_indirect (node, buffer + bytes_written,
                                                       to_read_from_double_indirect,
                                                       offset);
    }

  free (node);
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode)
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode)
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

bool
inode_isdir(const struct inode *inode)
{
  if (inode == NULL)
  {
    return false;
  }
  struct inode_disk *inoded = malloc(sizeof *inoded);
  cache_read (fs_device, inode_get_inumber(inode), inoded, 0, BLOCK_SECTOR_SIZE);
  bool isdir = inoded->is_dir;
  free (inoded);
  return isdir;
}


void inode_release_lock (struct inode *inode)
{
  ASSERT(inode != NULL);
  lock_release(&inode->l);
}

void inode_acquire_lock (struct inode *inode)
{
  ASSERT(inode != NULL);
  lock_acquire(&inode->l);
}