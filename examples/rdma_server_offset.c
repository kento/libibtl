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
  //  data = (char*)valloc(max);
  memset(data, 1, max);
  int req_num = 2;
  struct RDMA_request req;

  // for (s = max; s >= min; s = s / 2) {
  sleep(1);
  ss = get_dtime();    
  for (offset = 0; offset + chunk <= max; offset += chunk) {
    //    fprintf(stderr, "RDMA irecv\n");
    //    RDMA_Irecv(data, s, NULL, RDMA_ANY_SOURCE, RDMA_ANY_TAG, &comm, &req);  
    if (offset + chunk == max) {
      RDMA_Irecv_offset(data, offset, chunk, NULL, RDMA_ANY_SOURCE, RDMA_ANY_TAG, &comm, &req);  
      fprintf(stderr, "Last\n");
      //      RDMA_Irecv(data + offset, chunk, NULL, RDMA_ANY_SOURCE, RDMA_ANY_TAG, &comm, &req);  
    } else {
      RDMA_Irecv_silent_offset(data, offset, chunk, NULL, RDMA_ANY_SOURCE, RDMA_ANY_TAG, &comm, &req);  
    }
    RDMA_Wait(&req);
    //    fprintf(stderr, "wait\n");
    //    fprintf(stderr, "%d %f %f\n", s, ee- ss, (s/1000000000.0)/(ee - ss));
    //    sleep(1);
  }
  ee = get_dtime();
  sleep(1);
  fprintf(stderr, "[%c] Size: %d [bytes], Latency: %f, Throughput: %f [Gbytes/sec]\n", data[max-chunk], max, ee- ss, (max/1000000000.0)/(ee - ss));

    //  dump(data, max);
  return 0;
}

int dump(char* addr, int size) {
  int fd;
  int n_write = 0;
  int n_write_sum = 0;

  printf("dumping...\n");
  fd = open("/home/usr2/11D37048/tmp/test", O_WRONLY | O_CREAT, 0777); 
  if (fd <= 0) {
    fprintf(stderr, "error\n");
    exit(1);
  }
  do {
    n_write = write(fd, addr + n_write_sum, size);
    n_write_sum = n_write_sum + n_write;
    if (n_write_sum >= size) break;
  }  while(n_write > 0);
  close(fd);
  printf("dumping...ends\n");
  return 0;
}
