/*For RDMA transfer*/
#include "ibtl_io_client.h"
#include "ibtls.h"
#include "transfer.h"
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/file.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

#include "fdmi.h"
#include "fdmi_datatype.h"
#include "fdmi_pmtv_common.h"
#include "fdmi_proc_man.h"

/*For RDMA transfer*/
#define IBTL_FILE_BUF_SIZE ((512 + 128) * 1024 * 1024)
#define NUM_BUFFS 2
#define MAX_NOFILE 1024


/* TODO: use direct I/O for improved performance */
/* TODO: compute crc32 during transfer */

#define STOPPED (1)
#define RUNNING (2)

static char*  scr_transfer_file = NULL;
static int    keep_running      = 1;
static int    state             = STOPPED;
static double bytes_per_second  = 0.0;
static double percent_runtime   = 0.0;

static size_t scr_file_buf_size = IBTL_FILE_BUF_SIZE;


/*For RDMA transfer*/
struct RDMA_communicator comm;
struct RDMA_param param;
int   is_init = 0;
void * buff[NUM_BUFFS];

static int transfer_init(void);
static int file_transfer(char *from, char* to);
static uint64_t get_file_size(char* path);
static int get_id(void);

struct scr_transfer_ctl ctls[MAX_NOFILE];
static int ctls_index = 3;

char data[4];

int ibtl_open(const char *pathname, int flags, int mode)
{
  struct scr_transfer_ctl *ctl;
  FMI_Request req;


  if (!is_init) {
    fdmi_verbs_init(0, NULL);
  }
  
  //  fdmi_verbs_connect(0, "rkm00.m.gsic.titech.ac.jp");
  fdmi_verbs_connect(0, "10.1.4.200");
                   

  sprintf(data, "te");
  fdmi_verbs_isend(data, 4, FMI_BYTE, 0, 0, FMI_COMM_WORLD, &req, 0);
  fdmi_dbg("data: %s", data);
  
  /* ctl = &ctls[ctls_index]; */
  /* memcpy(ctl->path, pathname, PATH_SIZE); */
  return ctls_index++;
}

ssize_t ibtl_write(int fd, void *buf, size_t count)
{
  /* struct scr_transfer_ctl *ctl; */
  /* size_t offset = 0; */
  /* size_t chunk_size = CHUNK_SIZE; */

  /* ctl = &ctls[fd]; */
  /* ctl->id = get_id();  */
  /* ctl->size = count; */
  /* RDMA_Send(ctl, sizeof(struct scr_transfer_ctl), NULL, ctl->id, 0, &comm); */
  /* //  ibtl_dbg("write called: id: %d, size: %d", ctl->id, sizeof(ctl)); */
  /* while(offset < count) { */
  /*   if (offset + chunk_size > count) { */
  /*     chunk_size = count - offset; */
  /*   } */
  /*   RDMA_Send(buf + offset, chunk_size, NULL, ctl->id, 1, &comm); */
  /*   offset += chunk_size; */
  /*   //    ibtl_dbg("offset: %lu, count: %lu", offset, count); */
  /* } */
  /* ibtl_dbg("start"); */
  /* RDMA_Recv(buf + offset, 0, NULL, RDMA_ANY_SOURCE, RDMA_ANY_TAG, &comm); */
  /* ibtl_dbg("end"); */
  return count;
}

ssize_t ibtl_read(int fd, void *buf, size_t count) 
{
  /* struct scr_transfer_ctl *ctl; */
  /* size_t offset = 0; */
  /* size_t chunk_size = CHUNK_SIZE; */

  /* ctl = &ctls[fd]; */
  /* ctl->id = get_id();  */
  /* ctl->size = count; */
  /* RDMA_Send(ctl, sizeof(struct scr_transfer_ctl), NULL, ctl->id, 0, &comm); */
  /* //  ibtl_dbg("write called: id: %d, size: %d", ctl->id, sizeof(ctl)); */
  /* while(offset < count) { */
  /*   if (offset + chunk_size > count) { */
  /*     chunk_size = count - offset; */
  /*   } */
  /*   RDMA_Send(buf + offset, chunk_size, NULL, ctl->id, 1, &comm); */
  /*   offset += chunk_size; */
  /*   //    ibtl_dbg("offset: %lu, count: %lu", offset, count); */
  /* } */
  return count;
}



int ibtl_close(int fd)
{
  printf("close called\n");
}

static int transfer_init(void)
{
  /* int i; */
  /* RDMA_Active_Init(&comm, &param); */
  /* for (i = 0; i < NUM_BUFFS; i++) { */
  /*   buff[i] = RDMA_Alloc(scr_file_buf_size); */
  /* } */
  return 1;
}

static int file_transfer(char *from, char* to)
{
  /* struct RDMA_request req[NUM_BUFFS], init_req; */
  /* int init = 1; */
  /* struct scr_transfer_ctl ctl; */
  /* int fd_src; */
  /* int nread = 1; */
  /* int buff_index = 0; */
  /* int i, j; */

  
  /* memcpy(ctl.path, to, PATH_SIZE); */
  /* ctl.id = get_id();  */
  /* ctl.size = get_file_size(from);   */
  /* RDMA_Send(&ctl, sizeof(ctl), NULL, ctl.id, 0, &comm); */


  /* fd_src = scr_open(from, O_RDONLY); */
  /* int send_count = 0; */
  /* for (i = 0; i < NUM_BUFF; i++) { */
  /*   //    fprintf(stderr, "read : %d ...", buff_index); */
  /*   nread = scr_read(from, fd_src, buff[buff_index], scr_file_buf_size); */
  /*   //    fprintf(stderr, "done (%d) \n", nread); */
  /*   if (!nread) { */
  /*     while (send_count > 0) { */
  /* 	buff_index = (buff_index + 1) % NUM_BUFF; */
  /* 	//	fprintf(stderr, "Wait : %d ...", buff_index); */
  /* 	RDMA_Wait(&req[buff_index]); */
  /* 	send_count--; */
  /* 	//	fprintf(stderr, "%p: DONE\n"); */
  /*     } */
  /*     return 0; */
  /*   } */
  /*   //    fprintf(stderr, "Send :  %d ... ", buff_index); */
  /*   RDMA_Isend(buff[buff_index], scr_file_buf_size, NULL, ctl.id, 1, &comm, &req[buff_index]); */
  /*   send_count++; */
  /*   //    fprintf(stderr, "DONE (count:%d)\n", send_count); */
  /*   buff_index = (buff_index + 1) % NUM_BUFF; */
  /* } */
  /* //  fprintf(stderr, "TEST !!\n"); */
  /* while(1) { */
  /*   RDMA_Wait(&req[buff_index]); */
  /*   send_count--; */
  /*   //    fprintf(stderr, "Read : %d ...", buff_index); */
  /*   nread = scr_read(from, fd_src, buff[buff_index], scr_file_buf_size); */
  /*   //    fprintf(stderr, "done (%d) \n", nread); */
  /*   if (!nread) { */
  /*     while (send_count > 0) { */
  /* 	buff_index = (buff_index + 1) % NUM_BUFF; */
  /* 	//	fprintf(stderr, "RDMA Wait1: %d  ... ", buff_index); */
  /* 	RDMA_Wait(&req[buff_index]); */
  /* 	send_count--; */
  /* 	//	fprintf(stderr, "DONE (count:%d)\n", send_count); */
  /*     } */
  /*     return 0; */
  /*   } */
  /*   //    fprintf(stderr, "Send : %d ...", buff_index); */
  /*   RDMA_Isend(buff[buff_index], scr_file_buf_size, NULL, ctl.id, 1, &comm, &req[buff_index]); */
  /*   send_count++; */
  /*   //    fprintf(stderr, "DONE (count:%d)\n", send_count); */
  /*   buff_index = (buff_index + 1) % NUM_BUFF; */
  /* } */

  /* for (j = 0; j < NUM_BUFF; j++) { */
  /*   buff_index = (buff_index + 1) % NUM_BUFF; */
  /*   RDMA_Wait(&req[buff_index]); */
  /* } */
  return 0;
}

static uint64_t get_file_size(char* path)
{
  struct stat StatBuf;
  FILE        *FilePtr = NULL;
  FilePtr = fopen(path, "r" );
  if ( FilePtr == NULL ) {
    fprintf(stderr, "Open error \n", errno);
    return -1;
  }
  fstat( fileno( FilePtr ), &StatBuf );
  fclose( FilePtr );
  return StatBuf.st_size;
}


static int get_id(void)
{ 
  char *ip;
  int fd;
  struct ifreq ifr;
  int id = 0;
  int octet;
  fd = socket(AF_INET, SOCK_STREAM, 0);
  ifr.ifr_addr.sa_family = AF_INET;
  strncpy(ifr.ifr_name, "ib0", IFNAMSIZ-1);
  ioctl(fd, SIOCGIFADDR, &ifr);
  ip = inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
  close(fd);
  strtok(ip, ".");  strtok(NULL, ".");
  id = 1000 * atoi(strtok(NULL, "."));
  id += atoi(strtok(NULL, "."));
  //  fprintf(stderr, "id: %d\n", id);
  return id;
}
