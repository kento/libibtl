#include <sys/time.h>
#include <stdio.h>

#include <unistd.h> /* for close */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

#include "ibtl_io_client.h"
#include "common.h"

#define NUM 1
#define ITE 1000
#define SLP 1

#define BUF_SIZE (1 * 1024 * 1024 * 1024)

char data[BUF_SIZE];

int get_tag(void);

int main(int argc, char **argv)
{
  int fd;
  double s, e;
  fd = ibtl_open("/data/test", 0, 0);
  sleep(1111);
  //  s = get_dtime();
  ibtl_write(fd, data, BUF_SIZE);

  //  e = get_dtime();
  //  ibtl_dbg("Time: %f, size: %d GB, bw: %f GB/s", e - s, BUF_SIZE / 1000000000, BUF_SIZE / (e - s) / 1000000000.0 );
  //ibtl_read(fd, data, BUF_SIZE);
  //  ibtl_close(fd);
  return 0;
}




