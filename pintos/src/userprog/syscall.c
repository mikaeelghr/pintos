#include "userprog/syscall.h"
#include <stdio.h>
#include <stdlib.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

int fdall = 5;

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

struct file *
get_file_from_fd(struct list *l, int the_fd)
{
  struct list_elem *e = list_head (l);
  while ((e = list_next (e)) != list_end (l))
    {
      struct file_descriptor *ev = list_entry(e, struct file_descriptor, elem);
      if (ev->fd == the_fd) return ev->file;
    }
  return NULL;
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  uint32_t* args = ((uint32_t*) f->esp);

  /*
   * The following print statement, if uncommented, will print out the syscall
   * number whenever a process enters a system call. You might find it useful
   * when debugging. It will cause tests to fail, however, so you should not
   * include it in your final submission.
   */

  /* printf("System call number: %d\n", args[0]); */

  if (args[0] == SYS_EXIT)
    {
      f->eax = args[1];
      printf ("%s: exit(%d)\n", &thread_current ()->name, args[1]);
      thread_exit ();
    }
  else if (args[0] == SYS_OPEN)
    {
      struct file *fi = filesys_open(args[1]);
      if (fi == NULL)
        {
          f->eax = -1;
          return;
        } 
      struct file_descriptor *fds = palloc_get_page(0);
      (sizeof (struct file_descriptor));
      fds->fd = fdall;
      fdall += 1;
      fds->file = fi;
      list_push_back(&thread_current ()->file_descriptors, &(fds->elem));
      f->eax = fds->fd;
    }
  else if (args[0] == SYS_FILESIZE)
    {
      struct file *fi = get_file_from_fd(&thread_current ()->file_descriptors, args[1]);
      f->eax = file_length(fi);
    }
  else if (args[0] == SYS_WRITE)
    {
      if(args[1] == STDOUT_FILENO)
        {
          const char *buffer = (char *) args[2];
          putbuf (buffer, (int) args[3]);
        }
    }
  else if (args[0] == SYS_PRACTICE)
    {
      f->eax = args[1] + 1;
    }
  else if (args[0] == SYS_READ)
    {
      struct file *fi = get_file_from_fd(&thread_current ()->file_descriptors, args[1]);
      f->eax = file_read(fi, args[2], args[3]);
    }
}
