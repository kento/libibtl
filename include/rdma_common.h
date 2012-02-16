#ifndef _RDMA_COMMON
#define _RDMA_COMMON

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <rdma/rdma_cma.h>
#include <semaphore.h>



#ifndef RDMA_PORT
#define RDMA_PORT 10150
#endif

#ifndef RDMA_BUF_NUM_C
#define RDMA_BUF_NUM_C (2)
#endif

#ifndef RDMA_CLIENT_NUM_S
#define RDMA_CLIENT_NUM_S (1)
#endif

#ifndef MAX_RDMA_BUF_SIZE_C
#define MAX_RDMA_BUF_SIZE_C ( 1 * 1024 * 1024 * 1024)
#endif

#ifndef RDMA_READ_UNIT_SIZE_S
#define RDMA_READ_UNIT_SIZE_S ( 1 * 1024 * 1024 * 1024)
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
  uint64_t count;


  struct rdma_cm_id *id;
  struct ibv_qp *qp;
  
  uint16_t slid;/*ibv_wc.slid*/
  enum ibv_wc_opcode opcode; /*ibv_wc.opcode*/

  struct ibv_mr *recv_mr;
  struct ibv_mr *send_mr;

  struct rdma_read_request_entry *recv_msg;
  struct rdma_read_request_entry *send_msg;

  struct rdma_read_request_entry *active_rrre;
  struct rdma_read_request_entry *passive_rrre;
};



/*
  An entry(rdma_read_request_entry) is queued in passive side RDMA request queue
  A passive side popes one of the entries form its queue and queries a RDMA READ request
*/
struct rdma_read_request_entry {
  /*
    Initialized in a passive side to determine an active source node.
  */
  struct connection* conn;
    
  /*
    ID to know which process a passive machine received the request from.
  */
  uint64_t id;  

  /*
    NOT IN USE NOW:
    To be used for client to be able to overlap RDMA read request with ibv_reg() 
    to hide the latency by ibv_reg().
  */
  uint64_t order;

  /*
    Same as a tag used by MPI library.
  */
  uint64_t tag;

  /*
    A registered active side memory region(mr) which is allowed to be read with RDMA from remote (passive side)
    mr.addr and mr.rkey to be used for the purpose. 
  */
  struct ibv_mr mr;
  
  struct ibv_mr *passive_mr;

  /*
    The lock is used to know if a RDMA request is completed with minimal CPU usage.
   */
  sem_t *is_rdma_completed;

  /*
    Size of the registered active side memory region(mr).
    But the siez can be got from ibv_mr mr.
  */
  //uint64_t size;

};

struct RDMA_request {
  sem_t is_rdma_completed;
} ;




void die(const char *reason);
const char *rdma_err_status_str(enum ibv_wc_status status);
const char *event_type_str(enum rdma_cm_event_type event);
const char *ibv_wc_opcode_str(enum ibv_wc_opcode opcode);
//int send_control_msg (struct connection *conn, struct control_msg *cmsg);
//void post_receives(struct connection *conn);


struct connection* create_connection(struct rdma_cm_id *id);
int free_connection(struct connection* conn);
int send_ctl_msg (struct connection* conn, enum ctl_msg_type cmt, struct rdma_read_request_entry *entry);
//int recv_ctl_msg(enum ctl_msg_type *cmt, uint64_t *data);
int recv_ctl_msg(enum ctl_msg_type *cmt, uint64_t *data, struct connection** conn);
int recv_wc (int num_entries, struct connection** conn);

int finalize_ctl_msg (uint32_t *cmt, uint64_t *data);

void register_rdma_msg_mr(struct connection* conn, void* addr, uint32_t size);

void dereg_mr(struct ibv_mr *mr);
struct ibv_mr* reg_mr(void* addr, uint32_t size);


int init_ctl_msg(uint32_t **cmt, uint64_t **data);

/*For accitve side*/
int rdma_active_init(struct RDMA_communicator *comm, struct RDMA_param *param, uint32_t mr_num);




/*For passive side*/
int init_rdma_buffs(uint32_t num_client);
int alloc_rdma_buffs(uint16_t id, uint64_t size);
int rdma_read(struct connection* conn, uint16_t id, uint64_t offset, uint64_t mr_size, int signal);
int get_rdma_buff(uint16_t id, char** addr, uint64_t *size);

void* rdma_passive_init(void * arg /*(struct RDMA_communicator *comm)*/);
int wait_accept();
int post_recv_ctl_msg(struct connection *conn);


int rdma_wait(struct RDMA_request *request);
#endif //_RDMA_COMMON


