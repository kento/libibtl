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

#define NUM 1
#define ITE 1000
#define SLP 1

int get_tag(void);

int main(int argc, char **argv)
{
  char* host;
  char* data;
  uint64_t size;
  double s,e;
  double ss,ee;
  int i, j;
  char * a;
  struct  RDMA_communicator comm;
  struct  RDMA_param param;
  struct RDMA_request req;

  if (argc < 2) {
    printf("./rdma_client_offset <send size(Bytes)>\n");
    exit(1);
  }
  size = atoi(argv[1]);

  RDMA_Active_Init(&comm, &param);
  data = (char*)RDMA_Alloc(size);

  ss = get_dtime();
  RDMA_Isend(data, size, NULL, 0, 2, &comm, &req);
  while (RDMA_Trywait(&req) == 0);
  ee = get_dtime();

  sleep(1);
  printf("Send: %d[MB]  %f %f GB/s\n", (size/1000000) * ITE ,  ee - ss, (size/1000000000.0 * ITE )/(ee -  ss));
  return 0;
}
