#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <rdma/rdma_cma.h>



#ifndef RDMA_PORT
#define RDMA_PORT 10150
#endif



#ifndef RDMA_BUF_NUM_C
#define RDMA_BUF_NUM_C (1)
#endif

#ifndef RDMA_CLIENT_NUM_S
#define RDMA_CLIENT_NUM_S (1)
#endif

#ifndef MAX_RDMA_BUF_SIZE_C
#define MAX_RDMA_BUF_SIZE_C (5 * 1000 * 1000)
#endif

//-------------

//#ifndef MAX_RDMA_LOCKED_MEMORY_C
//#define MAX_RDMA_LOCKED_MEMORY_C (1*1000*1000)
//#endif

//#ifndef MAX_RDMA_LOCKED_MEMORY_S
//#define MAX_RDMA_LOCKED_MEMORY_S (100*1000)
//#endif
//-------------


#ifndef RDMA_BUF_SIZE_C
#define RDMA_BUF_SIZE_C ((MAX_RDMA_BUF_SIZE_C)/(RDMA_CLIENT_NUM_S))
#endif


#ifndef HASH_TABLE_LEN
#define HASH_TABLE_LEN RDMA_CLIENT_NUM_S
#endif

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL (2)
#endif

#ifndef DEBUG_RDMA
#define DEBUG_RDMA (1)
#endif

#ifndef DEBUG_TRANS
#define DEBUG_TRANS (2)
#endif

#define TEST_NZ(x) do { if ( (x)) die("error: " #x " failed (returned non-zero)." ); } while (0)
#define TEST_Z(x)  do { if (!(x)) die("error: " #x " failed (returned zero/null)."); } while (0)
//#define debug(p,x)  if ((x) >= DEBUG_LEVEL){ printf("%d:", get_pid());p; }
#define debug(p,x)  if ((x) >= DEBUG_LEVEL){ p; }


enum ctl_msg_type {
  MR_INIT,
  MR_INIT_ACK,
  MR_CHUNK,
  MR_CHUNK_ACK,
  MR_FIN,
  MR_FIN_ACK
};

struct control_msg {
  enum ctl_msg_type cmt;

  union {
    struct ibv_mr mr;
  } data;
  union {
    uint64_t buff_size; // for MR_INIT
    uint32_t mr_size; // for MR_CHUNK
    int tag; // for MR_FIN
  } data1 ;
};

struct RDMA_message {
  char* buff;
  uint64_t size;
  int tag;
};

struct RDMA_communicator {
  struct rdma_event_channel *ec;
  struct rdma_cm_id *cm_id;
};

struct RDMA_param {
  char* host;
};

struct context {
  struct ibv_context *ctx;
  struct ibv_pd *pd;
  struct ibv_cq *cq;
  struct ibv_comp_channel *comp_channel;

  pthread_t cq_poller_thread;
};

struct connection {
  struct rdma_cm_id *id;
  struct ibv_qp *qp;

  int connected;
      
  struct ibv_mr *recv_mr;
  struct ibv_mr *send_mr;
  struct ibv_mr *rdma_msg_mr;

  struct ibv_mr peer_mr;

  struct control_msg *recv_msg;
  struct control_msg *send_msg;

  char *rdma_msg_region;
};



void die(const char *reason);
const char *rdma_err_status_str(enum ibv_wc_status status);
const char *event_type_str(enum rdma_cm_event_type event);
//int send_control_msg (struct connection *conn, struct control_msg *cmsg);
//void post_receives(struct connection *conn);
int rdma_active_init(struct RDMA_communicator *comm, struct RDMA_param *param, uint32_t mr_num);

int recv_ctl_msg(struct connection *conn, enum ctl_msg_type *cmt, uint64_t *data);
int send_ctl_msg (struct connection *conn, enum ctl_msg_type cmt, uint32_t mr_index, uint64_t data);
void register_rdma_msg_mr(int mr_index, void* addr, uint32_t size);

int post_recv_ctl_msg(struct connection *conn);













