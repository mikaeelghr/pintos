#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/off_t.h"
#include "devices/block.h"
#include "threads/synch.h"

struct bitmap;


/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define INODE_INSTANT_CHILDREN_COUNT 123
#define INODE_INDIRECT_INSTANT_CHILDREN_COUNT 128
#define min(a, b) ((a < b)? a : b)

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    int is_dir;
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    block_sector_t children[INODE_INSTANT_CHILDREN_COUNT];
    block_sector_t double_indirect, indirect;
  };

struct indirect_node
  {
    block_sector_t children[INODE_INDIRECT_INSTANT_CHILDREN_COUNT];
  };


/* In-memory inode. */
struct inode
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
    struct lock l;
  };


void inode_init (void);
bool inode_create (block_sector_t, off_t);
struct inode *inode_open (block_sector_t);
struct inode *inode_reopen (struct inode *);
block_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);
void inode_acquire_lock (struct inode *inode);
void inode_release_lock (struct inode *inode);

bool inode_isdir(const struct inode *inode);
#endif /* filesys/inode.h */
