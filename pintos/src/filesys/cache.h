#ifndef FILESYS_CACHE
#define FILESYS_CACHE

#include "filesys/off_t.h"
#include "devices/block.h"

void cache_init (void);

void cache_read (struct block *, block_sector_t, void *, off_t, off_t);

void cache_write (struct block *, block_sector_t, void *, off_t, off_t);

void cache_done (struct block *);

int
cache_get_stats (long long *access_count, long long *hit_count);

#endif