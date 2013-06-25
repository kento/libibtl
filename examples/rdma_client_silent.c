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
#include "common.h"

#define BUF_SIZE (1024 * 1024 *128)

int main(int argc, char **argv)
{
  struct RDMA_communicator comm;
  struct RDMA_param param;
  char* data;
  uint64_t size;


  size = BUF_SIZE;

  RDMA_Active_Init(&comm, &param);
  data = (char*)RDMA_Alloc(size);

  RDMA_Send(data, size, NULL, 0, 2, &comm);

  return 0;
}


