#include <sys/time.h>
#include <stdio.h>

#include <unistd.h> /* for close */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

#include "ibtls.h"
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
  fd = ibtl_open("/data/test", 0, 0);
  ibtl_write(fd, data, BUF_SIZE);
  ibtl_read(fd, data, BUF_SIZE);
  ibtl_close(fd);

  return 0;
}




