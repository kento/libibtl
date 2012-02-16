#include <time.h>
#include <sys/mman.h>

#include "common.h"
#include "rdma_server.h"
#include "buffer_table.h"
#include "hashtable.h"
#include "list_queue.h"

#define MIN_LENGTH 1

static void *poll_cq(struct RDMA_communicator *comm);
static void build_connection(struct rdma_cm_id *id);
static void build_context(struct ibv_context *verbs);
static void build_params(struct rdma_conn_param *params);
static void build_qp_attr(struct ibv_qp_init_attr *qp_attr);

static void set_envs(void);
static int free_rrre(int mode, struct rdma_read_request_entry *rrre);
static int create_rrre(struct connection *conn, struct rdma_read_request_entry *rrre);
static int post_matched_request (int target_q_id, struct rdma_read_request_entry* cur_rrre);
static void post_RDMA_read (struct connection* conn, uint64_t remote_addr, uint32_t rkey, uint64_t local_addr, uint32_t length, uint32_t lkey);


static struct context *s_ctx = NULL;

pthread_t listen_thread;

/*Queue in which RDMA requests from active sides are put */
static lq rdma_request_aq;
#define ACTIVE 0

/*Queue in which RDMA requests on this process are put */
static lq rdma_request_pq;
#define PASSIVE 10

/*Lock for post_matched_request*/
static pthread_mutex_t post_req_lock = PTHREAD_MUTEX_INITIALIZER;

static void set_envs()
{
  /*left empty for future requirment*/
  return;
}

int RDMA_Passive_Init(struct RDMA_communicator *comm) 
{
  static int thread_running = 0;

  set_envs();

  TEST_NZ(pthread_create(&listen_thread, NULL, (void *) rdma_passive_init, comm));
  wait_accept();

  //TODO: chage the below as a thread safe region.
  if (thread_running == 0) {
    thread_running = 1;
    TEST_NZ(pthread_create(&listen_thread, NULL, (void *)poll_cq, comm));
  } else {
    fprintf(stderr, "RDMA lib: RECV: Do not call RDMA_Passive_Init more than once\n");
    exit(1);
  }
  return 0;
}

static void * poll_cq(struct RDMA_communicator *comm)
{  
  int num_entries;

  struct connection* conn_send;
  struct connection* conn_recv;

  lq_init(&rdma_request_aq);
  lq_init(&rdma_request_pq);

  while (1) {
    double mm, ss, ee;
    struct rdma_read_request_entry* rrre;

    ss = get_dtime();
    num_entries = recv_wc(1, &conn_recv);
    mm = ss - ee;
    ee = get_dtime();
    debug(fprintf(stdout, "RDMA lib: RECV: recv_wc time: %f(%f) (%s)\n", ee - ss, mm, ibv_wc_opcode_str(conn_recv->opcode) ), 1);

    /*Check which request was successed*/
    if (conn_recv->opcode == IBV_WC_RECV) {
      uint64_t rdma_read_sum = 0;
      uint64_t rrs = 0;
      int last_rdma_read = 0;
      debug(printf("RDMA lib: COMM: Recv REQ: id=%lu, wc.slid=%u recv_wc time=%f(%f)\n",  conn_recv->id, conn_recv->slid, ee - ss, mm), 2);
      rrre = (struct rdma_read_request_entry*)malloc(sizeof(struct rdma_read_request_entry));
      memcpy(rrre, conn_recv->recv_msg, sizeof(struct rdma_read_request_entry));      
      debug(printf("RDMA lib: RECV: qp= %lu, id=%lu, order=%lu, tag=%lu, addr=%p, length=%lu, rkey=%lu, wc.slid=%u recv_wc time=%f(%f)\n", conn_recv->id->qp, rrre->id, rrre->order, rrre->tag, rrre->mr.addr, rrre->mr.length, rrre->mr.rkey, conn_recv->id, conn_recv->slid, ee - ss, mm), 2);
      conn_send = create_connection(conn_recv->id);
      free_connection(conn_recv);
      rrre->conn = conn_send;
      post_matched_request(PASSIVE, rrre);
    } else if (conn_recv->opcode == IBV_WC_RDMA_READ) {
      struct rdma_read_request_entry *passive_rrre;
      debug(printf("RDMA lib: COMM: Done IBV_WC_RDMA_READ: id=%lu(%lu) recv_wc time=%f(%f)\n", conn_recv->count, (uintptr_t)conn_recv, ee - ss, mm), 2);
      conn_send = create_connection(conn_recv->id);
      passive_rrre = conn_recv->passive_rrre;
      //TODO: find an optimal location for "create_connection()", "sem_post()".
      if (conn_recv->passive_rrre->mr.length == MIN_LENGTH) {
	free_rrre(PASSIVE, conn_recv->passive_rrre);
	free_connection(conn_recv);
      } else {
	send_ctl_msg (conn_send, MR_INIT_ACK, 0);
	free_rrre(ACTIVE, conn_recv->active_rrre);
	free_rrre(PASSIVE, conn_recv->passive_rrre);
	free_connection(conn_recv);
      }
      sem_post(passive_rrre->is_rdma_completed);
      continue;
    } else if (conn_recv->opcode == IBV_WC_SEND) {
      debug(printf("RDMA lib: COMM: Sent IBV_WC_SEND: id=%lu(%lu) recv_wc time=%f(%f)\n", conn_recv->count, (uintptr_t)conn_recv, ee - ss, mm), 2);
      /* ========
	Note: free_connection(conn_recv) is not needed, because conn_recv was used for post_sent but the conn_recv is being used.
      */
      // free_connection(conn_recv);
      /* ======== */
      continue;
    } else {
      die("unknow opecode.");
      continue;
    }
  }
  return NULL;
}

static int create_rrre(struct connection *conn, struct rdma_read_request_entry *rrre)
{
  rrre = (struct rdma_read_request_entry*)malloc(sizeof(struct rdma_read_request_entry));
  memset(rrre, 0, sizeof(struct rdma_read_request_entry));
  memcpy(rrre, conn->recv_msg, sizeof(struct rdma_read_request_entry));
  rrre->conn = conn;
}

static int free_rrre(int mode, struct rdma_read_request_entry *rrre)
{
  switch(mode){
  case ACTIVE:
    break;
  case PASSIVE:
    sem_destroy(rrre->is_rdma_completed);
    dereg_mr(rrre->passive_mr);
    break;
  }
  //  free(rrre);
}

static int post_matched_request (int target_q_id, struct rdma_read_request_entry* cur_rrre)
{
  struct rdma_read_request_entry* target_rrre;
  struct rdma_read_request_entry** active_rrre, **passive_rrre;
  lq *target_rrre_q, *cur_rrre_q;

  pthread_mutex_lock(&post_req_lock);
  switch (target_q_id){
  case ACTIVE:
    target_rrre_q = &rdma_request_aq;
    cur_rrre_q = &rdma_request_pq;
    active_rrre = &target_rrre;
    passive_rrre = &cur_rrre;
    break;
  case PASSIVE:
    target_rrre_q = &rdma_request_pq;
    cur_rrre_q = &rdma_request_aq;
    active_rrre = &cur_rrre;
    passive_rrre = &target_rrre;
    break;
  default:
    fprintf(stderr, "Wrong Q id \n");
    exit(1);
  }
  lq_init_it(target_rrre_q);
  while ((target_rrre = (struct rdma_read_request_entry*)lq_next(target_rrre_q)) != NULL) {
    //    fprintf(stderr,"id: %lu-%lu, tag: %lu-%lu\n", target_rrre->id, cur_rrre->id, target_rrre->tag, cur_rrre->tag);
    //TODO: write more sophisticated code !!
    if (target_rrre->id == RDMA_ANY_SOURCE || 
	cur_rrre->id    == RDMA_ANY_SOURCE || 
	target_rrre->id == cur_rrre->id) {
      if (target_rrre->tag == RDMA_ANY_TAG || 
	  cur_rrre->tag   == RDMA_ANY_TAG || 
	  target_rrre->tag == cur_rrre->tag) {
	(*active_rrre)->conn->active_rrre = *active_rrre;
	(*active_rrre)->conn->passive_rrre = *passive_rrre;
	if ((*passive_rrre)->mr.length == MIN_LENGTH) {
	  post_RDMA_read ((*active_rrre)->conn, (uint64_t)(*active_rrre)->mr.addr, (*active_rrre)->mr.rkey, (uint64_t)(*passive_rrre)->mr.addr, MIN_LENGTH, (*passive_rrre)->mr.lkey) ;
	  if (target_q_id == PASSIVE) {
	    lq_remove(target_rrre_q, target_rrre);
	    lq_enq(cur_rrre_q, cur_rrre);
	  }
	} else {
	  post_RDMA_read ((*active_rrre)->conn, (uint64_t)(*active_rrre)->mr.addr, (*active_rrre)->mr.rkey, (uint64_t)(*passive_rrre)->mr.addr, (*active_rrre)->mr.length, (*passive_rrre)->mr.lkey) ;
	  lq_remove(target_rrre_q, target_rrre);
	}
	lq_fin_it(target_rrre_q);
	pthread_mutex_unlock(&post_req_lock);
	return 1;
      }
    }

  }
  lq_enq(cur_rrre_q, cur_rrre);
  fprintf(stderr,"Queued: %p: id:%lu, tag:%lu\n", cur_rrre_q, cur_rrre->id, cur_rrre->tag);
  lq_fin_it(target_rrre_q);
  {
    struct rdma_read_request_entry *rrre;
    lq_init_it(cur_rrre_q);
    while ((rrre = (struct rdma_read_request_entry*)lq_next(cur_rrre_q)) != NULL) { printf("Q: %p: rrre:%d\n", cur_rrre_q, rrre->mr.length);}
    lq_fin_it(cur_rrre_q);
  }

  pthread_mutex_unlock(&post_req_lock);
  return 0;
}


int rdma_irecv_r (void *buf, int size, void* datatype, int source, int tag, struct RDMA_communicator *rdma_com, struct RDMA_request *request)
{
  struct rdma_read_request_entry *rrre;
  struct ibv_mr *passive_mr;
  rrre = (struct rdma_read_request_entry *) malloc(sizeof(struct rdma_read_request_entry));
  rrre->id = source;
  //TODO: use order for something.
  rrre->order = 0;
  rrre->tag = tag;
  
  sem_init(&(request->is_rdma_completed), 0, 0);
  rrre->is_rdma_completed = &(request->is_rdma_completed);

  passive_mr = reg_mr(buf, size);
  memcpy(&(rrre->mr), passive_mr, sizeof(struct ibv_mr));
  rrre->passive_mr =  passive_mr;
  printf("RDMA lib: RECV: local_addr=%p, length=%lu, lkey=%lu\n", rrre->mr.addr, rrre->mr.length, rrre->mr.lkey);
  if (!post_matched_request (ACTIVE, rrre)) {
    printf("irecv Skiped\n");
    return 0;
  }
  printf("irecv RDMA\n");
  return 1;
}

/*
 Note: post RDMA request to the quere pair
   INPUT
     conn       : 
     remote_addr: Remote virtual address to be read from
     rkey       : Remote key
     local_addr : Local virtual address to be read to
     length     : Length to be read from remote memory
     lkey       : Local key
   OUTPUT
     N/A
*/
static void post_RDMA_read (struct connection* conn, uint64_t remote_addr, uint32_t rkey, uint64_t local_addr, uint32_t length, uint32_t lkey) 
{
    struct RDMA_buff *rdma_buff = NULL;
    struct ibv_wc wc;
    struct ibv_send_wr wr, *bad_wr;
    struct ibv_sge sge;

    /* !! memset must be called to initialize ibv_wc !!*/
    memset(&wr, 0, sizeof(wr));

    wr.wr_id = (uintptr_t)conn;
    wr.opcode = IBV_WR_RDMA_READ;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.send_flags = IBV_SEND_SIGNALED;
    wr.wr.rdma.remote_addr = remote_addr;
    wr.wr.rdma.rkey = rkey;

    sge.addr = local_addr;
    sge.length = length;
    sge.lkey = lkey;

    debug(printf("RDMA lib: Preparing RDMA transfer: Done\n"), 1);
    if ((ibv_post_send(conn->qp, &wr, &bad_wr))) {
      fprintf(stderr, "RDMA lib: ERROR: post send failed @ %s:%d\n", __FILE__, __LINE__);
      exit(1);
    }

    debug(printf("RDMA lib: RECV: Post RDMA_READ: qp=%lu, id=%lu(%d), remote_addr=%p(%lu), rkey=%u, sge.addr=%lu, sge.length=%u,  sge.lkey=%lu\n", conn->qp, conn->count, (uintptr_t)conn, wr.wr.rdma.remote_addr, wr.wr.rdma.remote_addr, wr.wr.rdma.rkey, sge.addr, sge.length, sge.lkey), 2);
}

int RDMA_Passive_Finalize(struct RDMA_communicator *comm)
{
  rdma_destroy_id(comm->cm_id);
  rdma_destroy_event_channel(comm->ec);
  return 0;
}

