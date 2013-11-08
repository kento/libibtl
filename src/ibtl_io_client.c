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
#include "fdmi_config.h"
#include "fdmi_util.h"
#include "ibvio_common.h"

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

//#define BUF_SIZE (128 * 1024)
char data[TEST_BUFF_SIZE];

int host_ids[1024]; /*Mapping between fd:host_id */
int next_host_id = 0;

int ibtl_open(const char *pathname, int flags, int mode)
{
  struct ibvio_open iopen;
  FMI_Request req;
  FMI_Status stat;
  int tag;
  int host_id;
  struct scr_transfer_ctl *ctl;
  char local_pathname[256];
  char *hostname, *path;
  double s, e;

  if (!is_init) {
    fdmi_verbs_init(0, NULL);
  }

  s = fdmi_get_time();
  host_id = next_host_id;

  
  sprintf(local_pathname, "%s", pathname);
  hostname = strtok(local_pathname, ":");

  path     = strtok(NULL, ":");
  sprintf(iopen.path, "%s", path);
  iopen.flags = flags;
  iopen.mode  = mode;

  //  fdmi_verbs_connect(0, "rkm00.m.gsic.titech.ac.jp");
  fdmi_verbs_connect(host_id, hostname);
  
  ibvio_serialize_tag(IBVIO_OP_OPEN, 0, &tag);
  fdmi_verbs_isend(&iopen, sizeof(struct ibvio_open), FMI_BYTE, host_id, tag, FMI_COMM_WORLD, &req, 0);
  fdmi_verbs_wait(&req, &stat);

  fdmi_verbs_irecv(&iopen, sizeof(struct ibvio_open), FMI_BYTE, host_id, FMI_ANY_TAG, FMI_COMM_WORLD, &req, 0);
  fdmi_verbs_wait(&req, &stat);

  host_ids[iopen.fd] = next_host_id;
  next_host_id++;
  e = fdmi_get_time();
  /* fdmi_verbs_irecv(data, TEST_BUFF_SIZE, FMI_BYTE, FMI_ANY_SOURCE, FMI_ANY_TAG,  FMI_COMM_WORLD, &req, 0); */
  /* fdmi_verbs_wait(&req, &stat); */
  /* fdmi_dbg("data: %s: from rank: %d, tag; %d", data, stat.FMI_SOURCE, stat.FMI_TAG); */
  

  /* s = fdmi_get_time(); */
  /* sprintf(data, "small"); */
  /* fdmi_verbs_isend(data, TEST_BUFF_SIZE, FMI_BYTE, host_id, 15, FMI_COMM_WORLD, &req, 0); */
  /* fdmi_verbs_wait(&req, &stat); */
  /* fdmi_dbg("data: %s", data); */

  /* fdmi_verbs_irecv(data, TEST_BUFF_SIZE, FMI_BYTE, FMI_ANY_SOURCE, FMI_ANY_TAG,  FMI_COMM_WORLD, &req, 0); */
  /* fdmi_verbs_wait(&req, &stat); */
  /* e = fdmi_get_time(); */
  /* fdmi_dbg("data: %s: from rank: %d, tag; %d, time: %f, bw:%f", data, stat.FMI_SOURCE, stat.FMI_TAG, e - s, TEST_BUFF_SIZE / (e - s) / 1000000000 * 2);   */


  /* /\* ctl = &ctls[ctls_index]; *\/ */
  /* /\* memcpy(ctl->path, pathname, PATH_SIZE); *\/ */
  /* return ctls_index++; */
  fdmi_dbg("OPEN: time: %f", e - s);
  return iopen.fd;
}

ssize_t ibtl_dwrite(int fd, void *buf, size_t count)
{
  struct ibvio_open iopen;
  FMI_Request req;
  FMI_Status stat;
  int host_id;
  int tag;
  int current_write_size = 0;
  double s, e, t1, t2, t3;
  double st, t4;
  int chunk_size = IBVIO_CHUNK_SIZE;

  host_id = host_ids[fd];

  ibvio_serialize_tag(IBVIO_OP_WRITE, fd, &tag);
  
  iopen.count = count;
  s = fdmi_get_time();
  st = fdmi_get_time();
  fdmi_verbs_isend(&iopen, sizeof(struct ibvio_open), FMI_BYTE, host_id, tag, FMI_COMM_WORLD, &req, 0);
  fdmi_verbs_wait(&req, &stat);
  t1 = fdmi_get_time() - s;

  s = fdmi_get_time();
  while (current_write_size < count) {
    if (current_write_size + chunk_size > count) {
      chunk_size = count - current_write_size;
    }
    fdmi_verbs_isend((char *)buf + current_write_size, chunk_size, FMI_BYTE, host_id, tag, FMI_COMM_WORLD, &req, 0);
    fdmi_verbs_wait(&req, &stat);
    current_write_size += chunk_size;
  }
  t2 = fdmi_get_time() - s;

  s = fdmi_get_time();
  fdmi_verbs_irecv(&iopen, sizeof(struct ibvio_open), FMI_BYTE, host_id, FMI_ANY_TAG, FMI_COMM_WORLD, &req, 0);
  fdmi_verbs_wait(&req, &stat);
  t3 = fdmi_get_time() - s;
  t4 = fdmi_get_time() - st;
  
  fdmi_dbg("WRITE: op: %f transfer: %f, comp: %f, bw: %f GB/s", t1, t2, t3, count / t4 / 1000000000);

  return iopen.stat;
}

ssize_t ibtl_write(int fd, void *buf, size_t count)
{
  struct ibvio_open iopen;
  FMI_Request req;
  FMI_Status stat;
  int host_id;
  int tag;
  int current_write_size = 0;
  double s, e, t1, t2, t3;
  double st, t4;
  int chunk_size = IBVIO_CHUNK_SIZE;

  host_id = host_ids[fd];

  /*WRITE_BEGIN*/
  ibvio_serialize_tag(IBVIO_OP_WRITE_BEGIN, fd, &tag);
  iopen.count = count;
  s = fdmi_get_time();
  st = fdmi_get_time();
  fdmi_verbs_isend(&iopen, sizeof(struct ibvio_open), FMI_BYTE, host_id, tag, FMI_COMM_WORLD, &req, 0);
  fdmi_verbs_wait(&req, &stat);
  t1 = fdmi_get_time() - s;

  /*WRITE_CHUNK*/
  ibvio_serialize_tag(IBVIO_OP_WRITE_CHUNK, fd, &tag);
  s = fdmi_get_time();
  while (current_write_size < count) {
    if (current_write_size + chunk_size > count) {
      chunk_size = count - current_write_size;
    }
    fdmi_verbs_isend((char *)buf + current_write_size, chunk_size, FMI_BYTE, host_id, tag, FMI_COMM_WORLD, &req, 0);
    fdmi_verbs_wait(&req, &stat);
    current_write_size += chunk_size;
  }
  t2 = fdmi_get_time() - s;

  s = fdmi_get_time();
  fdmi_verbs_irecv(&iopen, sizeof(struct ibvio_open), FMI_BYTE, host_id, FMI_ANY_TAG, FMI_COMM_WORLD, &req, 0);
  fdmi_verbs_wait(&req, &stat);
  t3 = fdmi_get_time() - s;
  t4 = fdmi_get_time() - st;
  
  fdmi_dbg("WRITE: op: %f transfer: %f, comp: %f, size: %f GB,  bw: %f GB/s", t1, t2, t3, count / 1000000000.0, count / t4 / 1000000000);

  return iopen.stat;
}

ssize_t ibtl_read(int fd, void *buf, size_t count) 
{
  struct ibvio_open iopen;
  FMI_Request req;
  FMI_Status stat;
  int host_id;
  int tag;
  int current_recv_size = 0;
  double s, e, t1, t2;
  double st, t3;
  int chunk_size = IBVIO_CHUNK_SIZE;

  host_id = host_ids[fd];

  ibvio_serialize_tag(IBVIO_OP_READ, fd, &tag);
  
  iopen.count = count;
  s = fdmi_get_time();
  st = fdmi_get_time();
  fdmi_verbs_isend(&iopen, sizeof(struct ibvio_open), FMI_BYTE, host_id, tag, FMI_COMM_WORLD, &req, 0);
  fdmi_verbs_wait(&req, &stat);
  t1 = fdmi_get_time() - s;

  s = fdmi_get_time();
  while (current_recv_size < count) {
    if (current_recv_size + chunk_size > count) {
      chunk_size = count - current_recv_size;
    }
    fdmi_verbs_irecv((char *)buf + current_recv_size, chunk_size, FMI_BYTE, host_id, tag, FMI_COMM_WORLD, &req, 0);
    fdmi_verbs_wait(&req, &stat);
    current_recv_size += chunk_size;
  }
  t2 = fdmi_get_time() - s;
  t3 = fdmi_get_time() - st;
  /* s = fdmi_get_time(); */
  /* fdmi_verbs_irecv(&iopen, sizeof(struct ibvio_open), FMI_BYTE, host_id, FMI_ANY_TAG, FMI_COMM_WORLD, &req, 0); */
  /* fdmi_verbs_wait(&req, &stat); */
  /* t3 = fdmi_get_time() - s; */
  
  fdmi_dbg("READ: op: %f transfer: %f, bw: %f GB/s", t1, t2, count / t3 / 1000000000);

  return iopen.stat;
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
