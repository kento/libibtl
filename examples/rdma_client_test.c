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


int main(int argc, char **argv)
{
  char* host;
  char* data;
  uint64_t size;
  int flag1;
  double s,e;
  double ss,ee;
  int i,j;
  struct  RDMA_communicator comm;
  struct  RDMA_param param;
  struct RDMA_request req[NUM];

  if (argc < 1) {
    printf("./rdma_client_test  <send size(Bytes)>\n");
    exit(1);
  }
  size = atoi(argv[1]);

  s = get_dtime();
  RDMA_Active_Init(&comm, &param);
  e = get_dtime();
  printf("Initialization: %f\n",e - s);

  data = (char*)RDMA_Alloc(size);

  ss = get_dtime();
  for (j = 0; j < ITE; j++) {
    s = get_dtime();
    for (i = 0; i < NUM; i++) {
      RDMA_Isend(data + i * (size/NUM), size/NUM, NULL, 0, i, &comm, &req[i]);
    }
    for (i = 0; i < NUM; i++) {
      RDMA_Wait(&req[i]);
    }
    e = get_dtime();
    printf("Send(%d): %d[MB]  %f %f GB/s\n", j, (size/1000000) ,  e - s, (size/1000000000.0  )/(e - s));
    sleep(SLP);
  }
  ee = get_dtime();
  sleep(1);
  return 0;
}
