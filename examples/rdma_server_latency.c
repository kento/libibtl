#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "ibtls.h"
#include "common.h"


int main(int argc, char **argv) {
  struct RDMA_communicator comm;
  double latency;
  double A = 0.0001;

  RDMA_Passive_Init(&comm);

  while (1) {
    latency = RDMA_Latency(RDMA_ANY_SOURCE, &comm);
    if (latency > A) {
      fprintf(stderr, "Latency: %.10f (%.10f)\n", latency, A);
    }
    usleep(100);
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
}
