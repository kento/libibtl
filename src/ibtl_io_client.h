#ifdef _IBTL_IO_CLIENT_H
#define _IBTL_IO_CLIENT_H

#include <stddef.h>

int ibtl_open(const char *pathname, int flags, int mode);
ssize_t ibtl_read(int fd, void *buf, size_t count);
ssize_t ibtl_write(int fd, void *buf, size_t count);
int ibtl_close(int fd);

#endif //_IBTL_IO_CLIENT_H
