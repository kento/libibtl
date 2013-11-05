#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include "fdmi.h"
#include "common.h"
#include "transfer.h"
#include "scr_list_queue.h"
#include "fdmi_util.h"
#include "pgz.h"
#include "ibvio_common.h"

#define COMPRESS 0
#define BUF_SIZE (2 * 1024 * 1024 * 1024L)

struct write_args {
  int id;
  char *path;
  char *addr;
  uint64_t size; 
  uint64_t offset;;
};

int dump_ckpt(struct write_args *wa);
ssize_t write_ckpt(const char* file, int fd, const void* buf, size_t size);
int simple_write(struct write_args *wa);
int compress_write(struct write_args *wa) ;
static struct write_args* get_write_arg(scr_lq *q, int id);

pthread_mutex_t compress_mutex = PTHREAD_MUTEX_INITIALIZER;
int comp_count = 0;
pthread_mutex_t dump_mutex = PTHREAD_MUTEX_INITIALIZER;
int dump_count = 0;
pthread_t dump_thread;

uint64_t buff_size = BUF_SIZE;
uint64_t chunk_size = CHUNK_SIZE;
uint64_t allocated_size = 0;

char data[TEST_BUFF_SIZE];


struct ibvio_sfile_info
{
  int stat;
  char path[256];
  char *cache;
};

struct ibvio_sopen_info {
  char fd;
  struct ibvio_sfile_info *file_info;
};
struct ibvio_sopen_info open_info[1024];

static int ibvio_sopen(FMI_Status *stat)
{
  FMI_Request req;
  struct ibvio_open iopen;
  int fd;

  fdmi_verbs_irecv(&iopen, sizeof(struct ibvio_open), FMI_BYTE, stat->FMI_SOURCE, stat->FMI_TAG, FMI_COMM_WORLD, &req, FDMI_ABORT);
  fdmi_verbs_wait(&req, NULL, FDMI_ABORT);

  iopen.fd = open(iopen.path, iopen.flags, iopen.mode);

  fdmi_verbs_isend(&iopen, sizeof(struct ibvio_open), FMI_BYTE, stat->FMI_SOURCE, stat->FMI_TAG, FMI_COMM_WORLD, &req, FDMI_ABORT);
  fdmi_verbs_wait(&req, NULL, FDMI_ABORT); 


  open_info[iopen.fd].file_info = (struct ibvio_sfile_info *)malloc(sizeof(struct ibvio_sfile_info));
  open_info[iopen.fd].file_info->cache =  (char *)malloc(BUF_SIZE);

  memset(open_info[iopen.fd].file_info->cache, 0, BUF_SIZE);

  fdmi_dbg("OPEN: path: %s, flags: %d, mode: %d", iopen.path, iopen.flags, iopen.mode);
  return;
}

static int ibvio_swrite(int fd, FMI_Status *stat)
{
  FMI_Request req;
  struct ibvio_open iopen;
  char *buf;
  int write_size, current_recv_size = 0;
  int write_chunk_size, chunk_size = IBVIO_CHUNK_SIZE;
  double s, t = 0;

  fdmi_dbg("Write start");

  fdmi_verbs_irecv(&iopen, sizeof(struct ibvio_open), FMI_BYTE, stat->FMI_SOURCE, stat->FMI_TAG, FMI_COMM_WORLD, &req, FDMI_ABORT);
  fdmi_verbs_wait(&req, NULL, FDMI_ABORT);

  fdmi_dbg("fd: %d, count: %d", fd, iopen.count);

  while (current_recv_size < iopen.count) {
    if (current_recv_size + chunk_size > iopen.count) {
      chunk_size = iopen.count - current_recv_size;
    }
    fdmi_verbs_irecv(open_info[fd].file_info->cache + current_recv_size, chunk_size, FMI_BYTE, stat->FMI_SOURCE, stat->FMI_TAG, FMI_COMM_WORLD, &req, FDMI_ABORT);
    if (!IBVIO_DELAYED_WRITE) {
      if (current_recv_size > 0) {
	s = fdmi_get_time();
	if (write(fd, open_info[fd].file_info->cache + write_size, write_chunk_size) < 0) {
	  fdmi_err("write error");
	}
	t += fdmi_get_time() - s;
	write_size += write_chunk_size;
      }
    }
    fdmi_verbs_wait(&req, NULL, FDMI_ABORT);

    current_recv_size += chunk_size;
    write_chunk_size = chunk_size;
  }

  /*Write the last chunk*/

  if (!IBVIO_DELAYED_WRITE) {
    s = fdmi_get_time();
    if (write(fd, open_info[fd].file_info->cache + write_size, write_chunk_size) < 0) {
      fdmi_err("write error");
    }
    write_size += write_chunk_size;
    fsync(fd);
    t += fdmi_get_time() - s;
  }



  fdmi_verbs_isend(&iopen, sizeof(struct ibvio_open), FMI_BYTE, stat->FMI_SOURCE, stat->FMI_TAG, FMI_COMM_WORLD, &req, FDMI_ABORT);
  fdmi_verbs_wait(&req, NULL, FDMI_ABORT); 

  if (IBVIO_DELAYED_WRITE) {
    s = fdmi_get_time();
    if (write(fd, open_info[fd].file_info->cache, iopen.count) < 0) {
      fdmi_err("write error");
    }
    fsync(fd);
    t += fdmi_get_time() - s;
  }
  fdmi_dbg("Finished Write: Write: time: %f, bw: %f GB/s", t, iopen.count / t / 1000000000.0);

  return;
}

int main(int argc, char **argv) 
{
  FMI_Request req;
  FMI_Status stat;
  int flag;

  fdmi_verbs_init(&argc, &argv);
  while (1) {
    fdmi_verbs_iprobe(FMI_ANY_SOURCE, FMI_ANY_TAG, FMI_COMM_WORLD, &flag, &stat);
    if (flag) {
      int op, fd;
      op = IBVIO_OP_NOOP;
      ibvio_deserialize_tag(&op, &fd, stat.FMI_TAG);
      fdmi_dbg("Probe data %s from rank: %d, tag: %d => op: %d, fd: %d", data, stat.FMI_SOURCE, stat.FMI_TAG, op, fd);
      switch (op) {
      case IBVIO_OP_OPEN:
	ibvio_sopen(&stat);
	break;
      case IBVIO_OP_WRITE:
	ibvio_swrite(fd, &stat);
	break;
      case IBVIO_OP_READ:
	break;
      case IBVIO_OP_CLOSE:
	break;
      case IBVIO_OP_NOOP:
	usleep(1000);
	break;
      }

    }
    /* fdmi_verbs_irecv(data, TEST_BUFF_SIZE, FMI_BYTE, FMI_ANY_SOURCE, FMI_ANY_TAG, FMI_COMM_WORLD, &req, FDMI_ABORT); */
    /* fdmi_dbg("irecv"); */
    /* fdmi_verbs_wait(&req, &stat, FDMI_ABORT); */
    /* fdmi_dbg("data %s from rank: %d, tag: %d", data, stat.FMI_SOURCE, stat.FMI_TAG); */
    /* sprintf(data, "aho"); */
    /* fdmi_verbs_isend(data, TEST_BUFF_SIZE, FMI_BYTE, stat.FMI_SOURCE, stat.FMI_TAG, FMI_COMM_WORLD, &req, FDMI_ABORT); */
    /* fdmi_verbs_wait(&req, &stat, FDMI_ABORT); */
  }
  sleep(11111);
/*   struct RDMA_communicator comm; */
/*   struct RDMA_request *req1, *req2; */
/*   struct scr_transfer_ctl ctl[NUM_BUFF]; */
/*   struct scr_transfer_ctl file_info; */
/*   file_info.size = 0; */

/*   char *data[NUM_BUFF]; */
/*   int i; */
/*   int buff_index = 0; */
/*   uint64_t recv_size = -1; */
/*   uint64_t ckpt_size = 0; */
/*   pthread_t thread; */
/*   scr_lq wq; /\*enqueue incomming PFS chkp data*\/ */

/*   struct write_args *pendding_write_args = NULL; */
/*   int req_num = 2; */

/*   struct RDMA_request req[req_num]; */
/*   int req_id = 0; */
/*   int ctl_tag; */
/*   double rdma_s, rdma_e; */
/*   int count = 0; */
/*   int request_status = 0; */


/*   RDMA_Passive_Init(&comm); */
/*   scr_lq_init(&wq); */


/*   while(1) { */
/*     /\*Colloect RDMA read requests until buffer exceeds*\/ */
/*     usleep(1000); */
/*     while (allocated_size < buff_size) { */
/*       struct write_args *wa; */
/*       request_status = RDMA_Iprobe(RDMA_ANY_SOURCE, 0, &comm); */
/*       if (!request_status) { */
/*         break; */
/*       } */
/*       RDMA_Recv(&file_info, sizeof(file_info), NULL, RDMA_ANY_SOURCE, 0, &comm); */
/*       ibtl_dbg("PATH: %s, ID: %d size:%lu\n", file_info.path, file_info.id, file_info.size); */

/*       wa = (struct write_args*)malloc(sizeof(struct write_args)); */
/*       wa->id = file_info.id; */
/*       wa->path = (char *)malloc (128); */
/*       sprintf(wa->path, "%s", file_info.path); */
/*       wa->size = file_info.size; */
/*       wa->offset = 0; */
/*       allocated_size += file_info.size; */
/*       wa->addr = RDMA_Alloc(file_info.size); */
/*       //      fprintf(stderr, "%lu/%lu\n", allocated_size, buff_size ); */
/*       scr_lq_enq(&wq, wa); */
/*     } */

/*     while (RDMA_Iprobe(RDMA_ANY_SOURCE, 1, &comm)) { */
/*       struct write_args* recv_wa; */
/*       req_id = RDMA_Reqid(&comm, 1); */
/*       recv_wa = get_write_arg(&wq, req_id); */
/*       //      fprintf(stderr, "req_id: %d, recv_wa: %p\n", req_id, recv_wa); */
/*       //      fprintf(stderr, "%d: Offset: %lu, size: %lu, req_id:%d\n", req_id, recv_wa->offset, recv_wa->size, req_id);  */
/*       //     ibtl_dbg("%d: Offset: %lu, size: %lu, req_id:%d", req_id, recv_wa->offset, recv_wa->size, req_id);  */

/*       RDMA_Recv(recv_wa->addr + recv_wa->offset, 0, NULL, req_id, 1, &comm); */
/*       recv_wa->offset += chunk_size; */
/*       if (recv_wa->offset > recv_wa->size) { */
/*         recv_wa->offset = recv_wa->size; */
/*       } */
/*       //      fprintf(stderr, "%d: Offset: %lu, size: %lu, req_id:%d\n", req_id, recv_wa->offset, recv_wa->size, req_id); */

/*       if (recv_wa->size == recv_wa->offset) { */
/*       /\*If I receive the all data to the buffer, I start writing the data to a file system *\/ */
/* 	ibtl_dbg("start"); */
/* 	RDMA_Send(recv_wa->addr + recv_wa->offset - CHUNK_SIZE, CHUNK_SIZE, NULL, req_id, 1, &comm); */
/* 	ibtl_dbg("end"); */
/* #if COMPRESS == 1 */
/*         pthread_mutex_lock(&compress_mutex); */
/*         if (pthread_create(&thread, NULL, (void *)compress_write, recv_wa)) { */
/*           fprintf(stderr, "RDMA lib: SEND: ERROR: pthread create failed @ %s:%d", __FILE__, __LINE__); */
/*           exit(1); */
/*         } */
/* #else */

/*         pthread_mutex_lock(&dump_mutex); */
/*         if (pthread_create(&thread, NULL, (void *)simple_write, recv_wa)) { */
/*           fprintf(stderr, "RDMA lib: SEND: ERROR: pthread create failed @ %s:%d", __FILE__, __LINE__); */
/*           exit(1); */
/*         } */
/* #endif */
        
/*         if (pthread_detach(thread)) { */
/*           fprintf(stderr, "RDMA lib: SEND: ERROR: pthread detach failed @ %s:%d", __FILE__, __LINE__); */
/*           exit(1); */
/*         } */
/*         scr_lq_remove(&wq, recv_wa); */

/*       } else if  (recv_wa->size <= recv_wa->offset) { */
/*         fprintf(stderr, "Received size exceeded the actual file size: file size=%lu, Received size=%lu\n", recv_wa->size, recv_wa->offset); */
/*       } */
/*     } */
/*   } */
/*   return 1; */
}

static struct write_args* get_write_arg(scr_lq *q, int id)
{
  struct write_args* wa;
  struct write_args* war = NULL;
  scr_lq_init_it(q);
  while ((wa = (struct write_args*)scr_lq_next(q)) != NULL) {
    if (wa->id == id) {
      war = wa;
      break;
    }
  }
  scr_lq_fin_it(q);
  return war;
}


int free_write_args(struct write_args *wa) {
  /* free(wa->path);                                                                                                                 */
  /* RDMA_Free(wa->addr);                                                                                                            */
  /* allocated_size -= wa->size; */
  /* free(wa);                                                                                                                       */

  return 1;
}

int simple_write(struct write_args *wa) 
{
  int fd;
  int n_write = 0;
  int n_write_sum = 0;
  double write_s, write_e;
  char *path = wa->path;
  char *addr = wa->addr;
  int size   = wa->size;

  write_s = fdmi_get_time();
  //  fd = open(path, O_WRONLY | O_APPEND | O_CREAT, S_IREAD | S_IWRITE); 
  fd = open(path, O_WRONLY | O_CREAT, S_IREAD | S_IWRITE); 
  if (fd <= 0) {
    fprintf(stderr, "error(%d): path: |%s|, size: %d, fd=%d\n",  errno, path, size, fd);
    exit(1);
  }
  do {
    n_write = write(fd, (char*)addr + n_write_sum, size - n_write_sum);
    if (n_write == -1) {
      fprintf(stderr, "Write error: %d\n", errno);
      exit(1);
    }
    n_write_sum += n_write;
    if (n_write_sum >= size) break;
  } while(n_write > 0);
  fsync(fd);
  close(fd);
  write_e = fdmi_get_time();
  free_write_args(wa);
  pthread_mutex_unlock(&dump_mutex);
  fprintf(stderr, "OVLP: Write: Time: %f bw:%f GB/s size: %d \n", write_e - write_s, n_write_sum / (write_e - write_s) / 1000000000.0, n_write_sum);
  //  fprintf(stderr, "OVLP: WRIT %f %d\n", write_e, dump_count++);
  //  fprintf(stderr, "OVLP: WRIT \n");
}

int compress_write(struct write_args *wa) 
{
  struct write_args *cwa;
  double comp_s, comp_e;

  //  fprintf(stderr, "Checkpoint path: %s, %p, %d\n", wa->path, wa->addr, wa->size);
  //  fprintf(stderr, "ST.");
  cwa = (struct write_args*)malloc(sizeof(struct write_args));
  // =======
  comp_s = fdmi_get_time();
  cwa->size = pcompress(&(cwa->addr), wa->addr, wa->size);
  comp_e = fdmi_get_time();
  //  fprintf(stderr, "OVLP: COMP %f %d %f %f\n", comp_s, comp_count, comp_e, comp_e - comp_s);
  //  fprintf(stderr, "OVLP: COMP %f %d\n", comp_e, comp_count++);
  //  fprintf(stderr, "OVLP: COMP \n");
  //  cwa->size = wa->size;
  //  cwa->addr = wa->addr;
  // =======
  cwa->path = (char *)malloc(256);
  sprintf(cwa->path, "%s.gz", wa->path);
  
  {
    pthread_mutex_lock(&dump_mutex);
    //  dump(wa->path, wa->addr, wa->size);
    if (pthread_create(&dump_thread, NULL, (void *)dump_ckpt, cwa)) {
      fprintf(stderr, "RDMA lib: SEND: ERROR: pthread create failed @ %s:%d", __FILE__, __LINE__);
      exit(1);
    }
    if (pthread_detach(dump_thread)) {
      fprintf(stderr, "RDMA lib: SEND: ERROR: pthread detach failed @ %s:%d", __FILE__, __LINE__);
      exit(1);
    }
  }
  
  free_write_args(wa);
  pthread_mutex_unlock(&compress_mutex);
}

int dump_ckpt(struct write_args *cwa)
{
  int fd;
  int n_write = 0;
  int n_write_sum = 0;
  double write_s, write_e;
  char *path = cwa->path;
  char *addr = cwa->addr;
  int size   = cwa->size;

  write_s = fdmi_get_time();
  fd = open(path, O_WRONLY | O_APPEND | O_CREAT, S_IREAD | S_IWRITE); 
  //  fprintf(stderr, "path: |%s|, size: %d, fd=%d\n",  path, size, fd);
  if (fd <= 0) {
    fprintf(stderr, "error(%d): path: |%s|, size: %d, fd=%d\n",  errno, path, size, fd);
    exit(1);
  }
  do {
    //    fprintf(stderr, "Write: addr:%lu, off:%lu\n", addr, n_write_sum);
    n_write = write(fd, (char*)addr + n_write_sum, size - n_write_sum);
    if (n_write == -1) {
      fprintf(stderr, "Write error: %d\n", errno);
      exit(1);
    }
    n_write_sum += n_write;
    //    fprintf(stderr, "Write: %d sum=%d, size=%d\n", n_write, n_write_sum, size);
    if (n_write_sum >= size) break;
  } while(n_write > 0);
  fsync(fd);
  close(fd);
  write_e = fdmi_get_time();
  //  fprintf(stderr, "OVLP: WRIT %f %d %f %f\n", write_s, dump_count, write_e, write_e - write_s);
  //  fprintf(stderr, "OVLP: WRIT %f %d\n", write_e, dump_count++);
  //  fprintf(stderr, "OVLP: WRIT \n");
  free(cwa->path);
  free(cwa->addr);
  free(cwa);
  pthread_mutex_unlock(&dump_mutex);
}
