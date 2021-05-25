
#include <random.h>
#include <string.h>
#include <syscall.h>
#include "lib/kernel/list.h"
#include "devices/block.h"
#include "tests/lib.h"
#include "tests/main.h"

#define NUM_ENTRIES 64
#define BLOCK_SECTOR_SIZE 512
#define BUF_SIZE (BLOCK_SECTOR_SIZE * NUM_ENTRIES / 2)

static char buf[BUF_SIZE];
static long long num_accesses;
static long long num_hits;
static long long num_misses;

void
test_main (void)
{
  int test_fd;
  char *test_file_name = "test";
  CHECK (create (test_file_name, 0), "create \"%s\"", test_file_name);
  CHECK ((test_fd = open (test_file_name)) > 1, "open \"%s\"", test_file_name);
  random_bytes (buf, sizeof buf);
  CHECK (write (test_fd, buf, sizeof buf) == BUF_SIZE,
   "write %d bytes to \"%s\"", (int) BUF_SIZE, test_file_name);
  close (test_fd);
  msg ("close \"%s\"", test_file_name);
  CHECK ((test_fd = open (test_file_name)) > 1,
    "open \"%s\"", test_file_name);

  cacheinv ();
  msg ("cacheinv");

  CHECK (cachestat (&num_accesses, &num_hits) == 0, "cachestat");

  long long base_accesses = num_accesses;
  long long base_hits = num_hits;

  CHECK (read (test_fd, buf, sizeof buf) == BUF_SIZE,
   "read %d bytes from \"%s\"", (int) BUF_SIZE, test_file_name);

  CHECK (cachestat (&num_accesses, &num_hits) == 0, "cachestat");

  int old_hit_rate = (num_hits - base_hits)*100/(num_accesses - base_accesses);

  base_accesses = num_accesses;
  base_hits = num_hits;

  close (test_fd);
  msg ("close \"%s\"", test_file_name);
  CHECK ((test_fd = open (test_file_name)) > 1,
    "open \"%s\"", test_file_name);

  CHECK (read (test_fd, buf, sizeof buf) == BUF_SIZE,
   "read %d bytes from \"%s\"", (int) BUF_SIZE, test_file_name);

  CHECK (cachestat (&num_accesses, &num_hits) == 0, "cachestat");

  int new_hit_rate = (num_hits - base_hits)*100/(num_accesses - base_accesses);

  CHECK (new_hit_rate >= old_hit_rate,
    "old hit rate percent: %d, new hit rate percent: %d",
    old_hit_rate, new_hit_rate);

  msg ("close \"%s\"", test_file_name);
  close (test_fd);
}
