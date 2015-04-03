/* 

Compile:
  gcc -o syscall-dump.o -c -fPIC -Wall syscall-dump.c
  gcc -o syscall-dump.so -shared -rdynamic syscall-dump.o -ldl

Usage:
   export LD_PRELOAD=/tmp/syscall-dump.so
   export SYSCALL_DUMP_LOG=/tmp/syscall.log
   ..run.a.binary..

*/

#define _LARGEFILE64_SOURCE
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#if defined(RTLD_NEXT)
#define REAL_LIBC RTLD_NEXT
#else
#define REAL_LIBC ((void *) -1L)
#endif

#ifdef SYS_gettid
static inline int gettid(void)
{
  return syscall(SYS_gettid);
}
#else
#error "SYS_gettid unavailable on this system"
#endif

/* Function pointers for real libc versions */
static int dlog_fd = -1;
static int (*real_open)(const char *pathname, int flags, ...);
static int (*real_open64)(const char *pathname, int flags, ...);
static int (*real___open64)(const char *pathname, int flags, ...);
static int (*real_socket)(int domain, int type, int protocol);
static int (*real_ioctl)(int fd, unsigned long request, ...);
static ssize_t (*real_write)(int fd, const void *buf, size_t len);
static ssize_t (*real_read)(int fd, void *buf, size_t len);
static off_t (*real_lseek)(int fd, off_t offset, int whence);
static off64_t (*real_lseek64)(int fd, off64_t offset, int whence);
static int (*real_close)(int fd);
static int (*real_dup)(int oldfd);
static int (*real_dup2)(int oldfd, int newfd);
static int (*real_eventfd)(unsigned int initval, int flags);
static int (*real_system)(const char *command);
static FILE *(*real_fopen)(const char *pathname, const char *mode);
static FILE *(*real_freopen)(const char *pathname, const char *mode, FILE *stream);
static int (*real_fclose)(FILE *fp);
static size_t (*real_fread)(void *ptr, size_t size, size_t nmemb, FILE *stream);
static size_t (*real_fwrite)(const void *ptr, size_t size, size_t nmemb, FILE *stream);

#define REDIR(realptr, symname) do { \
  if ((realptr) == NULL) { \
    (realptr) = dlsym(REAL_LIBC, symname); \
    if ((realptr) == NULL) exit(1001); \
  } \
} while (0)

#define E(r) (r < 0 ? errno : 0)

/* log function */
static void dlog(const char *fmt, ...)
{
  char buf[2048], *b;
  va_list ap;
  size_t len;
  ssize_t r, j;
  int keep_errno = errno, fd;

  if (dlog_fd < 0) {
    const char *f = getenv("SYSCALL_DUMP_LOG");
    REDIR(real_open, "open");
    REDIR(real_read, "read");
    REDIR(real_close, "close");
    sprintf(buf, "/proc/%d/cmdline", gettid());
    fd = real_open(buf, O_RDONLY);
    if (fd >= 0) {
      r = real_read(fd, buf, sizeof(buf)-1);
      real_close(fd);
      if (r > 0) {
        if (buf[r-1] == '\0')
          r--;
        for (j = 0; j < r; j++)
          if (buf[j] == '\0')
            buf[j] = '|';
        buf[r] = '\0';
      }
    } else {
      buf[0] = '\0';
    }
    dlog_fd = f ? real_open(f, O_CREAT|O_APPEND|O_WRONLY, 0600) : 2 /* stderr */;
    if (dlog_fd < 0) exit(1002);
    dlog("syscall dump init for executable '%s', log fd %d\n", buf, dlog_fd);
  }
  sprintf(buf, "[%5d]", gettid());
  va_start(ap, fmt);
  vsnprintf(buf + 7, sizeof(buf) - 7, fmt, ap);
  va_end(ap);
  REDIR(real_write, "write");
  len = strlen(b = buf);
  while (len > 0) {
    r = real_write(dlog_fd, b, len);
    if (r < 0) {
      if (errno != EAGAIN && errno != EINTR && errno != EWOULDBLOCK)
        break;
    } else {
      len -= r;
      b += r;
    }
  }
  errno = keep_errno;
}

static void dlog_hexa(const char *prefix, void *buf, size_t len)
{
  char b[128];
  size_t i, l;
  int keep_errno = errno;
  
  while (len > 0) {
    for (i = l = 0; i < 16 && len > 0; i++, len--, buf++)
      l += sprintf(b + l, "%s%02x", i > 0 ? ":" : "", *(unsigned char *)buf);
    b[l] = '\n';
    b[l+1] = '\0';
    dlog("%s %s", prefix, b);
  }
  errno = keep_errno;
}

/* open() wrapper */
int open(const char *pathname, int flags, ...)
{
  va_list ap;
  mode_t mode;
  int r;
  
  REDIR(real_open, "open");

  if (flags & O_CREAT) {
    /* Get argument */
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);

    r = real_open(pathname, flags, mode);
    dlog("open('%s', 0x%x, 0x%lx) = %d (%d)\n", pathname, flags, (long)mode, r, E(r));
  } else {
    r = real_open(pathname, flags);
    dlog("open('%s', 0x%x) = %d (%d)\n", pathname, flags, r, E(r));
  }
  return r;
}

/* open64() wrapper */
int open64(const char *pathname, int flags, ...)
{
  va_list ap;
  mode_t mode;
  int r;

  REDIR(real_open64, "open64");

  if (flags & O_CREAT) {
    /* Get argument */
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);

    r = real_open64(pathname, flags, mode);
    dlog("open64('%s', 0x%x, 0x%lx) = %d (%d)\n", pathname, flags, (long)mode, r, E(r));
  } else {
    r = real_open64(pathname, flags);
    dlog("open64('%s', 0x%x) = %d (%d)\n", pathname, flags, r, E(r));
  }
  return r;
}

/* __open64() wrapper */
int __open64(const char *pathname, int flags, ...)
{
  va_list ap;
  mode_t mode;
  int r;

  REDIR(real___open64, "__open64");

  if (flags & O_CREAT) {
    /* Get argument */
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);

    r = real_open64(pathname, flags, mode);
    dlog("__open64('%s', 0x%x, 0x%lx) = %d (%d)\n", pathname, flags, (long)mode, r, E(r));
  } else {
    r = real_open64(pathname, flags);
    dlog("__open64('%s', 0x%x) = %d (%d)\n", pathname, flags, r, E(r));
  }
  return r;
}

/* fopen() wrapper */
FILE *fopen(const char *pathname, const char *mode)
{
  FILE *r;
  int keep_errno;

  REDIR(real_fopen, "fopen");

  r = real_fopen(pathname, mode);
  keep_errno = errno;
  dlog("fopen('%s', '%s') = %p (%d) (fileno %d)\n", pathname, mode,
       r, r == NULL ? errno : 0, r != NULL ? fileno(r) : -1);
  errno = keep_errno;
  return r;
}

/* freopen() wrapper */
FILE *freopen(const char *pathname, const char *mode, FILE *stream)
{
  FILE *r;
  int keep_errno;

  REDIR(real_freopen, "freopen");

  r = real_freopen(pathname, mode, stream);
  keep_errno = errno;
  dlog("freopen('%s', '%s', %p) = %p (%d) (fileno %d)\n", pathname, mode, stream,
       r, r == NULL ? errno : 0, r != NULL ? fileno(r) : -1);
  errno = keep_errno;
  return r;
}

/* socket() wrapper */
int socket(int domain, int type, int protocol)
{
  int r;

  REDIR(real_socket, "socket");

  r = real_socket(domain, type, protocol);
  dlog("socket(%d, %d, %d) = %d (%d)\n", domain, type, protocol, r, E(r));
  return r;
}

/* close() wrapper */
int close(int fd)
{
  int r;

  REDIR(real_close, "close");

  r = real_close(fd);
  dlog("close(%d) = %d (%d)\n", fd, r, E(r));
  return r;
}

/* fclose() wrapper */
int fclose(FILE *fp)
{
  int r;

  REDIR(real_fclose, "fclose");

  r = real_fclose(fp);
  dlog("fclose(%p) = %d (%d)\n", fp, r, r == EOF ? errno : 0);
  return r;
}

/* write() wrapper */
ssize_t write(int fd, const void *buf, size_t len)
{
  int r;

  REDIR(real_write, "write");
  
  r = real_write(fd, buf, len);
  if (r > 0) {
    dlog_hexa("write:", (void *)buf, r);
    dlog("  write(%d, %p, %zd) = %d (%d)\n", fd, buf, len, r, E(r));
  } else {
    dlog("write(%d, %p, %zd) = %d (%d)\n", fd, buf, len, r, E(r));
  }
  return r;
}

/* read() wrapper */
ssize_t read(int fd, void *buf, size_t len)
{
  int r;

  REDIR(real_read, "read");
  
  r = real_read(fd, buf, len);
  dlog("read(%d, %p, %zd) = %d (%d)\n", fd, buf, len, r, E(r));
  if (r > 0)
    dlog_hexa("  read:", buf, r);
  return r;
}

/* lseek() wrapper */
off_t lseek(int fd, off_t offset, int whence)
{
  int r;

  REDIR(real_lseek, "lseek");

  r = real_lseek(fd, offset, whence);
  dlog("lseek(%d, %ld, %d) = %d (%d)\n", fd, (long)offset, whence, r, E(r));
  return r;
}

/* lseek64() wrapper */
off64_t lseek64(int fd, off64_t offset, int whence)
{
  int r;

  REDIR(real_lseek64, "lseek64");

  r = real_lseek64(fd, offset, whence);
  dlog("lseek(%d, %lld, %d) = %d (%d)\n", fd, (long long)offset, whence, r, E(r));
  return r;
}

/* fread () wrapper */
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  size_t r;

  REDIR(real_fread, "fread");

  r = real_fread(ptr, size, nmemb, stream);
  dlog("fread(%p, %zu, %zu, %p) = %zu\n", ptr, size, nmemb, stream, r);
  return r;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  size_t r;

  REDIR(real_fwrite, "fwrite");

  r = real_fwrite(ptr, size, nmemb, stream);
  dlog("fwrite(%p, %zu, %zu, %p) = %zu\n", ptr, size, nmemb, stream, r);
  return r;
}

/* dup() wrapper */
int dup(int oldfd)
{
  int r;

  REDIR(real_dup, "dup");

  r = real_dup(oldfd);
  dlog("dup(%d) = %d (%d)\n", oldfd, r, E(r));
  return r;
}

/* dup2() wrapper */
int dup2(int oldfd, int newfd)
{
  int r;

  REDIR(real_dup2, "dup2");

  r = real_dup2(oldfd, newfd);
  dlog("dup2(%d, %d) = %d (%d)\n", oldfd, newfd, r, E(r));
  return r;
}

/* eventfd() wrapper */
int eventfd(unsigned int initval, int flags)
{
  int r;

  REDIR(real_eventfd, "eventfd");

  r = real_eventfd(initval, flags);
  dlog("eventfd(%u, %d) = %d (%d)\n", initval, flags, r, E(r));
  return r;
}

/* system() wrapper */
int system(const char *command)
{
  int r;

  REDIR(real_system, "system");

  r = real_system(command);
  dlog("system('%s') = %d (%d)\n", command, r, E(r));
  return r;
}

/* ioctl() wrapper */
int ioctl(int fd, unsigned long request, ...)
{
  va_list ap;
  unsigned long size, dir, arg;
  int r;

  REDIR(real_ioctl, "ioctl");

  /* Get argument */
  va_start(ap, request);
  arg = va_arg(ap, unsigned long);
  va_end(ap);

//  nr = (request >> _IOC_NRSHIFT) & _IOC_NRMASK;
  size = (request >> _IOC_SIZESHIFT) & _IOC_SIZEMASK;
//type = (request >> _IOC_TYPESHIFT) & _IOC_TYPEMASK;
   dir = (request >> _IOC_DIRSHIFT) & _IOC_DIRMASK;

  switch (dir) {
  case _IOC_NONE:
    r = real_ioctl(fd, request, arg);
    dlog("ioctl(%d, 0x%04lx, 0x%08x) = %d (%d)\n", fd, request, arg, r, E(r));
    break;
  case _IOC_READ:
    r = real_ioctl(fd, request, arg);
    dlog("ioctl(%d, 0x%04lx, %p) = %d (%d)\n", fd, request, arg, r, E(r));
    dlog_hexa("  ioctl()/r:", (void *)arg, size);
    break;
  case _IOC_WRITE:
    dlog_hexa("ioctl()/w:", (void *)arg, size);
    r = real_ioctl(fd, request, arg);
    dlog("  ioctl(%d, 0x%04lx, %p) = %d (%d)\n", fd, request, arg, r, E(r));
    break;
  case _IOC_READ|_IOC_WRITE:
    dlog_hexa("ioctl()/w:", (void *)arg, size);
    r = real_ioctl(fd, request, arg);
    dlog("  ioctl(%d, 0x%04lx, %p) = %d (%d)\n", fd, request, arg, r, E(r));
    dlog_hexa("  ioctl()/r:", (void *)arg, size);
    break;
  }

  return r;
}
