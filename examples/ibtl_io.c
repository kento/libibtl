#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h> /* for close */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

#include "ibtl_io_client.h"

#define NUM 1
#define ITE 1000
#define SLP 1

#define BUF_SIZE (1 * 1024 * 1024 * 1024)

char data[BUF_SIZE];

int get_tag(void);
double get_time(void);

int main(int argc, char **argv)
{
  int fd;
  double s, e;
  char path[256];

  sprintf(path, "%s:/tmp/test", argv[1]);
  fd = ibtl_open(path, O_RDWR | O_CREAT , S_IRWXU);

  memset(data, 0, BUF_SIZE);
  ibtl_write(fd, data, BUF_SIZE);
  sleep(2);
  //  ibtl_write(fd, data, BUF_SIZE);
  s = get_time();
  ibtl_write(fd, data, BUF_SIZE);
  e = get_time();
  fdmi_dbg("Time: %f, size: %d GB, bw: %f GB/s", e - s, BUF_SIZE / 1000000000, BUF_SIZE / (e - s) / 1000000000.0 );
  sleep(1111);
  //ibtl_read(fd, data, BUF_SIZE);
  //  ibtl_close(fd);
  return 0;
}

double get_time(void)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return ((double)(tv.tv_sec) + (double)(tv.tv_usec) * 0.001 * 0.001);
}




