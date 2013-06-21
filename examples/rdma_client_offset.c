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

int get_tag(void);

int main(int argc, char **argv)
{
  char* host;
  char* data;
  uint64_t size;
  int flag1;
  double s,e;
  double ss,ee;

  /*
  if (argc < 2) {
    printf("./rdma_client_test <host> <send size(Bytes)>\n");
    exit(1);
  }
  host = argv[1];
  */
  size = atoi(argv[1]);
  
  struct  RDMA_communicator comm;
  struct  RDMA_param param;
  //  param.host = host;

  s = get_dtime();
  RDMA_Active_Init(&comm, &param);
  e = get_dtime();

  /* ===== */
  data = (char*)RDMA_Alloc(size);

  int i, j;
  char * a;
  flag1= 0;
  for (i=0; i <= size-2; i++) {
    data[i] = 'x';
  }
  data[size-1] += '\0';
  
  printf("Initialization: %f\n",e - s);

  ss = get_dtime();
  struct RDMA_request req;
  RDMA_Isend(data, size, NULL, 0, 2, &comm, &req);
  while (RDMA_Trywait(&req) == 0);
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
