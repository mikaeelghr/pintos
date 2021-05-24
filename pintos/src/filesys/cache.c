#include "filesys/cache.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include <debug.h>
#include <string.h>
#include <stdbool.h>


#define CACHE_BLOCK_COUNT 64

struct cache_block
  {
    struct lock l;
    block_sector_t sector_idx;
    bool valid;
    bool dirty;
    bool used;
    uint8_t data[BLOCK_SECTOR_SIZE];
  };

static int clock;

static struct cache_block cache_blocks[CACHE_BLOCK_COUNT];
static struct lock global_cache_lock;

void cache_init (void)
{
  lock_init (&global_cache_lock);

  int i;
  for (i = 0; i < CACHE_BLOCK_COUNT; i++)
    {
      lock_init (&cache_blocks[i].l);
      cache_blocks[i].valid = false;
    }
}


void flush_block (struct block *fs_device, int index)
{
  block_write (fs_device, cache_blocks[index].sector_idx, cache_blocks[index].data);
  cache_blocks[index].dirty = 0;
}


int try_finding_block (struct block *fs_device, block_sector_t sector_idx)
{
  int i, found = -1;
  for (i = 0; i < CACHE_BLOCK_COUNT; i++)
    {
      lock_acquire (&cache_blocks[i].l);
      if (cache_blocks[i].valid && cache_blocks[i].sector_idx == sector_idx)
        {
          found = i;
          break;
        }
      lock_release (&cache_blocks[i].l);
    }
  return found;
}

/*
 * returns from this function with the lock of that block in hand
 */
static int find_an_empty_cache_block (struct block *fs_device, block_sector_t sector_idx)
{
  lock_acquire (&global_cache_lock);
  // try once more with global cache lock
  int index = try_finding_block (fs_device, sector_idx);
  if (index != -1)
    return index;
  while (1)
    {
      index = clock;
      clock = (clock + 1) % CACHE_BLOCK_COUNT;
      lock_acquire (&cache_blocks[index].l);

      if (!cache_blocks[index].valid)
        {
          lock_release (&global_cache_lock);
          return index;
        }

      if (!cache_blocks[index].used)
        break;

      cache_blocks[index].used = 0;
      lock_release (&cache_blocks[index].l);
    }

  // do this before writing to disk to not make others wait
  lock_release (&global_cache_lock);
  if (cache_blocks[index].dirty)
    flush_block (fs_device, index);

  return index;
}

/*
 * returns from this function with the lock of that block in hand
 */
static int bring_block_to_cache (struct block *fs_device, block_sector_t sector_idx)
{
  int index = find_an_empty_cache_block (fs_device, sector_idx);
  cache_blocks[index].used = 1;
  cache_blocks[index].valid = 1;
  cache_blocks[index].dirty = 0;
  cache_blocks[index].sector_idx = sector_idx;
  block_read (fs_device, sector_idx, cache_blocks[index].data);
  return index;
}

/*
 * returns from this function with the lock of that block in hand
 */
int get_block_index (struct block *fs_device, block_sector_t sector_idx)
{
  int found = try_finding_block (fs_device, sector_idx);
  if (found >= 0)
    return found;

  return bring_block_to_cache (fs_device, sector_idx);
}

void cache_read (struct block *fs_device, block_sector_t sector_idx, void *buffer, off_t size, off_t offset)
{
  int index = get_block_index (fs_device, sector_idx);
  memcpy (buffer, cache_blocks[index].data + offset, size);
  lock_release (&cache_blocks[index].l);
}


void cache_write (struct block *fs_device, block_sector_t sector_idx, void *buffer, off_t size, off_t offset)
{
  int index = get_block_index (fs_device, sector_idx);
  memcpy (cache_blocks[index].data + offset, buffer, size);
  cache_blocks[index].dirty = 1;
  lock_release (&cache_blocks[index].l);
}

void cache_done (struct block *fs_device)
{
  lock_acquire (&global_cache_lock);
  int i;
  for (i = 0; i < CACHE_BLOCK_COUNT; i++)
    {
      lock_acquire (&cache_blocks[i].l);
      if (cache_blocks[i].valid && cache_blocks[i].dirty)
        flush_block(fs_device, i);
      lock_release (&cache_blocks[i].l);
    }
  lock_release (&global_cache_lock);
}
