#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "ibtls.h"
#include "common.h"

int main(int argc, char **argv) {
  struct RDMA_communicator comm;
  struct RDMA_request *req1, *req2;
  char *data;
  uint64_t chunk, offset;
  int ctl_tag;
  double ss, ee;
  int max;

  chunk = 1024 * 1024 * 1;
  max   = 1024 * 1024 * 1024;

  RDMA_Passive_Init(&comm);
  data = (char*)RDMA_Alloc(max);
  memset(data, 1, max);
  int req_num = 2;
  struct RDMA_request req;

  sleep(1);
  ss = get_dtime();    
  for (offset = 0; offset + chunk <= max; offset += chunk) {
    if (offset + chunk == max) {
      RDMA_Irecv_offset(data, offset, chunk, NULL, RDMA_ANY_SOURCE, RDMA_ANY_TAG, &comm, &req);  
      fprintf(stderr, "Last\n");
    } else {
      RDMA_Irecv_silent_offset(data, offset, chunk, NULL, RDMA_ANY_SOURCE, RDMA_ANY_TAG, &comm, &req);  
    }
    RDMA_Wait(&req);
  }
  ee = get_dtime();
  sleep(1);
  fprintf(stderr, "[%c] Size: %d [bytes], Latency: %f, Throughput: %f [Gbytes/sec]\n", data[max-chunk], max, ee- ss, (max/1000000000.0)/(ee - ss));
  return 0;
}

