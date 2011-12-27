#include <stdlib.h>
#include <fcntl.h>
#include "rdma_server.h"



int main(int argc, char **argv) {
  struct RDMA_communicator comm;
  char *data;
  uint64_t size;
  int ctl_tag;

  RDMA_Passive_Init(&comm);
  while (1) {
    RDMA_Recvr(&data, &size, &ctl_tag, &comm);
    //printf("%d: size=%lu: %s\n", ctl_tag, size, data);
    
    printf("%d: size=%lu: \n", ctl_tag, size);
    //    dump(data, size);
  }

  //  RDMA_show_buffer();  
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
