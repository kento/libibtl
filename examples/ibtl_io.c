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
  int is_read_mode;

  if (argc != 3) {
    fdmi_err("a.out <hostname:/path/to/file> <mode:0(write) 1(read)>");
  }

  sprintf(path, "%s", argv[1]);
  is_read_mode = atoi(argv[2]);
  fd = ibtl_open(path, O_RDWR | O_CREAT , S_IRWXU);

  memset(data, 1, BUF_SIZE);

  if (!is_read_mode) {
    ibtl_write(fd, data, BUF_SIZE);
    sleep(5);
    s = get_time();
    ibtl_write(fd, data, BUF_SIZE);
    e = get_time();
    fdmi_dbg("Write Time: %f, size: %d GB, bw: %f GB/s", e - s, BUF_SIZE / 1000000000, BUF_SIZE / (e - s) / 1000000000.0 );
  } else { 
    ibtl_read(fd, data, BUF_SIZE);
    sleep(5);
    s = get_time();
    ibtl_read(fd, data, BUF_SIZE);
    e = get_time();
    fdmi_dbg("Read Time: %f, size: %d GB, bw: %f GB/s", e - s, BUF_SIZE / 1000000000, BUF_SIZE / (e - s) / 1000000000.0 );
  }

  return 0;
}

double get_time(void)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return ((double)(tv.tv_sec) + (double)(tv.tv_usec) * 0.001 * 0.001);
}




