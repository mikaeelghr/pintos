/* Verifies that a deleted file may still be written to and read
   from. */

#include <random.h>
#include <string.h>
#include <syscall.h>
#include "lib/kernel/list.h"
#include "devices/block.h"
#include "tests/lib.h"
#include "tests/main.h"


char buf1[1234];
char buf2[1234];

void
test_main (void)
{
  const char *file_name = "garbage";
  int fd;

  CHECK (create (file_name, sizeof buf1), "create \"%s\"", file_name);
  CHECK ((fd = open (file_name)) > 1, "open \"%s\"", file_name);

  for (int i = 0; i < 100; i += 1) {
    random_bytes (buf1, sizeof buf1);
    CHECK (write (fd, buf1, sizeof buf1) > 0, "write %d \"%s\"", i, file_name);
  }

  int read_count, write_count;

  CHECK (diskreadwritecount (&read_count, &write_count) >= 0, "disk count");
  
  CHECK(write_count > 200, "write count ok");
  CHECK(read_count < 10, "read count ok");

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