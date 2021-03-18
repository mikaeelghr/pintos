#include "userprog/syscall.h"
#include <stdio.h>
#include <stdlib.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"

#define put_error_on_frame_when_false(o, f) if (!o) f->eax=-1
#define put_error_on_frame_when_null(o, f) if ((o) == NULL) f->eax=-1
#define return_on_null(o) if ((o) == NULL) return

static void syscall_handler (struct intr_frame *);

bool are_args_valid (uint32_t *, int);

bool is_void_pointer_valid (struct thread *, void *);

int fdall = 5;

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

struct file_descriptor *
get_file_descriptor_from_fd (struct list *l, int the_fd)
{
  struct list_elem *e = list_head (l);
  while ((e = list_next (e)) != list_end (l))
    {
      struct file_descriptor *ev = list_entry (e,
      struct file_descriptor, elem);
      if (ev->fd == the_fd) return ev;
    }
  return NULL;
}

struct file *
get_file_from_fd (struct list *l, int the_fd)
{
  struct file_descriptor *file_descriptor_instance = get_file_descriptor_from_fd (l, the_fd);
  if (file_descriptor_instance == NULL)
    return NULL;
  return file_descriptor_instance->file;
}

void _exit (int status)
{
  struct thread *t = thread_current ();
  t->exit_code = status;
  printf ("%s: exit(%d)\n", &thread_current ()->name, status);
  file_close (thread_current ()->execfile);
  thread_exit ();
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  uint32_t *args = ((uint32_t *) f->esp);

  /*
   * The following print statement, if uncommented, will print out the syscall
   * number whenever a process enters a system call. You might find it useful
   * when debugging. It will cause tests to fail, however, so you should not
   * include it in your final submission.
   */

  /* printf("System call number: %d\n", args[0]); */

  if (!are_args_valid (args, 1))
    _exit (-1);

  if (args[0] == SYS_EXIT)
    {
      if (!are_args_valid (args, 2))
        _exit (-1);
      f->eax = args[1];
      _exit (args[1]);
    }
  else if (args[0] == SYS_EXEC)
    {
      if (!are_args_valid (args, 2) || !is_void_pointer_valid (thread_current (), (void *) args[1]))
        _exit (-1);
      f->eax = process_execute (args[1]);
    }
  else if (args[0] == SYS_WAIT)
    {
      if (!are_args_valid (args, 2))
        _exit (-1);
      f->eax = process_wait (args[1]);
    }
  else if (args[0] == SYS_OPEN)
    {
      if (!are_args_valid (args, 2) || !is_void_pointer_valid (thread_current (), (void *) args[1]))
        _exit (-1);
      struct file *fi = filesys_open (args[1]);
      put_error_on_frame_when_null(fi, f);
      return_on_null(fi);
      struct file_descriptor *fds = palloc_get_page (0);
      fds->fd = fdall;
      fdall += 1;
      fds->file = fi;
      list_push_back (&thread_current ()->file_descriptors, &(fds->elem));
      f->eax = fds->fd;
    }
  else if (args[0] == SYS_FILESIZE)
    {
      if (!are_args_valid (args, 2))
        _exit (-1);
      struct file *fi = get_file_from_fd (&thread_current ()->file_descriptors, args[1]);
      put_error_on_frame_when_null(fi, f);
      return_on_null(fi);
      f->eax = file_length (fi);
    }
  else if (args[0] == SYS_WRITE)
    {
      if (!are_args_valid (args, 4) || !is_void_pointer_valid (thread_current (), (void *) args[2]))
        _exit (-1);
      if (args[1] == STDOUT_FILENO)
        {
          const char *buffer = (char *) args[2];
          putbuf (buffer, (int) args[3]);
        }
      else
        {
          struct file *fi = get_file_from_fd (&thread_current ()->file_descriptors, args[1]);
          put_error_on_frame_when_null(fi, f);
          return_on_null(fi);
          f->eax = file_write (fi, args[2], args[3]);
        }
    }
  else if (args[0] == SYS_PRACTICE)
    {
      if (!are_args_valid (args, 2))
        _exit (-1);
      f->eax = args[1] + 1;
    }
  else if (args[0] == SYS_READ)
    {
      if (!are_args_valid (args, 4) || !is_void_pointer_valid (thread_current (), (void *) args[2]))
        _exit (-1);
      struct file *fi = get_file_from_fd (&thread_current ()->file_descriptors, args[1]);
      put_error_on_frame_when_null(fi, f);
      return_on_null(fi);
      f->eax = file_read (fi, args[2], args[3]);
    }
  else if (args[0] == SYS_CREATE)
    {
      if (!are_args_valid (args, 3) || !is_void_pointer_valid (thread_current (), (void *) args[1]))
        _exit (-1);
      f->eax = filesys_create (args[1], args[2]);
    }
  else if (args[0] == SYS_CLOSE)
    {
      if (!are_args_valid (args, 2))
        _exit (-1);
      struct file_descriptor *file_descriptor_instance = get_file_descriptor_from_fd (
              &thread_current ()->file_descriptors, args[1]);
      return_on_null(file_descriptor_instance);
      file_close (file_descriptor_instance->file);
      list_remove (&(file_descriptor_instance->elem));
      palloc_free_page (file_descriptor_instance);
    }
  else if (args[0] == SYS_SEEK)
    {
      if (!are_args_valid (args, 3))
        _exit (-1);
      struct file *fi = get_file_from_fd (&thread_current ()->file_descriptors, args[1]);
      return_on_null(fi);
      file_seek (fi, args[2]);
    }
  else if (args[0] == SYS_TELL)
    {
      if (!are_args_valid (args, 2))
        _exit (-1);
      struct file *fi = get_file_from_fd (&thread_current ()->file_descriptors, args[1]);
      put_error_on_frame_when_null(fi, f);
      return_on_null(fi);
      f->eax = file_tell (fi);
    }
  else if (args[0] == SYS_REMOVE)
    {
      if (!are_args_valid (args, 2) || !is_void_pointer_valid (thread_current (), (void *) args[1]))
        _exit (-1);
      f->eax = filesys_remove (args[1]);
    }
  else if (args[0] == SYS_HALT)
    {
      shutdown_power_off ();
    }
}

bool
is_pointer_mapped (struct thread *t, uint32_t *p)
{
  return pagedir_get_page (t->pagedir, p) != NULL;
}

bool
is_void_pointer_mapped (struct thread *t, uint32_t *p)
{
  return pagedir_get_page (t->pagedir, p) != NULL;
}

bool
is_pointer_valid (struct thread *t, uint32_t *p)
{
  return (p != NULL) && is_user_vaddr (p) && is_pointer_mapped (t, p);
}

bool
is_void_pointer_valid (struct thread *t, void *p)
{
  return (p != NULL) && is_user_vaddr (p) && is_void_pointer_mapped (t, p);
}

bool
are_args_valid (uint32_t *args, int number_of_args)
{
  struct thread *t = thread_current ();
  int i;
  for (i = 0; i <= number_of_args; i++)
    {
      if (!is_pointer_valid (t, &args[i]))
        return false;
    }
  return true;
}
