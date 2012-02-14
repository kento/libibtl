#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "rdma_server.h"



int main(int argc, char **argv) {
  struct RDMA_communicator comm;
  struct RDMA_request *req1, *req2;
  char *data;
  uint64_t size;
  int ctl_tag;

  size = 1000 * 1000 * 1000;
  data = (char*)valloc(size);
  memset(data, 1, size);
  RDMA_Passive_Init(&comm);
  
  fprintf(stderr, "%p: size=%lu: \n", data, size);
  while (1) {
    req1 = (struct RDMA_request *)malloc(sizeof(struct RDMA_request));
    req2 = (struct RDMA_request *)malloc(sizeof(struct RDMA_request));
    rdma_irecv_r (data, size, NULL, RDMA_ANY_SOURCE, RDMA_ANY_TAG, &comm, req1);  
    rdma_irecv_r (data, size, NULL, RDMA_ANY_SOURCE, RDMA_ANY_TAG, &comm, req2);  
    rdma_wait(req1);
    rdma_wait(req2);
    free(req1);
    free(req2);
    sleep(1);
  }

  fprintf(stderr, "%p: size=%lu: \n", data, size);
  
  while(1);

  while (0) {

    //    RDMA_Recvr(&data, &size, &ctl_tag, &comm);
    //printf("%d: size=%lu: %s\n", ctl_tag, size, data);
    
    printf("%d: size=%lu: \n", ctl_tag, size);
    //printf("%s\n", data);
    //dump(data, size);
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
