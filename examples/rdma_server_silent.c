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
  //  data = (char*)valloc(max);
  memset(data, 1, max);
  int req_num = 2;
  struct RDMA_request req;

  // for (s = max; s >= min; s = s / 2) {
  while (1) {
    for (s = min; s <= max; s = s * 2) {
      ss = get_dtime();    
      //    fprintf(stderr, "RDMA irecv\n");
      //    RDMA_Irecv(data, s, NULL, RDMA_ANY_SOURCE, RDMA_ANY_TAG, &comm, &req);  
      RDMA_Irecv_silent(data, s, NULL, RDMA_ANY_SOURCE, RDMA_ANY_TAG, &comm, &req);  
      //    fprintf(stderr, "wait\n");
      RDMA_Wait(&req);
      ee = get_dtime();
      //    fprintf(stderr, "Size: %d [bytes], Latency: %f, Throughput: %f [Gbytes/sec]\n", s, ee- ss, (s/1000000000.0)/(ee - ss));
      //      fprintf(stderr, "%d %f %f\n", s, ee- ss, (s/1000000000.0)/(ee - ss));
      //    sleep(1);
    }
  }



  return 0;
}

int dump(char* addr, int size) {
  int fd;
  int n_write = 0;
  int n_write_sum = 0;

  printf("dumping...\n");
  fd = open("/home/usr2/11D37048/tmp/test", O_WRONLY | O_CREAT); 
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
