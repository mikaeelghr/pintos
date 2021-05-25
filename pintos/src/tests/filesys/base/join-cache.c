/* Verifies that a deleted file may still be written to and read
   from. */

#include <random.h>
#include <string.h>
#include <syscall.h>
#include "lib/kernel/list.h"
#include "devices/block.h"
#include "tests/lib.h"
#include "tests/main.h"


char buf1[1];
char buf2[1];

void
test_main (void)
{
  const char *file_name = "garbage";
  int fd;


  long long int base_read_count, base_write_count;
  long long int read_count, write_count;

  CHECK (create (file_name, sizeof buf1), "create \"%s\"", file_name);
  CHECK ((fd = open (file_name)) > 1, "open \"%s\"", file_name);

  CHECK (diskreadwritecount (&base_read_count, &base_write_count) >= 0, "disk count");


  for (int i = 0; i < 64*1000; i += 1) {
    random_bytes (buf1, sizeof buf1);
    if (write (fd, buf1, sizeof buf1) <= 0) {
        CHECK(0, "Failed on %d write", i);
    }
  }


  CHECK (diskreadwritecount (&read_count, &write_count) >= 0, "disk count");

  read_count -= base_read_count;
  write_count -= base_write_count;

  CHECK(write_count < 129, "write count ok", write_count);
  CHECK(read_count < 129,   "read count ok", read_count);

/*
  struct list_elem *e;

  e = list_head (&all_blocks);
  while ((e = list_next (e)) != list_end (&all_blocks))
    {
      
    }
  */
  //thread_create("const char *name", 2, test_main, NULL);
  //block_print_stats();
}
