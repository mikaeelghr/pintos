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
      struct file_descriptor *fds = malloc(sizeof (struct file_descriptor));
      fds->fd = fdall;
      fdall += 1;
      fds->file = fi;
      list_push_back(&thread_current ()->file_descriptors, &(fds->elem));
      f->eax = fds->fd;
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
}
