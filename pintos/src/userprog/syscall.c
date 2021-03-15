#include "userprog/syscall.h"
#include <stdio.h>
#include <stdlib.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/vaddr.h"
#include "threads/thread.h"

#define put_error_on_frame_when_null(o, f) \
  if (o == NULL)                           \
  f->eax = -1
#define return_on_null(o) \
  if (o == NULL)          \
  return

static void syscall_handler(struct intr_frame *);

int fdall = 5;

void syscall_init(void)
{
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

struct file_descriptor *
get_file_descriptor_from_fd(struct list *l, int the_fd)
{
  struct list_elem *e = list_head(l);
  while ((e = list_next(e)) != list_end(l))
  {
    struct file_descriptor *ev = list_entry(e,
                                            struct file_descriptor, elem);
    if (ev->fd == the_fd)
      return ev;
  }
  return NULL;
}

struct file *
get_file_from_fd(struct list *l, int the_fd)
{
  struct file_descriptor *file_descriptor_instance = get_file_descriptor_from_fd(l, the_fd);
  if (file_descriptor_instance == NULL)
    return NULL;
  return file_descriptor_instance->file;
}

static bool is_char_pointer_valid(const char *ptr)
{
  char *endptr = ptr;
  struct thread *t = thread_current();
  if (ptr < t)
  {
    return 0;
  }
  while (endptr < t + PGSIZE && *endptr != 0)
  {
    endptr++;
  }
  if (endptr == t + PGSIZE)
  {
    return 0;
  }
  return 1;
}

static bool is_pointer_valid(const void *ptr)
{
  struct thread *t = thread_current();
  if (ptr < t || ptr + 4 > t + PGSIZE)
  {
    return 0;
  }
  return 1;
}

static void
syscall_handler(struct intr_frame *f UNUSED)
{
  uint32_t *args = ((uint32_t *)f->esp);
  if (!is_pointer_valid(args) || !is_pointer_valid(args + 1))
  {
    f->eax = -1;
    return;
  }

  /*
   * The following print statement, if uncommented, will print out the syscall
   * number whenever a process enters a system call. You might find it useful
   * when debugging. It will cause tests to fail, however, so you should not
   * include it in your final submission.
   */

  /* printf("System call number: %d\n", args[0]); */

  printf("tname: %s\n", &thread_current()->name);
  if (args[0] == SYS_EXIT)
  {
    f->eax = args[1];
    printf("%s: exit(%d)\n", &thread_current()->name, args[1]);
    thread_exit();
  }
  else if (args[0] == SYS_OPEN)
  {
    if (is_char_pointer_valid(args[1]))
    {
      f->eax = -1;
      return;
    }
    struct file *fi = filesys_open(args[1]);
    put_error_on_frame_when_null(fi, f);
    return_on_null(fi);
    struct file_descriptor *fds = palloc_get_page(0);
    fds->fd = fdall;
    fdall += 1;
    fds->file = fi;
    list_push_back(&thread_current()->file_descriptors, &(fds->elem));
    f->eax = fds->fd;
  }
  else if (args[0] == SYS_FILESIZE)
  {
    struct file *fi = get_file_from_fd(&thread_current()->file_descriptors, args[1]);
    put_error_on_frame_when_null(fi, f);
    return_on_null(fi);
    f->eax = file_length(fi);
  }
  else if (args[0] == SYS_WRITE)
  {
    if (!is_pointer_valid(args + 2) || !is_pointer_valid(args + 3) || !is_char_pointer_valid(args[2]))
    {
      f->eax = -1;
      return;
    }
    if (args[1] == STDOUT_FILENO)
    {
      const char *buffer = (char *)args[2];
      putbuf(buffer, (int)args[3]);
    }
    else
    {
      struct file *fi = get_file_from_fd(&thread_current()->file_descriptors, args[1]);
      put_error_on_frame_when_null(fi, f);
      return_on_null(fi);
      f->eax = file_write(fi, args[2], args[3]);
    }
  }
  else if (args[0] == SYS_PRACTICE)
  {
    f->eax = args[1] + 1;
  }
  else if (args[0] == SYS_READ)
  {
    if (!is_pointer_valid(args + 2) || !is_pointer_valid(args + 3) || !is_char_pointer_valid(args[2]))
    {
      f->eax = -1;
      return;
    }
    struct file *fi = get_file_from_fd(&thread_current()->file_descriptors, args[1]);
    put_error_on_frame_when_null(fi, f);
    return_on_null(fi);
    f->eax = file_read(fi, args[2], args[3]);
  }
  else if (args[0] == SYS_CREATE)
  {
    if (!is_pointer_valid(args + 2) || !is_char_pointer_valid(args[1]))
    {
      f->eax = -1;
      return;
    }
    f->eax = filesys_create(args[1], args[2]);
  }
  else if (args[0] == SYS_CLOSE)
  {

    struct file_descriptor *file_descriptor_instance = get_file_descriptor_from_fd(
        &thread_current()->file_descriptors, args[1]);
    return_on_null(file_descriptor_instance);
    file_close(file_descriptor_instance->file);
    list_remove(&(file_descriptor_instance->elem));
    palloc_free_page(file_descriptor_instance);
  }
  else if (args[0] == SYS_SEEK)
  {
    if (!is_pointer_valid(args + 2))
    {
      f->eax = -1;
      return;
    }
    struct file *fi = get_file_from_fd(&thread_current()->file_descriptors, args[1]);
    return_on_null(fi);
    file_seek(fi, args[2]);
  }
  else if (args[0] == SYS_TELL)
  {
    struct file *fi = get_file_from_fd(&thread_current()->file_descriptors, args[1]);
    put_error_on_frame_when_null(fi, f);
    return_on_null(fi);
    f->eax = file_tell(fi);
  }
  else if (args[0] == SYS_REMOVE)
  {
    if (!is_char_pointer_valid(args[1]))
    {
      f->eax = -1;
      return;
    }
    f->eax = filesys_remove(args[1]);
  }
}
