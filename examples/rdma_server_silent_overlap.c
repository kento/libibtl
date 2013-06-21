#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "ibtls.h"
#include "common.h"


int main(int argc, char **argv) {
  struct RDMA_communicator comm;
  struct RDMA_request *req1, *req2;
  char *data[2];
  uint64_t size;
  int ctl_tag;
  double ss[2], ee[2];

  size = 1024;

  data[0] = (char*)valloc(size);
  data[1] = (char*)valloc(size);
  
  memset(data[0], 1, size);
  memset(data[1], 1, size);
  RDMA_Passive_Init(&comm);
  int req_num = 2;
  struct RDMA_request req[req_num];
  int req_id = 0;
  ss[req_id] = get_dtime();
  sleep(1);
  RDMA_Irecv_silent(data[req_id], size, NULL, RDMA_ANY_SOURCE, RDMA_ANY_TAG, &comm, &req[req_id]);  
  req_id = (req_id + 1) % req_num;
  fprintf(stderr, "%p: size=%lu: \n", data, size);
  while (1) {
    ss[req_id] = get_dtime();
    RDMA_Irecv_silent(data[req_id], size, NULL, RDMA_ANY_SOURCE, RDMA_ANY_TAG, &comm, &req[req_id]);  
    req_id = (req_id + 1) % req_num;
    fprintf(stderr, "Wait\n", ee[req_id] - ss[req_id]);
    RDMA_Wait(&req[req_id]);
    ee[req_id] = get_dtime();
    fprintf(stderr, "Latency: %f\n", ee[req_id] - ss[req_id]);
  }
  return 0;
}

