#include <sys/time.h>
#include <stdio.h>

#include <unistd.h> /* for close */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

#include "ibtls.h"
#include "common.h"

#define NUM 1
#define ITE 1000
#define SLP 1

#define BUF_SIZE 64

int get_tag(void);

int main(int argc, char **argv)
{
  char* data;
  uint64_t size;
  int flag1;
  int i, j;
  char * a;
  double s,e;
  double ss,ee;
  struct  RDMA_communicator comm;
  struct  RDMA_param param;
  struct RDMA_request req[NUM];
  
  size = BUF_SIZE;

  RDMA_Active_Init(&comm, &param);
  data = (char*)RDMA_Alloc(size);
  printf("Initialization: %f\n",e - s);
  ss = get_dtime();

  
  for (j = 0; j < ITE; j++) {
    s = get_dtime();
    for (i = 0; i < NUM; i++) {
      RDMA_Isend(data + i * (size/NUM), size/NUM, NULL, 0, 2, &comm, &req[i]);
    }
    for (i = 0; i < NUM; i++) {
      RDMA_Wait(&req[i]);
    }
    e = get_dtime();
    printf("i=%d\n", j);
    printf("Send: %d[MB]  %f %f GB/s\n", (size/1000000) ,  e - s, (size/1000000000.0  )/(e - s));
    sleep(SLP);
  }
  ee = get_dtime();
  sleep(1);
  printf("Send: %d[MB]  %f %f GB/s\n", (size/1000000) * ITE ,  ee - ss, (size/1000000000.0 * ITE )/(ee -  ss));
  return 0;
}

int get_tag(void)
{
  char *ip;
  int tag = 0;
  int i;
  ip = get_ip_addr("ib0");
  /*use last three ip octet for the message tag.                                                                                                                                
    Fisrt octet is passed.
  */
  atoi(strtok(ip, "."));
  tag = atoi(strtok(NULL, "."));
  for (i = 0; i < 2; i++) {
    tag = tag * 1000;
    tag = tag + atoi(strtok(NULL, "."));
  }
  return tag;
}





