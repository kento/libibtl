#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "ibtls.h"
#include "common.h"

#define BUF_SIZE_MIN 1024
#define BUF_SIZE_MAX (1024 * 1024)

int main(int argc, char **argv) {
  struct RDMA_communicator comm;
  struct RDMA_request req;
  char *data;
  uint64_t min, max;
  double ss, ee;
  uint64_t s;

  min = BUF_SIZE_MIN;
  max = BUF_SIZE_MAX;

  RDMA_Passive_Init(&comm);
  data = (char*)RDMA_Alloc(max);
  memset(data, 1, max);

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
