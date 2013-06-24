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
  double ss, ee;
  int req_num = 2;
  struct RDMA_request req[req_num];
  int req_id = 0;

  size = 200 * 1024 * 1024  + 1;
  data[0] = (char*)valloc(size);
  data[1] = (char*)valloc(size);
  memset(data[0], 1, size);
  memset(data[1], 1, size);

  RDMA_Passive_Init(&comm);

  RDMA_Irecv(data[req_id], size, NULL, RDMA_ANY_SOURCE, RDMA_ANY_TAG, &comm, &req[req_id]);  
  req_id = (req_id + 1) % req_num;
  fprintf(stderr, "%p: size=%lu: \n", data, size);
  while (1) {
    ss = get_dtime();
    RDMA_Irecv(data[req_id], size, NULL, RDMA_ANY_SOURCE, RDMA_ANY_TAG, &comm, &req[req_id]);  
    req_id = (req_id + 1) % req_num;
    RDMA_Wait(&req[req_id]);
    ee = get_dtime();
    fprintf(stderr, "Latency: %f buf_id=%d\n", ee - ss, req_id);
  }
  return 0;
}
