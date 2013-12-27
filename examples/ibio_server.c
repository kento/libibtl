#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>

#include "fdmi.h"
#include "common.h"
#include "transfer.h"
#include "scr_list_queue.h"
#include "fdmi_util.h"
#include "pgz.h"
#include "ibio_common.h"

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

struct ibvio_sread_info {
  int fd;
  FMI_Status stat;
};

struct ibvio_sopen_info {
  char fd;
  /*For write operation*/
  size_t write_count;
  size_t current_write_count;
  size_t current_recv_size;
  pthread_t write_thread;
  pthread_mutex_t fastmutex;
  double timestamp;
  /*For read operation*/
  size_t read_count;
  size_t current_read_count;
  size_t current_send_size;
  pthread_t read_thread;
  pthread_mutex_t read_mutex;
  double read_timestamp;

  struct ibvio_sfile_info *file_info;
};
struct ibvio_sopen_info open_info[1024];

static int ibvio_sopen(FMI_Status *stat)
{
  FMI_Request req;
  struct ibvio_open iopen;
  int fd;
  double s, t;

  fdmi_verbs_irecv(&iopen, sizeof(struct ibvio_open), FMI_BYTE, stat->FMI_SOURCE, stat->FMI_TAG, FMI_COMM_WORLD, &req);
  fdmi_verbs_wait(&req, NULL);


  iopen.fd = open(iopen.path, iopen.flags, iopen.mode);


  fdmi_verbs_isend(&iopen, sizeof(struct ibvio_open), FMI_BYTE, stat->FMI_SOURCE, stat->FMI_TAG, FMI_COMM_WORLD, &req);
  fdmi_verbs_wait(&req, NULL);   


  open_info[iopen.fd].fd = iopen.fd;
  open_info[iopen.fd].file_info = (struct ibvio_sfile_info *)malloc(sizeof(struct ibvio_sfile_info));
  open_info[iopen.fd].file_info->cache =  (char *)malloc(BUF_SIZE);
  open_info[iopen.fd].write_count = 0;
  open_info[iopen.fd].current_write_count = 0;
  pthread_mutex_init(&open_info[iopen.fd].fastmutex, NULL);
  open_info[iopen.fd].current_recv_size = 0;

  //  memset(open_info[iopen.fd].file_info->cache, 0, BUF_SIZE);  


  fdmi_dbg("OPEN: path: %s, flags: %d, mode: %d, time: %f", iopen.path, iopen.flags, iopen.mode, t);
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

  fdmi_verbs_irecv(&iopen, sizeof(struct ibvio_open), FMI_BYTE, stat->FMI_SOURCE, stat->FMI_TAG, FMI_COMM_WORLD, &req);
  fdmi_verbs_wait(&req, NULL);

  //  fdmi_dbg("fd: %d, count: %d", fd, iopen.count);

  while (current_recv_size < iopen.count) {
    if (current_recv_size + chunk_size > iopen.count) {
      chunk_size = iopen.count - current_recv_size;
    }
    fdmi_verbs_irecv(open_info[fd].file_info->cache + current_recv_size, chunk_size, FMI_BYTE, stat->FMI_SOURCE, stat->FMI_TAG, FMI_COMM_WORLD, &req);
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
    fdmi_verbs_wait(&req, NULL);

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


  fdmi_verbs_isend(&iopen, sizeof(struct ibvio_open), FMI_BYTE, stat->FMI_SOURCE, stat->FMI_TAG, FMI_COMM_WORLD, &req);
  fdmi_verbs_wait(&req, NULL); 

  if (IBVIO_DELAYED_WRITE) {
    s = fdmi_get_time();
    if (write(fd, open_info[fd].file_info->cache, iopen.count) < 0) {
      fdmi_err("write error");
    }
    fsync(fd);
    t += fdmi_get_time() - s;
  }
  fdmi_dbg("WRITE: time: %f, bw: %f GB/s", t, iopen.count / t / 1000000000.0);

  return;
}

static int ibvio_swrite_begin(int fd, FMI_Status *stat)
{
  FMI_Request req;
  struct ibvio_open iopen;

  fdmi_verbs_irecv(&iopen, sizeof(struct ibvio_open), FMI_BYTE, stat->FMI_SOURCE, stat->FMI_TAG, FMI_COMM_WORLD, &req);
  fdmi_verbs_wait(&req, NULL);

  open_info[fd].write_count = iopen.count;
  open_info[fd].current_recv_size = 0;
  open_info[fd].current_write_count = 0;

  return;
}

static void* ibvio_swrite_chunk_thread(void *arg)
{
  struct ibvio_sopen_info *oinfo;
  int write_chunk_size = IBVIO_CHUNK_SIZE;
  double s, t;

  oinfo = (struct ibvio_sopen_info *)arg;

  pthread_mutex_lock(&oinfo->fastmutex);
  if (oinfo->current_write_count + write_chunk_size > oinfo->write_count) {
    write_chunk_size = oinfo->write_count - oinfo->current_write_count;
  }

  s = fdmi_get_time();
  if (write(oinfo->fd, oinfo->file_info->cache + oinfo->current_write_count, write_chunk_size) < 0) {
      fdmi_err("write error");
  }
  fsync(oinfo->fd);
  t = fdmi_get_time() - s;
  oinfo->current_write_count += write_chunk_size;
  fdmi_dbg("fd: %d, %f GB/s  (%f p)", oinfo->fd, write_chunk_size / t / 1000000000.0, oinfo->current_write_count / (float)oinfo->write_count);
  pthread_mutex_unlock(&oinfo->fastmutex);
  
  return;
}

static void ibvio_run_thread(pthread_t *thread, void* (*start_routine)(void *), void *arg)
{

  if (pthread_create(thread, NULL, start_routine, arg)) {
    fdmi_dbg(stderr, "pthread create failed @ %s:%d", __FILE__, __LINE__);
    exit(1);
  }
  
  if (pthread_detach(*thread)) {
    fdmi_dbg(stderr, "pthread detach failed @ %s:%d", __FILE__, __LINE__);
    exit(1);
  }

  return;
}

static int ibvio_swrite_chunk(int fd, FMI_Status *stat)
{
  FMI_Request req;
  struct ibvio_open iopen;
  struct ibvio_sopen_info *oinfo;
  char *buf;
  int write_size, current_recv_size = 0;
  int write_chunk_size, chunk_size = IBVIO_CHUNK_SIZE;
  double s, t = 0;

  oinfo = &open_info[fd];

  if (oinfo->current_recv_size == 0) {
    oinfo->timestamp = fdmi_get_time();
  }

  if (oinfo->current_recv_size + chunk_size > oinfo->write_count) {
    chunk_size = oinfo->write_count - oinfo->current_recv_size;
  }

  fdmi_verbs_irecv(oinfo->file_info->cache + oinfo->current_recv_size, chunk_size, FMI_BYTE, stat->FMI_SOURCE, stat->FMI_TAG, FMI_COMM_WORLD, &req);
  fdmi_verbs_wait(&req, NULL);
  oinfo->current_recv_size += chunk_size;

  ibvio_run_thread(&(oinfo->write_thread), ibvio_swrite_chunk_thread, oinfo);

  if (oinfo->current_recv_size == oinfo->write_count) {
    /*If this is the last chunk, reply completion msg*/
    while (oinfo->current_write_count != oinfo->write_count) {usleep(1000);}
    fsync(fd);
    fdmi_verbs_isend(&iopen, sizeof(struct ibvio_open), FMI_BYTE, stat->FMI_SOURCE, stat->FMI_TAG, FMI_COMM_WORLD, &req);
    fdmi_verbs_wait(&req, NULL); 
    t = fdmi_get_time() - oinfo->timestamp;
    fdmi_dbg("WRITE: fd: %d, time: %f, count %f GB, bw: %f GB/s", fd, t, oinfo->write_count / 1000000000.0, oinfo->write_count / t / 1000000000.0);
  }

  return;
}



static int ibvio_sread(int fd, FMI_Status *stat)
{
  FMI_Request req;
  struct ibvio_open iopen;
  char *buf;
  int read_size = 0, current_send_size = 0;
  int read_chunk_size = IBVIO_CHUNK_SIZE, chunk_size = IBVIO_CHUNK_SIZE;
  double s, t = 0;

  
  pthread_mutex_lock(&open_info[fd].fastmutex);
  //  fdmi_dbg("READ: start");
  fdmi_verbs_irecv(&iopen, sizeof(struct ibvio_open), FMI_BYTE, stat->FMI_SOURCE, stat->FMI_TAG, FMI_COMM_WORLD, &req);
  fdmi_verbs_wait(&req, NULL);

  fdmi_dbg("READ: fd: %d, count: %d, start", fd, iopen.count);


  if (!IBVIO_CACHE_READ) {
    s = fdmi_get_time();
    if (read(fd, open_info[fd].file_info->cache + read_size, read_chunk_size) < 0) {
      fdmi_err("read error");
    }
    t += fdmi_get_time() - s;
  }
  read_size += read_chunk_size;

  
  while (current_send_size < iopen.count) {
    if (current_send_size + chunk_size > iopen.count) {
      chunk_size = iopen.count - current_send_size;
    }
    fdmi_verbs_isend(open_info[fd].file_info->cache + current_send_size, chunk_size, FMI_BYTE, stat->FMI_SOURCE, stat->FMI_TAG, FMI_COMM_WORLD, &req);
    current_send_size += chunk_size;

    if (read_size < iopen.count) {
      if (!IBVIO_CACHE_READ) {
	s = fdmi_get_time();
	if (read(fd, open_info[fd].file_info->cache + read_size, read_chunk_size) < 0) {
	  fdmi_err("read error");
	}
	t += fdmi_get_time() - s;
      }
      read_size += read_chunk_size;
    }

    fdmi_verbs_wait(&req, NULL);
  }

  fdmi_dbg("READ: fd: %d, time: %f, count: %f GB, bw: %f GB/s", fd, t, iopen.count / 1000000000.0, iopen.count / t / 1000000000.0);
  pthread_mutex_unlock(&open_info[fd].fastmutex);
  return;
}

static void* ibvio_sread_thread(void* arg)
{
  struct ibvio_sread_info *rinfo;
  
  rinfo = (struct ibvio_sread_info*) arg;
  ibvio_sread(rinfo->fd, &rinfo->stat);
  
  return;
}


static int ibvio_sread_async(int fd, FMI_Status *stat)
{
  struct ibvio_sopen_info *oinfo;
  struct ibvio_sread_info *rinfo;

  oinfo = &open_info[fd];
  rinfo = (struct ibvio_sread_info*)malloc(sizeof(struct ibvio_sread_info));
  rinfo->fd = fd;
  rinfo->stat = *stat;

  ibvio_run_thread(&(oinfo->read_thread), ibvio_sread_thread, rinfo);
  return;
}



int main(int argc, char **argv) 
{
  FMI_Request req;
  FMI_Status stat;
  int flag;
  int num_c = 0;
  int source;

  if (argc == 2) {
    num_c = atoi(argv[1]);
  }
  
  fdmi_verbs_init(&argc, &argv);
  while (1) {
    if (num_c > 0) {
      source = 0;
    } else {
      source = FMI_ANY_SOURCE;
    }
    fdmi_verbs_iprobe(FMI_ANY_SOURCE, FMI_ANY_TAG, FMI_COMM_WORLD, &flag, &stat);
    if (num_c > 0) {
      source = (source + 1) % num_c;
    }
    if (flag) {
      int op, fd;
      op = IBVIO_OP_NOOP;
      ibvio_deserialize_tag(&op, &fd, stat.FMI_TAG);
      //      fdmi_dbg("Probe data %s from rank: %d, tag: %d => op: %d, fd: %d", data, stat.FMI_SOURCE, stat.FMI_TAG, op, fd);
      switch (op) {
      case IBVIO_OP_OPEN:
	ibvio_sopen(&stat);
	break;
      case IBVIO_OP_WRITE:
	ibvio_swrite(fd, &stat);
	break;
      case IBVIO_OP_WRITE_BEGIN:
	ibvio_swrite_begin(fd, &stat);
	break;
      case IBVIO_OP_WRITE_CHUNK:
	ibvio_swrite_chunk(fd, &stat);
	break;
      case IBVIO_OP_READ:
	ibvio_sread_async(fd, &stat);
	break;
      case IBVIO_OP_CLOSE:
	break;
      case IBVIO_OP_NOOP:
	usleep(1000);
	break;
      }
    }
  }
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
