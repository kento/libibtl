#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "ibtls.h"
#include "common.h"

#define BUF_SIZE (1024 * 1024 * 128)
#define NUM_CHUNK (1024)

int main(int argc, char **argv) {
  struct RDMA_communicator comm;
  struct RDMA_request req;
  char *data;
  uint64_t max, chunk, offset;
  double ss, ee;

  RDMA_Passive_Init(&comm);

  chunk = BUF_SIZE / NUM_CHUNK;
  max   = BUF_SIZE;
  fprintf(stderr, "chunk: %d, max: %d\n", chunk, max);

  data = (char*)RDMA_Alloc(max);
  memset(data, 1, max);

  ss = get_dtime();    
  for (offset = 0; offset + chunk <= max; offset += chunk) {
    if (offset + chunk == max) {
      RDMA_Irecv_offset(data, offset, chunk, NULL, RDMA_ANY_SOURCE, RDMA_ANY_TAG, &comm, &req);  
    } else {
      RDMA_Irecv_silent_offset(data, offset, chunk, NULL, RDMA_ANY_SOURCE, RDMA_ANY_TAG, &comm, &req);  
    }
    RDMA_Wait(&req);
  }
  ee = get_dtime();

  fprintf(stderr, "Size: %d [bytes], Latency: %f, Throughput: %f [Gbytes/sec]\n",  max, ee- ss, (max/1000000000.0)/(ee - ss));
  return 0;
}

