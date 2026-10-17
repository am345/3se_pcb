#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

extern char end;
static char *heap_end;

int _close(int file)
{
  (void)file;
  return -1;
}

int _fstat(int file, struct stat *st)
{
  (void)file;
  st->st_mode = S_IFCHR;
  return 0;
}

int _isatty(int file)
{
  (void)file;
  return 1;
}

int _lseek(int file, int ptr, int dir)
{
  (void)file;
  (void)ptr;
  (void)dir;
  return 0;
}

int _read(int file, char *ptr, int len)
{
  (void)file;
  (void)ptr;
  (void)len;
  return 0;
}

int _write(int file, char *ptr, int len)
{
  (void)file;
  (void)ptr;
  return len;
}

void *_sbrk(ptrdiff_t incr)
{
  char *prev_heap_end;

  if (heap_end == 0)
  {
    heap_end = &end;
  }

  prev_heap_end = heap_end;
  heap_end += incr;
  return (void *)prev_heap_end;
}
