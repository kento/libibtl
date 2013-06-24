#include <time.h>
#include <sys/mman.h>

#include "common.h"
#include "rdma_server.h"
#include "list_queue.h"

#define MIN_LENGTH 1
#define ACTIVE 0
#define PASSIVE 10

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
static int matched_requests (struct rdma_read_request_entry *req1, struct rdma_read_request_entry *req2);
static int rdma_irecv_r_opt (void *buf, int size, void* datatype, int source, int tag, struct RDMA_communicator *rdma_com, struct RDMA_request *request, int dequeue);
static int rdma_irecv_r_opt_offset (void *buf, int offset, int size, void* datatype, int source, int tag, struct RDMA_communicator *rdma_com, struct RDMA_request *request, int silent);

static struct context *s_ctx = NULL;

pthread_t listen_thread;

/*Queue in which RDMA requests from active sides are put */
static lq rdma_request_aq;

/*Queue in which RDMA requests on this process are put */
static lq rdma_request_pq;

/*Lock for post_matched_request*/
static pthread_mutex_t post_req_lock = PTHREAD_MUTEX_INITIALIZER;

static void set_envs()
{
  /*leave empty for future requirment*/
  return;
}

int RDMA_Passive_Init(struct RDMA_communicator *comm) 
{
  static int thread_running = 0;

  lq_init(&rdma_request_aq);
  lq_init(&rdma_request_pq);

  set_envs();
  TEST_NZ(pthread_create(&listen_thread, NULL, (void *) rdma_passive_init, comm));
  wait_accept();

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



  while (1) {
    double mm, ss, ee, rss, ree;
    struct rdma_read_request_entry* rrre;

    ss = get_dtime();
    num_entries = recv_wc(1, &conn_recv);
    
    mm = ss - ee;
    ee = get_dtime();
    debug(fprintf(stdout, "RDMA lib: RECV: recv_wc time: %f(%f) (%s)\n", ee - ss, mm, ibv_wc_opcode_str(conn_recv->opcode) ), 1);

    /*Checking which request was successed*/
    if (conn_recv->opcode == IBV_WC_RECV) {
      uint64_t rdma_read_sum = 0;
      uint64_t rrs = 0;
      int last_rdma_read = 0;
      debug(printf("RDMA lib: COMM: Recv REQ: id=%lu, wc.slid=%u recv_wc time=%f(%f)\n",  conn_recv->id, conn_recv->slid, ee - ss, mm), 1);
      rrre = (struct rdma_read_request_entry*)malloc(sizeof(struct rdma_read_request_entry));
      memcpy(rrre, conn_recv->recv_msg, sizeof(struct rdma_read_request_entry));      
      debug(printf("RDMA lib: RECV: qp= %lu, id=%lu, order=%lu, tag=%lu, addr=%p, length=%lu, rkey=%p(%lu), wc.slid=%u recv_wc time=%f(%f)\n", conn_recv->id->qp, rrre->id, rrre->order, rrre->tag, rrre->mr.addr, rrre->mr.length, rrre->mr.rkey, rrre->mr.rkey, conn_recv->id, conn_recv->slid, ee - ss, mm), 1);
      conn_send = create_connection(conn_recv->id);
      free_connection(conn_recv);
      rrre->conn = conn_send;
      rss = get_dtime();
      post_matched_request(PASSIVE, rrre);
    } else if (conn_recv->opcode == IBV_WC_RDMA_READ) {
      ree = get_dtime();
      struct rdma_read_request_entry *passive_rrre;
      debug(printf("RDMA lib: COMM: Done IBV_WC_RDMA_READ: id=%lu(%lu) recv_wc time=%f(%f), rdma_time=%f\n", conn_recv->count, (uintptr_t)conn_recv, ee - ss, mm, ree - rss), 1);
      passive_rrre = conn_recv->passive_rrre;
      //TODO: find an optimal location for "create_connection()", "sem_post()".
      if (passive_rrre->silent == 1) {
	free_rrre(PASSIVE, conn_recv->passive_rrre);
	conn_recv->active_rrre = NULL;
	/* ==============
	  NOTE:
	    Since conn_recv is allocated in struct active_rrre->conn, which is reused for succsive request such as RDMA_{latency|irecv},
	    Do not free the conn_recv in case of silent==1
	 */
	// free_connection(conn_recv);
	/* ============== */
      } else {
	conn_send = create_connection(conn_recv->id);
	send_ctl_msg (conn_send, MR_INIT_ACK, 0);
	free_rrre(ACTIVE, conn_recv->active_rrre);
	free_rrre(PASSIVE, conn_recv->passive_rrre);
	free_connection(conn_recv);
      }
      if(sem_post(passive_rrre->is_rdma_completed)  < 0 ) {
	fprintf(stderr, "Failed sem_post() at %s:%s:%d\n", __FILE__, __func__, __LINE__);
	exit(1);
      }

      continue;
    } else if (conn_recv->opcode == IBV_WC_SEND) {
      debug(printf("RDMA lib: COMM: Sent IBV_WC_SEND: id=%lu(%lu) recv_wc time=%f(%f)\n", conn_recv->count, (uintptr_t)conn_recv, ee - ss, mm), 1);
      /* ========
	Note: free_connection(conn_recv) is not needed, 
	      because conn_recv was used for post_sent but the conn_recv is being used for post_recv for the XSnext msg.
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
    dereg_mr(rrre->passive_mr);
    break;
  }
}

static int post_matched_request (int target_q_id, struct rdma_read_request_entry* cur_rrre)
{
  struct rdma_read_request_entry* target_rrre;
  struct rdma_read_request_entry** active_rrre, **passive_rrre;
  lq *target_rrre_q, *cur_rrre_q;
  uint64_t remote_addr;

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
    if (matched_requests(target_rrre, cur_rrre)) {
      debug(fprintf(stderr, "RDMA lib: RECV: match target:%p(tag:%lu) cur:%p(tag:%lu)\n", target_rrre, target_rrre->tag, cur_rrre, cur_rrre->tag ), 1);
      (*active_rrre)->conn->active_rrre = *active_rrre;
      (*active_rrre)->conn->passive_rrre = *passive_rrre;

      remote_addr = (uint64_t)(*active_rrre)->mr.addr;
      if ((*passive_rrre)->offset > 0) {
	remote_addr += (*passive_rrre)->offset;
      }
      if ((*passive_rrre)->silent == 1) {
	post_RDMA_read ((*active_rrre)->conn, remote_addr, (*active_rrre)->mr.rkey, (uint64_t)(*passive_rrre)->mr.addr, (*passive_rrre)->length, (*passive_rrre)->mr.lkey) ;
      } else {
	post_RDMA_read ((*active_rrre)->conn, (uint64_t)(*active_rrre)->mr.addr, (*active_rrre)->mr.rkey, (uint64_t)(*passive_rrre)->mr.addr, (*active_rrre)->mr.length, (*passive_rrre)->mr.lkey) ;
      }
      // For rdma_latency_r
      if (((*passive_rrre)->silent == 1 && target_q_id == PASSIVE) ||
	  (*passive_rrre)->silent == 0) {
	lq_remove(target_rrre_q, target_rrre);	
      }
      if ((*passive_rrre)->silent == 1 && target_q_id == PASSIVE) { 
	lq_enq(cur_rrre_q, cur_rrre);

	lq_fin_it(target_rrre_q);
	pthread_mutex_unlock(&post_req_lock);
	return 1;
      }
      lq_fin_it(target_rrre_q);
      pthread_mutex_unlock(&post_req_lock);
      return 0;
    }
  }

  lq_enq(cur_rrre_q, cur_rrre);
  debug(fprintf(stderr,"Queued: %p: id:%lu, tag:%lu tq:%p, hd:%p,\n", cur_rrre_q, cur_rrre->id, cur_rrre->tag, target_rrre_q, cur_rrre_q->head),  1);
  lq_fin_it(target_rrre_q);

  pthread_mutex_unlock(&post_req_lock);      
  return 1;
}

static int matched_requests (struct rdma_read_request_entry *req1, struct rdma_read_request_entry *req2)
{
  //TODO: write more sophisticated code !!
  if (req1->id == RDMA_ANY_SOURCE || 
      req2->id == RDMA_ANY_SOURCE || 
      req1->id == req2->id) {
    if (req1->tag == RDMA_ANY_TAG || 
	req2->tag == RDMA_ANY_TAG || 
	req1->tag == req2->tag) {
      return 1;
	}
  }
  return 0;
}

long double rdma_latency_r (int source, struct RDMA_communicator *rdma_com) 
{
  long double begin, end;
  char v;
  struct RDMA_request request;
   //TODO: define datatypes and use an appropriate datatype here, for now, allocate NULL.
  rdma_irecv_r_opt(&v, 1, NULL, source, RDMA_ANY_TAG, rdma_com, &request, 1);
  begin = get_dtime();
  rdma_wait(&request);
  end = get_dtime();

  return end - begin;
}

int rdma_irecv_r (void *buf, int size, void* datatype, int source, int tag, struct RDMA_communicator *rdma_com, struct RDMA_request *request) 
{
  int result;
  result = rdma_irecv_r_opt (buf, size, datatype, source, tag, rdma_com, request, 0);
  return result;
}

int rdma_irecv_r_silent (void *buf, int size, void* datatype, int source, int tag, struct RDMA_communicator *rdma_com, struct RDMA_request *request) 
{
  return rdma_irecv_r_opt (buf, size, datatype, source, tag, rdma_com, request, 1);
}

int rdma_irecv_r_offset (void *buf, int offset, int size, void* datatype, int source, int tag, struct RDMA_communicator *rdma_com, struct RDMA_request *request)
{  return rdma_irecv_r_opt_offset (buf, offset, size, datatype, source, tag, rdma_com, request, 0);}


int rdma_irecv_r_silent_offset (void *buf, int offset, int size, void* datatype, int source, int tag, struct RDMA_communicator *rdma_com, struct RDMA_request *request) 
{
  return rdma_irecv_r_opt_offset (buf, offset, size, datatype, source, tag, rdma_com, request, 1);
}


static int rdma_irecv_r_opt_offset (void *buf, int offset, int size, void* datatype, int source, int tag, struct RDMA_communicator *rdma_com, struct RDMA_request *request, int silent)
{
  struct rdma_read_request_entry *rrre;
  struct ibv_mr *passive_mr;
  rrre = (struct rdma_read_request_entry *) malloc(sizeof(struct rdma_read_request_entry));
  rrre->id = source;
  rrre->order = 0;
  rrre->tag = tag;
  rrre->silent = silent;
  rrre->offset = offset;
  rrre->length = size;
  
  if (sem_init(&(request->is_rdma_completed), 0, 0) < 0) {
    fprintf(stderr, "Failed to sem_init\n");
    exit(1);
  }
  rrre->is_rdma_completed = &(request->is_rdma_completed);

  passive_mr = reg_mr(buf, size);
  memcpy(&(rrre->mr), passive_mr, sizeof(struct ibv_mr));
  rrre->passive_mr =  passive_mr;
  debug(fprintf(stderr,"RDMA lib: RECV: local_addr=%p, length=%lu, lkey=%lu\n", rrre->mr.addr, rrre->mr.length, rrre->mr.lkey), 1);

  if (!post_matched_request (ACTIVE, rrre)) {
    return 0;
  }
  return 1;
}

static int rdma_irecv_r_opt (void *buf, int size, void* datatype, int source, int tag, struct RDMA_communicator *rdma_com, struct RDMA_request *request, int silent)
{
  struct rdma_read_request_entry *rrre;
  struct ibv_mr *passive_mr;
  rrre = (struct rdma_read_request_entry *) malloc(sizeof(struct rdma_read_request_entry));
  rrre->id = source;
  rrre->order = 0;
  rrre->tag = tag;
  rrre->silent = silent;
  rrre->offset = 0;
  rrre->length = size;

  if (sem_init(&(request->is_rdma_completed), 0, 0) < 0) {
    fprintf(stderr, "Failed to sem_init\n");
    exit(1);
  }
  rrre->is_rdma_completed = &(request->is_rdma_completed);

  passive_mr = reg_mr(buf, size);
  memcpy(&(rrre->mr), passive_mr, sizeof(struct ibv_mr));
  rrre->passive_mr =  passive_mr;
  debug(fprintf(stderr,"RDMA lib: RECV: local_addr=%p, length=%lu, lkey=%lu\n", rrre->mr.addr, rrre->mr.length, rrre->mr.lkey), 1);
  if (!post_matched_request (ACTIVE, rrre)) {
    return 0;
  }
  return 1;
}

int rdma_iprobe(int source, int tag, struct RDMA_communicator *rdma_com)
{
  struct rdma_read_request_entry* target_rrre;
  struct rdma_read_request_entry  prove_rrre;
  lq *target_rrre_q;
  int result = 0;

  target_rrre_q = &rdma_request_aq;
  prove_rrre.id  = source;
  prove_rrre.tag = tag;

  lq_init_it(target_rrre_q);
  while ((target_rrre = (struct rdma_read_request_entry*)lq_next(target_rrre_q)) != NULL) {
    if (matched_requests(target_rrre, &prove_rrre)) {
      result = 1;
      break;
    }
  }
  lq_fin_it(target_rrre_q);
  return result;
}

//TODO: int RDMA_Reqid(struct RDMA_communicator *rdma_com, int source, int tag) 
int rdma_reqid(struct RDMA_communicator *rdma_com, int tag)
{
  struct rdma_read_request_entry* target_rrre;
  struct rdma_read_request_entry  prove_rrre;
  lq *target_rrre_q;
  int result = -1;

  target_rrre_q = &rdma_request_aq;
  lq_init_it(target_rrre_q);
  while ((target_rrre = (struct rdma_read_request_entry*)lq_next(target_rrre_q)) != NULL) {
    if (target_rrre->tag == tag) {
      result = target_rrre->id;
      break;
    }
  }  
  lq_fin_it(target_rrre_q);
  return result;
  
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
    debug(fprintf(stderr, "RDMA lib: RECV: Prpd RDMA_READ: qp=%lu, id=%lu(%d), remote_addr=%p(%lu), rkey=%p(%u), sge.addr=%p(%lu), sge.length=%u,  sge.lkey=%p(%lu)\n", conn->qp, conn->count, (uintptr_t)conn, wr.wr.rdma.remote_addr, wr.wr.rdma.remote_addr, wr.wr.rdma.rkey, wr.wr.rdma.rkey, sge.addr, sge.addr, sge.length, sge.lkey, sge.lkey), 1);   
    if ((ibv_post_send(conn->qp, &wr, &bad_wr))) {
      fprintf(stderr, "RDMA lib: ERROR: post send failed @ %s:%d\n", __FILE__, __LINE__);
      exit(1);
    }
    debug(fprintf(stderr, "RDMA lib: RECV: Post RDMA_READ: qp=%lu, id=%lu(%d), remote_addr=%p(%lu), rkey=%u, sge.addr=%lu, sge.length=%u,  sge.lkey=%lu\n", conn->qp, conn->count, (uintptr_t)conn, wr.wr.rdma.remote_addr, wr.wr.rdma.remote_addr, wr.wr.rdma.rkey, sge.addr, sge.length, sge.lkey), 1);   

}

int RDMA_Passive_Finalize(struct RDMA_communicator *comm)
{
  rdma_destroy_id(comm->cm_id);
  rdma_destroy_event_channel(comm->ec);
  return 0;
}

