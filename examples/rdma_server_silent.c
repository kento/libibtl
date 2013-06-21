#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "ibtls.h"
#include "common.h"


int main(int argc, char **argv) {
  struct RDMA_communicator comm;
  struct RDMA_request *req1, *req2;
  char *data;
  uint64_t min, s;
  int ctl_tag;
  double ss, ee;
  int max;

  min = 1024;
  max = 1024 * 1024;

  RDMA_Passive_Init(&comm);
  data = (char*)RDMA_Alloc(max);
  memset(data, 1, max);
  int req_num = 2;
  struct RDMA_request req;

  while (1) {
    for (s = min; s <= max; s = s * 2) {
      ss = get_dtime();    
      RDMA_Irecv_silent(data, s, NULL, RDMA_ANY_SOURCE, RDMA_ANY_TAG, &comm, &req);  
      RDMA_Wait(&req);
      ee = get_dtime();
      fprintf(stderr, "Size: %d [bytes], Latency: %f, Throughput: %f [Gbytes/sec]\n", s, ee- ss, (s/1000000000.0)/(ee - ss));
      sleep(1);
    }
  }
  return 0;
}
