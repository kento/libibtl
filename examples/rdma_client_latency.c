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
#define SLP 0

int get_tag(void);
//char* get_ip_addr (char* interface);

int main(int argc, char **argv)
{
  char* host;
  char* data;
  uint64_t size;
  //  int flag1, flag2;
  int flag1;
  double s,e;
  double ss,ee;
  if (argc < 2) {
    printf("./rdma_client_test <host> <send size(Bytes)>\n");
    exit(1);
  }
  host = argv[1];
  size = atoi(argv[2]);
  
  struct  RDMA_communicator comm;
  struct  RDMA_param param;
  param.host = host;

  s = get_dtime();
  RDMA_Active_Init(&comm, &param);
  e = get_dtime();

  /* ===== */
  //  data = (char*)malloc(size);
  data = (char*)RDMA_Alloc(size);

  int i, j;
  char * a;
  flag1= 0;
  for (i=0; i <= size-2; i++) {
    //data[i] = (char) (i % 26 + 'a');
    data[i] = 'x';
    //    a = data[i];
    //    data[i] = a;
  }
  data[size-1] += '\0';

  
  printf("Initialization: %f\n",e - s);
  //  sleep(3);  
  ss = get_dtime();
  // RDMA_Sendr_ns(data, size, get_tag(), &comm);
  struct RDMA_request req[NUM];
  
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
  //  sleep(2);
  return 0;
  //  RDMA_Active_Finalize(&comm);

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

/*
char* get_ip_addr (char* interface)
{
  char *ip;
  int fd;
  struct ifreq ifr;
  fd = socket(AF_INET, SOCK_STREAM, 0);
  ifr.ifr_addr.sa_family = AF_INET;
  strncpy(ifr.ifr_name, interface, IFNAMSIZ-1);
  ioctl(fd, SIOCGIFADDR, &ifr);
  printf("%s\n", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
  ip = inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
  return ip;
  }*/




