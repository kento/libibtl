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

#define BUF_SIZE (1024 * 1024 * 128)

int main(int argc, char **argv)
{
  char* data;
  uint64_t size;
  double ss,ee;
  struct RDMA_communicator comm;
  struct RDMA_param param;
  struct RDMA_request req;

  size = BUF_SIZE;

  RDMA_Active_Init(&comm, &param);
  data = (char*)RDMA_Alloc(size);

  ss = get_dtime();
  RDMA_Isend(data, size, NULL, 0, 2, &comm, &req);
  while (RDMA_Trywait(&req) == 0);
  ee = get_dtime();

  sleep(1);
  printf("Send: %d[MB]  %f %f GB/s\n", size/1000000 ,  ee - ss, (size/1000000000.0) / (ee -  ss));
  return 0;
}
