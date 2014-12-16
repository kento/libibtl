#include <unistd.h>
#include <sys/mman.h>
#include <sys/utsname.h>
#include <string.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <errno.h>

#include "fdmi.h"
#include "fdmi_param.h"
#include "fdmi_err.h"
#include "fdmi_mem.h"
#include "fdmi_util.h"
#include "fdmi_queue.h"
#include "fdmi_proc_man.h"
#include "fdmi_datatype.h"
//#include "fdmi_loop.h"

#define  DEBUG_TEST
//#define DEBUG_P_INIT
//#define DEBUG_A_INIT
//#define DEBUG_RECV
//#define DEBUG_SEND
//#define DEBUG_QP
//#define DEBUG_UNLOCK
//#define DEBUG_PERF
//#define DEBUG_PERF_1
//#define DEBUG_WC_SEND

//#define FDMI_EAGER_BUFF_SIZE (513 * 1024)
//#define FDMI_EAGER_BUFF_SIZE (713 * 1024)

#define IBTL_LISTEN_PORT (841015)

#define FDMI_EAGER_BUFF_SIZE (64 * 1024)
#define FDMI_MAX_PENDING_RECV_QUERY 10

#define FDMI_HOSTNAME_LIMIT (16)
#define RDMA_LISTEN_BACKLOG (16)

#define FDMI_RECOVERY_TAG 28047

#define FMI_SUCCESS (0)
#define FMI_RECOVERY (1)
#define FMI_FAiLURE (2)

#define FMI_ANY_SOURCE (-1)
#define FMI_ANY_TAG (-1)

#define FDMI_FAILURE (1)
#define FDMI_RECOVERY (2)
#define FDMI_JOINING (3)

const int TIMEOUT_IN_MS = 5000; /* ms */

/* TYPE declaration */
struct fdmi_channel {
  /* Receive channel */
  struct fdmi_recv_channel *recv_channel;
  /* Send channel */
  struct fdmi_send_channel *send_channel;
};

struct fdmi_recv_channel {
  /* # of domain/HCA/network */
  int numdomain;
  struct fdmi_domain** domains;
  int numpeer;
  struct fdmi_peer** peers;
};

struct fdmi_sendrecv_channel {
  /* # of domain/HCA/network */
  int numdomain;
  struct fdmi_domain** domains;

  int numpeer;
  struct fdmi_peer** peers;
};

struct fdmi_poll_event_recv_channel_arg {
  int domain_id;
  struct fdmi_domain* domain;
  struct fdmi_peer** peers;
};


struct fdmi_send_channel {
  /* # of domain/HCA/network */
  int numdomain;

  struct fdmi_domain** domains;

  int numpeer;

  struct fdmi_peer** peers;
};

struct fdmi_domain {
  struct rdma_event_channel *rec;

  struct rdma_cm_id *rcid;

  struct fdmi_domain_context *dctx;
  
  /* This thread poll the even channel to welcome client connections */
  pthread_t ec_poller_thread;
};

struct fdmi_domain_context {
  /* Assisiated with HCA */
  struct ibv_context *ctx;
  /* Completion Channel  */
  /* this channel receive all events associated with Completion Queues(port)   */
  struct ibv_comp_channel *cc;
  /* Protection Domain */
  struct ibv_pd *pd;
  /* Completion Queue */
  struct ibv_cq *cq;
  /* Completion Queue For fdmi_get_wc */
  struct ibv_cq *tmp_cq;
  /* Query queue pair (Passive and Active size query) */
  struct fdmi_query_qp *query_qp;
  /* This thread poll a completion queue to handle messagings */
  pthread_t cq_poller_thread;
};


/* fdmi_peer is identical to MPI process/rank */
struct fdmi_peer {
  int numconn;
  struct fdmi_connection **conn;
  int is_connected;
  pthread_mutex_t on_connecting_lock;
};

struct fdmi_connection {
  struct rdma_cm_id		*rcid;
  struct ibv_qp			*qp;
  struct fdmi_msg		*msg;
  struct ibv_mr			*msg_mr;
  struct fdmi_domain_context	*dctx;
};

enum fdmi_query_queue_type {
  PASSIVE_Q,
  ACTIVE_Q
};

enum fdmi_opcode {
  FDMI_EAGER,
  FDMI_RDMA_READ,
  FDMI_LOOP,
};

enum fdmi_msg_type {
  FDMI_COMM_INIT,
  FDMI_COMM_FIN
};


struct fdmi_query_qp {
  struct fdmi_queue* pq;
  struct fdmi_queue* aq;
};

struct fdmi_query {
  struct fdmi_connection *conn;
  struct fdmi_msg *msg;
  struct ibv_mr *msg_mr;
  
  struct fdmi_query *active_query_to_free;

  volatile int *request_flag;
  struct fdmi_request *request;
  //  pthread_mutex_t *request_mtx;
  //  sem_t *request_sem;
};


struct fdmi_msg {
  int                   type; /*{FDMI_COMM_INIT|FDMI_COMM_FIN}*/
  int			opcode; /*{FDMI_EAGER|FDMI_RDMA_READ|FDMI_LOOOP(not in use)}*/
  int                   epoch;
  void*			addr;
  uint64_t		rank;
  uint64_t		order;
  uint64_t		tag;
  int			op;
  /*TODO: use union to reduce the size*/
  int			count;
  int			comm_id;
  struct fdmi_datatype	dtype;

  /*A registered active side memory region(mr) which is allowed to be read with RDMA from remote (Passive side use)
    mr.addr and mr.rkey to be used for the purpose. */
  struct ibv_mr rdma_mr;
  /*TODO: use union*/
  int		loop;
  
  int	eager_size;
  char	eager_buff[FDMI_EAGER_BUFF_SIZE];
};

enum fdmi_ft_msg_type {
  FDMI_FAILURE_REPORT,
  FDMI_REPORT_PROPA
};

struct fdmi_ft_msg {
  int id;
  int msg_type;
  int rlength;
  int ranks[128*2];

  int plength;
  int offset[128*2];
  int range[128*2];

  int elength;
  int eoffset[128*2];
  int erange[128*2];

  int epoch;
};


//static int set_envs (struct fdmi_communicator* comm);
static int fdmi_set_envs (void);
static int fdmi_wait_for_event(struct rdma_event_channel *channel, enum rdma_cm_event_type requested_event);

/*Just for a pasive side*/
static void accept_connection(struct rdma_cm_id *id);
static int wait_msg_sent(void);

/*PROTOTYPE declaration*/
const char *ibv_wc_opcode_str(enum ibv_wc_opcode opcode);
const char *rdma_err_status_str(enum ibv_wc_status status);
const char *event_type_str(enum rdma_cm_event_type event);



static void fdmi_init_vcomm_world(void);
static struct fdmi_channel* fdmi_create_channel (void);
static struct fdmi_port* fdmi_create_port (struct rdma_event_channel* rec, struct rdma_cm_id *rcid);
static struct fdmi_recv_channel* fdmi_create_recv_channel(void);
static int fdmi_destroy_port (struct fdmi_port* port);
static struct fdmi_ibnet*  fdmi_create_ibnet(struct rdma_event_channel* rec);
static void fdmi_destroy_ibnet(struct fdmi_ibnet* ibnet);
static struct fdmi_connection* fdmi_create_connection (struct rdma_cm_id* rcid, struct fdmi_domain_context* dctx);
static struct fdmi_peer* fdmi_create_peer (struct fdmi_domain **domain, int numdomain);
static int fdmi_destroy_peer (struct fdmi_peer* peer) ;
static void fdmi_destroy_recv_channel(struct fdmi_recv_channel* recv_channel);
static struct fdmi_send_channel* fdmi_create_send_channel(void);
static void fdmi_destroy_send_channel(struct fdmi_send_channel* send_channel);
static struct fdmi_query_qp* fdmi_create_query_qp(void);
static void fdmi_destroy_query_qp(struct fdmi_query_qp* query_qp);
static struct fdmi_query* fdmi_create_query(struct fdmi_connection* conn);
static void fdmi_destroy_query(struct fdmi_query* query);


static struct fdmi_query* fdmi_init_passive_query(struct fdmi_post_recv_param *prparam);
static struct fdmi_query* fdmi_get_matched_query2(int rank, int tag, struct fdmi_communicator* comm, struct fdmi_queue* queue);
static int fdmi_is_matched_query2(int rank, int tag, struct fdmi_communicator* comm, struct fdmi_query* query);
static int fdmi_is_matched_query(struct fdmi_query* query1, struct fdmi_query* query2);
static struct fdmi_query* fdmi_get_matched_query(struct fdmi_query* query, struct fdmi_queue* queue);
static void* fdmi_post_rdma_read(struct fdmi_query* passive_query, struct fdmi_query* active_query);
//static void fdmi_check_recovery();

static int fdmi_cpy_eager_buff2(char *recv_buff, struct fdmi_datatype *dtype, volatile int* request_flag, struct fdmi_query *active_query);
static int fdmi_cpy_eager_buff(struct fdmi_query *passive_query, struct fdmi_query *active_query);
static int get_port_count (void);
static void fdmi_build_params(struct rdma_conn_param *params);
static void fdmi_bind_addr_and_listen(struct fdmi_sendrecv_channel* sendrecv_channel);
//static void fdmi_bind_addr_and_listen(struct fdmi_recv_channel* recv_channel);
static void fdmi_create_qp (struct fdmi_domain_context** dctx, struct rdma_cm_id* rcid);
//static void fdmi_run_event_channel_poller(struct fdmi_recv_channel *recv_channel);
static void fdmi_run_event_channel_poller(struct fdmi_sendrecv_channel *sendrecv_channel);
static int fdmi_get_wc(int nument, struct ibv_cq** tmp_cq,  struct ibv_comp_channel* cc, struct ibv_wc** wc_out);
static void* fdmi_poll_recv_cq(void* struct_fdmi_domain_context__dctx);
static void* fdmi_poll_send_cq(void* struct_fdmi_domain_context__dctx);
static void* fdmi_poll_event_recv_channel(void* arg);
static void fdmi_make_alltoall_connection(struct fdmi_sendrecv_channel* sendrecv_channel);
//static void fdmi_make_ring_connection(struct fdmi_send_channel* send_channel);
static void fdmi_make_ring_connection(struct fdmi_sendrecv_channel* sendrecv_channel);
//struct fdmi_connection* fdmi_connect(struct fdmi_send_channel* send_channel, int rank);
struct fdmi_connection* fdmi_connect(struct fdmi_sendrecv_channel* sendrecv_channel, int rank);
static void fdmi_dereg_mr(struct ibv_mr *mr);
struct ibv_mr* fdmi_reg_mr (const void* addr, uint32_t size, struct ibv_pd* pd);
static void* fdmi_post_send_msg (struct fdmi_query* conn);
//static struct fdmi_query* fdmi_post_recv_msg(struct fdmi_connection* conn);
static struct fdmi_query* fdmi_post_recv_msg(struct fdmi_query* query);
static void fdmi_post_send_msg_param(struct fdmi_post_send_param* psparam);
static void fdmi_post_recv_msg_param(struct fdmi_post_recv_param* prparam);
static void fdmi_flush_query_qp(struct fdmi_query_qp *qqp);
static int fdmi_is_connected(int prank);

/*GLOBAL VARIABLES*/
//struct fdmi_channel*		channel;
struct fdmi_sendrecv_channel*	sendrecv_channel;

int	prank;
int	fdmi_size;
int	fdmi_numproc;
int	fdmi_numspare;
int	fdmi_state	   = FDMI_COMPUTE;
int	is_recv_loop	   = 0;
int	is_recv_cq_polling = 0;
int	is_flushed	   = 0;
int	comm_id		   = 0;
int     connected_rank     = 1; /*We allocate Id number begin with 0*/

struct fdmi_queue *cached_query_q;
struct fdmi_queue *free_query_q;
struct fdmi_queue *send_req_q; /*On a RDMA READ, to be able to do sem_post on a on_send_wc_recv*/
struct fdmi_queue *failed_prank_q;
struct fdmi_queue *disconnected_prank_q;

/*On a failure, all mutexes for fdmi_isend, fdmi_irecv must be unlock so that
  successive fdmi_wait do not block a thead and abort all of communications on a
  RECOVERY state.*/
struct fdmi_queue *locked_request_sem_q;

struct fdmi_ft_msg ft_msg;

#if defined(DEBUG_PERF) || defined(DEBUG_PERF_1) || defined(DEBUG_WC_SEND)
double gs, ge;
double fs, fe;
double ps, pe;
double is, ie;
double ws, we;
#endif


int ct = 0;

/*TODO: migrate to fdmi_mem.c*/
struct fdmi_queue *reg_mr_q;
struct fdmi_queue *dereg_mr_q;
static uint64_t reg_mr_cached_size = 0;
#define FDMI_REG_MR_CACHE_SIZE (2 * 1024 * 1024 * 1024L)
/*=====*/

static int get_port_count (void)
{
  int numdomain = 0;
  struct ibv_device **device;
  /* TODO: multiple HCA/port support */
  if ((device = ibv_get_device_list(&numdomain)) == NULL) {
    fdmi_err ("ibv_get_device_list failed (%s:%s:%d)", __FILE__, __func__, __LINE__);
  }
  return numdomain;
}


static struct fdmi_query_qp* fdmi_create_query_qp(void)
{
  struct fdmi_query_qp *query_qp;
  query_qp = (struct fdmi_query_qp*)fdmi_malloc(sizeof(struct fdmi_query_qp));

  query_qp->pq = fdmi_queue_create();
  query_qp->aq = fdmi_queue_create();
  return query_qp;
}

static void fdmi_destroy_query_qp(struct fdmi_query_qp* query_qp)
{
  fdmi_queue_destroy(query_qp->pq);
  fdmi_queue_destroy(query_qp->aq);
  fdmi_free(query_qp);
  return;
}

int counter = 0;

static struct fdmi_query* fdmi_create_query(struct fdmi_connection *conn)
{
  struct fdmi_query *query, *tmp_query;
  int errno;
  //  fdmi_dbg("========= start");
  //  if (conn == NULL) {
  //    fdmi_err ("fdmi_connection is NULL  (%s:%s:%d)", __FILE__, __func__, __LINE__);
  //  }
  counter++;
  query = NULL;

  fdmi_queue_lock(free_query_q);
  for (tmp_query = (struct fdmi_query*)fdmi_queue_iterate(free_query_q, FDMI_QUEUE_HEAD); tmp_query != NULL;
       tmp_query = (struct fdmi_query*)fdmi_queue_iterate(free_query_q, FDMI_QUEUE_NEXT)) {
    if (tmp_query->conn == NULL && conn == NULL) {
      query = tmp_query;
      fdmi_queue_remove(free_query_q, tmp_query);
      break;
    }
    if (conn == NULL || tmp_query->conn == NULL) continue;
    if (tmp_query->conn->dctx->pd == conn->dctx->pd) {
      query = tmp_query;
      fdmi_queue_remove(free_query_q, tmp_query);
      break;
    }
  }
  fdmi_queue_unlock(free_query_q);

  if (query  == NULL) {
    //    fdmi_dbgi(0, "========= mid: %p %p %p", query, conn, conn->dctx->pd);
    //    fdmi_dbg("========= end0: %d", sizeof(struct fdmi_msg));
    //    struct fdmi_msg *m = (struct fdmi_msg*)fdmi_malloc(sizeof(struct fdmi_msg));
    //    fdmi_dbg("========= end0.2");
    query = (struct fdmi_query*)fdmi_malloc(sizeof(struct fdmi_query));
    //    fdmi_dbg("========= end0.5: %p", query->msg);
    //    fdmi_dbg("========= end0.8: %p", query->msg);
    query->msg  = (struct fdmi_msg*)fdmi_malloc(sizeof(struct fdmi_msg));
    //    query->msg_mr = NULL;
    /* conn == NULL: passive_query, conn != active_query */
    //    fdmi_dbg("========= end1");
    if (conn != NULL) query->msg_mr = fdmi_reg_mr(query->msg, sizeof(struct fdmi_msg), conn->dctx->pd);
  } else {
    //    memset(query->msg, 0, fdmi_get_ctl_msg_size(query->msg));
  }
  //  fdmi_dbg("========= end");
  query->conn = conn;
  query->request_flag = NULL;
  query->active_query_to_free = NULL;

#ifdef DEBUG_TEST
  fdmi_test(query, __FILE__, __func__, __LINE__);
#endif

  return query;
}

static void fdmi_destroy_query(struct fdmi_query* query)
{
  fdmi_queue_lock(free_query_q);
  fdmi_queue_enq(free_query_q, query);
  counter--;
  //  fdmi_dbg("counter: %d", counter);
  fdmi_queue_unlock(free_query_q);

  /* msg_mr == NULL: passive_query, msg_mr != active_query */
  /* if (query->msg_mr != NULL) { */
  /*   fdmi_dereg_mr(query->msg_mr); */
  /* } */
  /* fdmi_free(query->msg); */
  /* fdmi_free(query); */
  return;
}

static struct fdmi_connection* fdmi_create_connection (struct rdma_cm_id* rcid, struct fdmi_domain_context* dctx)
{
  struct fdmi_connection* conn;
  
  conn = (struct fdmi_connection*)fdmi_malloc(sizeof(struct fdmi_connection));
  conn->rcid = rcid;
  conn->qp   = rcid->qp;
  /***
   * ==================
   * NOTE: We initialize conn->msg, when call fdmi_post_send_msg(),
   * because multiple different conn->msg is sent by overlapping
   //  conn->msg  = (struct fdmi_msg*)fdmi_malloc(sizeof(struct fdmi_msg));
   //  conn->msg_mr = fdmi_reg_mr (conn->msg, sizeof(struct fdmi_msg), dctx->pd);
   * ==================
   */
  conn->dctx = dctx;
  return conn;
}

static struct fdmi_peer* fdmi_create_peer (struct fdmi_domain **domain, int numdomain)
{
  struct fdmi_peer		 *peer;
  struct fdmi_domain		**d;
  struct fdmi_connection	**conn;
  int i;

  peer       = (struct fdmi_peer*)fdmi_malloc(sizeof(struct fdmi_peer));
  peer->conn = (struct fdmi_connection**)fdmi_malloc(sizeof(struct fdmi_connection*) * numdomain);
  for (i = 0; i < numdomain; i++) {
    peer->conn[i] = NULL;
  }
  peer->is_connected  = 0;
  pthread_mutex_init(&(peer->on_connecting_lock), NULL);
  /* for (i = 0, d = domain, conn = peer->conn; */
  /*      i < numdomain; */
  /*      i++, d++, conn++) { */
  /*   *conn = fdmi_create_connection (*d); */
  /* } */
  return peer;
}

static void fdmi_wait_ready()
{
  struct fdmi_domain		 *domain;
  domain = *(sendrecv_channel->domains);
  while (domain->dctx == NULL) {
    /*Wait until someone connect to me.                                                                                                                                    If someone do, domain->dctx is allocated by another thread*/
    usleep(100);
  }
  while (domain->dctx->query_qp == NULL) {
    usleep(100);
  }
  return;
}

static int fdmi_destroy_peer (struct fdmi_peer* peer)
{
  /* TODO */
}

struct fdmi_domain*  fdmi_create_domain(struct ibv_context *ctx) {
  struct fdmi_domain *domain;
  int errno;
  
  domain = (struct fdmi_domain*)fdmi_malloc(sizeof(struct fdmi_domain));

  if (!(domain->rec = rdma_create_event_channel())) {
    fdmi_err ("rdma_create_event_channel (%s:%s:%d)", __FILE__, __func__, __LINE__);
  }

  if (rdma_create_id(domain->rec, &(domain->rcid), NULL, RDMA_PS_TCP)) {
    fdmi_err ("rdma_create_id failed (%s:%s:%d)", __FILE__, __func__, __LINE__);
  }

  domain->dctx = NULL;
  

  return domain;
}



static void fdmi_destroy_recv_channel(struct fdmi_recv_channel* recv_channel)
{
  /* TODO */
}



static void fdmi_destroy_send_channel(struct fdmi_send_channel* send_channel)
{
  /* TODO */
}

static struct fdmi_sendrecv_channel* fdmi_create_sendrecv_channel()
{
  struct fdmi_sendrecv_channel *sendrecv_channel;
  struct fdmi_domain **domain;
  struct ibv_device **device, **device_to_free;
  struct fdmi_peer **peer;
  int i;
  char* value;

  sendrecv_channel = (struct fdmi_sendrecv_channel*)fdmi_malloc(sizeof(struct fdmi_sendrecv_channel));
  sendrecv_channel->numdomain = get_port_count();
  sendrecv_channel->domains = (struct fdmi_domain**)fdmi_malloc(sizeof(struct fdmi_domain*) * sendrecv_channel->numdomain);

  if ((device = device_to_free = ibv_get_device_list(NULL)) == NULL) {
    fdmi_err ("ibv_get_device_list failed (%s:%s:%d)", __FILE__, __func__, __LINE__);
  }
  
  for (i = 0, domain = sendrecv_channel->domains;
       i < sendrecv_channel->numdomain;
       i++, domain++, device++) {

    struct ibv_context *ctx;

    if ((ctx = ibv_open_device(*device)) == NULL) {
      fdmi_err ("ibv_open_device failed (%s:%s:%d)", __FILE__, __func__, __LINE__);
    }

    *domain = fdmi_create_domain(ctx);
    if(ibv_close_device(ctx) == -1) {
      fdmi_err ("ibv_close_device failed (%s:%s:%d)", __FILE__, __func__, __LINE__);
    }
  }
  ibv_free_device_list(device_to_free);

  sendrecv_channel->numpeer = fdmi_size;
  sendrecv_channel->peers = (struct fdmi_peer**)fdmi_malloc(sizeof(struct fdmi_peer*) * sendrecv_channel->numpeer);


  for (i = 0, peer = sendrecv_channel->peers; i < sendrecv_channel->numpeer; i++, peer++) {
    *peer = fdmi_create_peer (sendrecv_channel->domains, sendrecv_channel->numdomain);
  }

  return sendrecv_channel;
}




static struct fdmi_channel* fdmi_create_channel (void)
{
  struct fdmi_channel *channel;
  int i = 0;

  /* Memory allocation for the channel */
  channel = (struct fdmi_channel*) fdmi_malloc(sizeof(struct fdmi_channel));

  channel->recv_channel = fdmi_create_recv_channel();
  channel->send_channel = fdmi_create_send_channel();

  return channel;
}


static void fdmi_create_qp(struct fdmi_domain_context** dctx, struct rdma_cm_id* rcid)
{
  struct ibv_qp_init_attr qp_attr;

  //  static int count = 1;
  //  if (prank == 0) fdmi_dbg("count: %d", count++);

  if (*dctx == NULL) {

    struct fdmi_domain_context* dc;
    int errno;

    *dctx = dc = (struct fdmi_domain_context*)fdmi_malloc(sizeof(struct fdmi_domain_context));
    dc->ctx = rcid->verbs;
      
    if((dc->pd = ibv_alloc_pd(dc->ctx)) == NULL) {
      fdmi_err ("ibv_alloc_pd failed (%s:%s:%d)", __FILE__, __func__, __LINE__);
    }
    //    fdmi_dbg("pd: %p", dc->pd);
    /* struct ibv_mr *mr = ibv_reg_mr( */
    /* 		    dc->pd, */
    /* 		    0x4068f3, */
    /* 		    4, */
    /* 		    IBV_ACCESS_LOCAL_WRITE */
    /* 		    | IBV_ACCESS_REMOTE_READ); */
    /* fdmi_dbg("mr: %p", mr); */
    /* fdmi_dbg("ibv_reg_mr failed after several trial: pd:%p, addr:%p, size:%lu error:%s (%s:%s:%d)", dc->pd, rcid, sizeof(struct rdma_cm_id), strerror(errno),  __FILE__, __func__, __LINE__); */

    if((dc->cc = ibv_create_comp_channel(dc->ctx)) == NULL) {
      fdmi_err ("ibv_create_comp_channel failed (%s:%s:%d)", __FILE__, __func__, __LINE__);
    }

    if((dc->cq = ibv_create_cq(dc->ctx, 1000, NULL, dc->cc, 0)) == NULL) {
      fdmi_err ("ibv_create_cq_pd failed (%s:%s:%d)", __FILE__, __func__, __LINE__);
    }

    dc->tmp_cq = NULL;
     
    if((errno = ibv_req_notify_cq(dc->cq, 0)) > 0) {
      fdmi_err ("ibv_req_notify_cq failed (%s:%s:%d)", __FILE__, __func__, __LINE__);
    }
    (*dctx)->query_qp = fdmi_create_query_qp();
  } else {
    if ((*dctx)->ctx != rcid->verbs) {
      fdmi_err ("Invalide verbs(ibv_context): rcid->verbs(%p) must be (*dctx)->ctx(%p) (%s:%s:%d)",
		rcid->verbs, (*dctx)->ctx, __FILE__, __func__, __LINE__);
    }
  }

  memset(&qp_attr, 0, sizeof(struct ibv_qp_init_attr));
  qp_attr.send_cq = (*dctx)->cq;
  qp_attr.recv_cq = (*dctx)->cq;
  qp_attr.qp_type = IBV_QPT_RC;
  qp_attr.sq_sig_all = 0;
  qp_attr.cap.max_send_wr = 100;/*10*/
  qp_attr.cap.max_recv_wr = 100;/* 10 */
  qp_attr.cap.max_send_sge = 5;/* 1 */
  qp_attr.cap.max_recv_sge = 5;/* 1 */

  if ((rdma_create_qp(rcid, (*dctx)->pd, &qp_attr)) > 0) {
    fdmi_err ("rdma_create_qp failed (%s:%s:%d)", __FILE__, __func__, __LINE__);
  }

  if (rcid->qp == NULL) {
    fdmi_err("NULL POINTER EXCEPTION (%s:%s:%d)", __FILE__, __func__, __LINE__);
  }

  return;
}

static void fdmi_bind_addr_and_listen(struct fdmi_sendrecv_channel* sendrecv_channel)
{
  struct sockaddr_in*	addr;
  struct fdmi_domain**  domain;
  int			domain_count;

  for (domain_count = 0, domain = sendrecv_channel->domains;
       domain_count < sendrecv_channel->numdomain;
       domain_count++, domain++) {
    char iname[8];
    int  port;

    addr = (struct sockaddr_in*)fdmi_malloc(sizeof(struct sockaddr_in));
    memset(addr, 0, sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    /* TODO: Better port determination ? */
    port = IBTL_LISTEN_PORT;
    addr->sin_port   = htons(port);
    sprintf(iname, "ib0", domain_count);
    inet_aton(get_ip_addr(iname), (struct in_addr*)&(addr->sin_addr.s_addr));

    if (rdma_bind_addr((*domain)->rcid, (struct sockaddr *)addr)) {
      /*If failed, we use another port, which intend I'm client side, 
	and other client processes were already launched, and using the port */
      port = 0;
      addr->sin_port   = htons(port);
      sprintf(iname, "ib0", domain_count);
      inet_aton(get_ip_addr(iname), (struct in_addr*)&(addr->sin_addr.s_addr));
      if (rdma_bind_addr((*domain)->rcid, (struct sockaddr *)addr)) {
	/*If failed even with different port, return the error */
	fdmi_err("RDMA address binding failed: Invalid binding address or port (%s:%s:%d)", __FILE__, __func__, __LINE__);
      }
    }
#if defined(DEBUG_P_INIT)
    fdmi_dbg("P:Bind and Listen: iname=%s, port=%d", iname, port);
#endif

    /* TODO: Determine appropriate backlog value
          backlog=10 is arbitrary
    */

    if (rdma_listen((*domain)->rcid, RDMA_LISTEN_BACKLOG)) {
      fdmi_err("RDMA address listen failed (%s:%s:%d)", __FILE__, __func__, __LINE__);
    };

    /** ======
     * On RDMA_CM_EVENT_CONNECT_REQUEST event, qp need to be created from received rdma_cm_id(rcid) .
     * But it is not needed to create qp from this rcid.
     * fdmi_create_qp (*port);
     * ======
     */

    fdmi_free(addr);
  }
}


static int fdmi_get_wc(int nument, struct ibv_cq** tmp_cq,  struct ibv_comp_channel* cc, struct ibv_wc** wc_out)
{
  void* tmp_ctx;
  struct ibv_wc wc;
  struct fdmi_connection** c;
  int num_entries;

  while(1) {
    if (*tmp_cq != NULL) {
      while ((num_entries = ibv_poll_cq(*tmp_cq, nument, &wc))) {
	if (wc.status != IBV_WC_SUCCESS){
	  // fdmi_err ("wc status: %s (%s:%s:%d)", rdma_err_status_str(wc.status),   __FILE__, __func__, __LINE__);
	  return -1;
	}
	*wc_out = &wc;
	return num_entries;
      }
      if (num_entries == -1) {
	fdmi_err("ibv_poll_cq failed (%s:%s:%d)", __FILE__, __func__, __LINE__);
      }
    }
    
    if (ibv_get_cq_event(cc, tmp_cq, &tmp_ctx)) {
      fdmi_err ("ibv_get_cq_event failed (%s:%s:%d)", __FILE__, __func__, __LINE__);
    }

    ibv_ack_cq_events(*tmp_cq, 1);

    if (ibv_req_notify_cq(*tmp_cq, 0) > 0) {
      fdmi_err ("ibv_req_notify_cq (%s:%s:%d)", __FILE__, __func__, __LINE__);
    }
  }
  return 0;
}

static struct fdmi_query* fdmi_get_matched_query(struct fdmi_query* query, struct fdmi_queue* queue)
{
  struct fdmi_query *tmp_query = NULL;
  for (tmp_query = (struct fdmi_query*)fdmi_queue_iterate(queue, FDMI_QUEUE_HEAD); tmp_query != NULL;
       tmp_query = (struct fdmi_query*)fdmi_queue_iterate(queue, FDMI_QUEUE_NEXT)) {

    if (fdmi_is_matched_query(query, tmp_query)) {
      return tmp_query;
    }
  }
  return NULL;
}

static int fdmi_is_matched_query(struct fdmi_query* query1, struct fdmi_query* query2)
{
  struct fdmi_msg *msg1, *msg2;
  msg1 = query1->msg;
  msg2 = query2->msg;

  /*Assuming  !(msg1->comm_id == msg2->comm_id), 
   We use msg2->epoch < ft_msg.epoch insted of  
   msg2->epoch == ft_msg.epoch  */
  if (msg2->epoch < ft_msg.epoch) {
    //    fdmi_dbg("NOOOOOOOOOOOOOO !!!!!: %d < %d", msg2->epoch, ft_msg.epoch);
    return 0;
  }

#ifdef DEBUG_QP
  fdmi_dbg("P: TRYING: msg1: %d %d <=> msg2: %d %d", msg1->rank, msg1->tag, msg2->rank, msg2->tag);
#endif
  /*Communicator matching*/
  if (!(msg1->comm_id == msg2->comm_id)) {
#ifdef DEBUG_QP


    fdmi_dbg("P: MISCHG: msg1: %d %d %d <=> msg2: %d %d %d", msg1->comm_id, msg1->rank, msg1->tag, msg2->comm_id, msg2->rank, msg2->tag);
#endif
    return 0;
  }
  
  /* Rank matching */
  if (!(msg1->rank == FMI_ANY_SOURCE ||
	msg2->rank == FMI_ANY_SOURCE ||
	msg1->rank == msg2->rank)) {
#ifdef DEBUG_QP
  fdmi_dbg("P: MISCHG: msg1: %d %d <=> msg2: %d %d", msg1->rank, msg1->tag, msg2->rank, msg2->tag);
#endif
    return 0;
  }

  /* Tag matching */
  if (!(msg1->tag == FMI_ANY_TAG ||
	msg2->tag == FMI_ANY_TAG ||
	msg1->tag == msg2->tag)) {
#ifdef DEBUG_QP
  fdmi_dbg("P: MISCHG: msg1: %d %d <=> msg2: %d %d", msg1->rank, msg1->tag, msg2->rank, msg2->tag);
#endif
    return 0;
  }

#ifdef DEBUG_QP
  fdmi_dbg("P: MATCHG: msg1: %d %d <=> msg2: %d %d", msg1->rank, msg1->tag, msg2->rank, msg2->tag);
#endif

  return 1;
}

static struct fdmi_query* fdmi_get_matched_query2(int rank, int tag, struct fdmi_communicator* comm, struct fdmi_queue* queue)
{
  struct fdmi_query *tmp_query = NULL;
  struct fdmi_msg *msg;

  for (tmp_query = (struct fdmi_query*)fdmi_queue_iterate(queue, FDMI_QUEUE_HEAD); tmp_query != NULL;
       tmp_query = (struct fdmi_query*)fdmi_queue_iterate(queue, FDMI_QUEUE_NEXT)) {

    if (fdmi_is_matched_query2(rank, tag, comm, tmp_query)) {
      //      fdmi_dbg("Matched: msg1: %d %d <=> msg: %d %d (query:%p)", rank, tag, tmp_query->msg->rank, tmp_query->msg->tag);
      return tmp_query;
    }

  }
  return NULL;
}


static int fdmi_is_matched_query2(int rank, int tag, struct fdmi_communicator* comm, struct fdmi_query* query)
{
  struct fdmi_msg *msg;
  msg = query->msg;

#ifdef DEBUG_QP
  fdmi_dbg("P: TRYING: msg1: %d %d <=> msg: %d %d", rank, tag, msg->rank, msg->tag);
#endif
  if (msg->epoch < ft_msg.epoch) {
    return 0;
  }

  /*Communicator matching*/
  if (!(comm->id == msg->comm_id)) {
#ifdef DEBUG_QP
    fdmi_dbg("P: MISCHG: msg1: %d %d %d <=> msg: %d %d %d", comm_id, rank, tag, msg->comm_id, msg->rank, msg->tag);
#endif
    return 0;
  }
  
  /* Rank matching */
  if (!(rank == FMI_ANY_SOURCE ||
	msg->rank == FMI_ANY_SOURCE ||
	rank == msg->rank)) {
    //    fdmi_dbg("MISS MISCHG: msg1: %d %d <=> msg: %d %d (query:%p)", rank, tag, msg->rank, msg->tag, query);
#ifdef DEBUG_QP
  fdmi_dbg("P: MISCHG: msg1: %d %d <=> msg: %d %d", rank, tag, msg->rank, msg->tag);
#endif
    return 0;
  }

  /* Tag matching */
  if (!(tag == FMI_ANY_TAG ||
	msg->tag == FMI_ANY_TAG ||
	tag == msg->tag)) {
#ifdef DEBUG_QP
  fdmi_dbg("P: MISCHG: msg1: %d %d <=> msg: %d %d", rank, tag, msg->rank, msg->tag);
#endif
    return 0;
  }

#ifdef DEBUG_QP
  fdmi_dbg("P: MATCHG: msg1: %d %d <=> msg: %d %d", rank, tag, msg->rank, msg->tag);
#endif

  return 1;
}


static void fdmi_on_recv_cq_wc_recv(struct fdmi_domain_context* dctx, struct fdmi_query* active_query)
{
  struct fdmi_msg *msg;
  struct fdmi_query *matched_passive_query = NULL, *post_query;


  if (fdmi_state == FDMI_FAILURE) {
    if (!(active_query->msg->op | FDMI_FORCE)) {
	fdmi_destroy_query(active_query);
	return;
    }
  }

  msg = active_query->msg;
  /* Search qp(passive queue) and find out If RECV is alrady called */
  fdmi_queue_lock(dctx->query_qp->aq);
  fdmi_queue_lock(dctx->query_qp->pq);

  if ((matched_passive_query = fdmi_get_matched_query(active_query, dctx->query_qp->pq)) !=NULL) {
    fdmi_queue_remove(dctx->query_qp->pq, matched_passive_query);
  } else {
    if (active_query->msg->epoch >= ft_msg.epoch) {
      fdmi_queue_enq(dctx->query_qp->aq, active_query);
    }
    //    fdmi_dbgi(33, "enq. ==>> query:%p d",  active_query);
#ifdef DEBUG_QP
    fdmi_dbg("P: ENQ(A): rank:%lu, tag:%lu, raddr:%p, len:%lu, rkey:%lu",
	     msg->rank, msg->tag, msg->rdma_mr.addr, msg->rdma_mr.length, msg->rdma_mr.rkey);
#endif
  }
  fdmi_queue_unlock(dctx->query_qp->aq);
  fdmi_queue_unlock(dctx->query_qp->pq);

  /* If the passive queue has a mached query, ... */
  if (matched_passive_query != NULL) {
#ifdef DEBUG_RECV
    struct fdmi_msg *mmsg;
    mmsg = matched_passive_query->msg;
    fdmi_dbg("P: Recv msg: rank:%lu, tag:%lu, raddr:%p, len:%lu, rkey:%lu",
	     mmsg->rank, mmsg->tag, mmsg->rdma_mr.addr, mmsg->rdma_mr.length, mmsg->rdma_mr.rkey);
#endif
    if (msg->opcode == FDMI_EAGER) {
      fdmi_cpy_eager_buff(matched_passive_query, active_query);
    } else if  (msg->opcode == FDMI_RDMA_READ) {
      matched_passive_query->active_query_to_free = active_query;
#ifdef DEBUG_TEST
  fdmi_test(active_query, __FILE__, __func__, __LINE__);
#endif
      fdmi_post_rdma_read(matched_passive_query, active_query);
    }
  }

  fdmi_post_recv_msg(fdmi_create_query(active_query->conn));
  //WANKO
  //  return 0;
  //WANKO
  return;
}


static void fdmi_on_recv_cq_wc_rdma_read(struct fdmi_domain_context* dctx, struct fdmi_query* passive_query)
{
  int errno;

#ifdef DEBUG_TEST
  fdmi_test(passive_query, __FILE__, __func__, __LINE__);
  fdmi_test(passive_query->active_query_to_free, __FILE__, __func__, __LINE__);
#endif
  passive_query->active_query_to_free->msg->type = FDMI_COMM_FIN;
  *(passive_query->request_flag) = 1;
  passive_query->request->prank = passive_query->active_query_to_free->msg->rank;
  passive_query->request->tag   = passive_query->active_query_to_free->msg->tag;

  fdmi_post_send_msg(passive_query->active_query_to_free);
  fdmi_queue_lock_remove(locked_request_sem_q, &(passive_query->request_flag));


#ifdef DEBUG_UNLOCK
  fdmi_dbg("P: UNLOCK");
#endif
  fdmi_destroy_query(passive_query);
  return;
}

static void fdmi_on_recv_cq_wc_send(struct fdmi_domain_context* dctx, struct fdmi_query* active_query_to_free)
{
  fdmi_destroy_query(active_query_to_free);
  return;
}



static void fdmi_on_send_cq_wc_send(struct fdmi_domain_context *dctx, struct fdmi_query *query)
{

  if (query->msg->opcode == FDMI_EAGER) {
    *(query->request_flag) = 1;
    //    fdmi_dbgi(0, "sent: addr: %p , val:%d", query->request_flag, *(query->request_flag));
    fdmi_queue_lock_remove(locked_request_sem_q, &(query->request_flag));
    fdmi_queue_lock_remove(send_req_q, query->request_flag);
  } else {
    //fdmi_queue_lock_enq(send_req_q, query->request_flag);
    //    fdmi_dbg("enq: %p %d", query->request_flag, fdmi_queue_length(send_req_q));
  }
  fdmi_destroy_query(query);
  //  fdmi_dbgi(1, "aa SEND");
  //  fdmi_dbgi(0, "aa SEND -> 1");
  return;
}

static void fdmi_on_send_cq_wc_recv(struct fdmi_domain_context *dctx, struct fdmi_query *query)
{
  int errno;
  //  sem_t *sreq;
  //  pthread_mutex_t *sreq;
  int *sreq;

  if (fdmi_state == FDMI_FAILURE) {
    if (!(query->msg->op | FDMI_FORCE)) {
      if (query->active_query_to_free != NULL) {
	fdmi_destroy_query(query->active_query_to_free);
      }
      fdmi_destroy_query(query);
      fdmi_post_recv_msg(fdmi_create_query(query->conn));
      return;
    }
  }

  sreq = (int*)fdmi_queue_lock_deq(send_req_q);
  
  if (sreq != NULL) *sreq = 1; /*so sender can know that the message was sent*/
  //  fdmi_queue_lock_remove(locked_request_sem_q, &(query->active_query_to_free->request_flag));

#ifdef DEBUG_UNLOCK
  fdmi_dbg("A: UNLOCK");
#endif
  /*First several (pendding)query posted at initialization  does not have active_query_to_free */
  if (query->active_query_to_free != NULL) {
    fdmi_destroy_query(query->active_query_to_free);
  }
  fdmi_destroy_query(query);

  fdmi_post_recv_msg(fdmi_create_query(query->conn));

  return;
}





static void* fdmi_poll_sendrecv_cq(void* struct_fdmi_domain_context__dctx)
{
  struct fdmi_domain_context *dctx;
  struct fdmi_query *query;
  struct fdmi_connection *conn_to_post_recv;
  struct fdmi_communicator *commw;
  struct ibv_wc *wc;
  int fdmi_opcode;
  int errno;
#ifdef DEBUG_PERF
  double s,e;
#endif
  
  /*TODO: too bad to use FMI_COMM_WORLD here*/
  //  commw = FMI_COMM_WORLD;

  /*TODO: Lock mutex the standby processes do not proceed beyond fdmi_comm_rank*/
  /* if (fdmi_rmap_is_standby(prank)) { */
  /*   if ((errno = sem_wait(&(commw->standby_sem))) > 0) { */
  /*     fdmi_err ("sem_wait failed  (%s:%s:%d)", __FILE__, __func__, __LINE__); */
  /*   } */
  /* } */

  dctx = (struct fdmi_domain_context *) struct_fdmi_domain_context__dctx;
  while(1) {
    /*TODOB: examine this way works well to abort failure*/
    while(fdmi_get_wc(1, &(dctx->tmp_cq), dctx->cc, &wc) < 0);
    query = (struct fdmi_query*)(uintptr_t)wc->wr_id;
    query->msg->rank  = fdmi_rmap_itop_get(query->conn->rcid);

    conn_to_post_recv = query->conn;
    if (wc->opcode == IBV_WC_RECV) {
#if  defined(DEBUG_RECV)
    fdmi_dbg("%s: msg_type->%d, rank: %d, opcode:%d(EG:%d|RR:%d|LP:%d) ",  ibv_wc_opcode_str(wc->opcode), query->msg->type, 
	     fdmi_rmap_itop_get(conn_to_post_recv->rcid), query->msg->opcode, FDMI_EAGER, FDMI_RDMA_READ, FDMI_LOOP);
#endif
      if (query->msg->type == FDMI_COMM_INIT) {
	fdmi_on_recv_cq_wc_recv(dctx, query);
      } else if (query->msg->type == FDMI_COMM_FIN) {
	fdmi_on_send_cq_wc_recv(dctx, query);
	ct--;
      }
#ifdef DEBUG_PERF
      s = fdmi_get_time();
#endif
    } else if (wc->opcode == IBV_WC_RDMA_READ) {
#ifdef DEBUG_PERF
      e = fdmi_get_time();
      fdmi_dbg("P:POST_RDMA - WC_RDMA: %f", e - s);
#endif
#if  defined(DEBUG_RECV)
    fdmi_dbg("%s: msg_type->%d, rank: %d",  ibv_wc_opcode_str(wc->opcode), query->msg->type, 
	     fdmi_rmap_itop_get(conn_to_post_recv->rcid), query->msg->opcode, FDMI_EAGER, FDMI_RDMA_READ, FDMI_LOOP);
#endif
      /* query == passie_query */
      fdmi_on_recv_cq_wc_rdma_read(dctx, query);
    } else if (wc->opcode == IBV_WC_SEND) {
#if  defined(DEBUG_RECV)
    fdmi_dbg("%s: msg_type->%d, rank: %d",  ibv_wc_opcode_str(wc->opcode), query->msg->type, 
	     fdmi_rmap_itop_get(conn_to_post_recv->rcid), query->msg->opcode, FDMI_EAGER, FDMI_RDMA_READ, FDMI_LOOP);
#endif
      if (query->msg->type == FDMI_COMM_INIT) {
	/* query == active_query_to_free */
	fdmi_on_send_cq_wc_send(dctx, query);
	ct++;
      } else if (query->msg->type == FDMI_COMM_FIN) {
	/* query == active_query_to_free */
	fdmi_on_recv_cq_wc_send(dctx, query);
      }
    } else {
#if  defined(DEBUG_RECV)
      fdmi_err ("P: Unkown wc->opcode: %s(%d) (%s:%s:%d)",ibv_wc_opcode_str(wc->opcode), wc->opcode,  __FILE__, __func__, __LINE__);
#endif
    }
  }
}

static int fdmi_can_join()
{
  int i;
  for (i = 0; i < ft_msg.plength; i++) {
    if (ft_msg.eoffset[i] <= prank && prank < ft_msg.eoffset[i] + ft_msg.erange[i]) {
      return 1;
    }
  }
  return 0;
}

static int fdmi_need_update_ft_msg(int failed_rank)
{
  int i;
  for (i = 0; i < ft_msg.plength; i++) {
    if (ft_msg.offset[i] <= failed_rank && failed_rank < ft_msg.offset[i] + ft_msg.range[i]) {
      return 0;
    }
  }
  return 1;
}

static void fdmi_update_ft_msg(int failed_rank)
{
  int i;
  for (i = 0; i < ft_msg.plength; i++) {
    if (ft_msg.offset[i] <= failed_rank && failed_rank < ft_msg.offset[i] + ft_msg.range[i]) {
      return;
    }
  }
  ft_msg.offset[ft_msg.plength] = (failed_rank / fdmi_numproc) * fdmi_numproc;
  ft_msg.range[ft_msg.plength]  = fdmi_numproc;
  ft_msg.plength++;
  fdmi_reallocate_state_listd(ft_msg.offset, ft_msg.range, ft_msg.plength,
			      ft_msg.eoffset, ft_msg.erange, ft_msg.elength);
  ft_msg.elength = ft_msg.plength;
}


static struct fdmi_query* fdmi_init_passive_query(struct fdmi_post_recv_param *prparam)
{
  struct fdmi_domain             *domain;
  struct fdmi_query              *passive_query;
  struct fdmi_peer              **peers, *peer;
  struct fdmi_connection         *conn;
  struct fdmi_msg                *msg;
  struct ibv_mr                  *rdma_mr;

  uint64_t reg_size;
  
  peers	 = sendrecv_channel->peers;
  peer	 = peers[prparam->rank];

  conn   = NULL;
  /* conn   = peer->conn[0]; /\*TODO: Support for multiple HCAs. Currently we use first connection*\/ */
  /* if (conn == NULL) { */
  /*   fdmi_exit(1); */
  /* } */
  
  passive_query		      = fdmi_create_query(conn);
  passive_query->request_flag = prparam->request_flag;
  passive_query->request      = prparam->request;
  msg			      = passive_query->msg;
  msg->addr		      = (void*)prparam->addr;
  msg->rank		      = prparam->rank;
  msg->order		      = 0;	/* TODO: We need order ? => Yes, for multiple HCAs support*/
  msg->tag		      = prparam->tag;
  msg->count		      = prparam->count;
  msg->dtype		      = prparam->dtype;
  msg->comm_id		      = prparam->comm->id;
  msg->op		      = prparam->op;

  /*TODOP: If we recieve from eager buffer, reg_mr is not needed
    you may want to put this of after fdmi_gat_matched_query
  */
  reg_size =  (msg->dtype.stride * (msg->dtype.count - 1) + msg->dtype.blength) * msg->dtype.size * msg->count;

  domain = sendrecv_channel->domains[0];  /*TODO: Support for multiple HCAs. Currently we use first domain*/
  /*TODOB: Do not use rdma_mr.addr, because rdma_mr.addr can be different from prparam->addr.*/
  rdma_mr = fdmi_reg_mr(prparam->addr, reg_size, domain->dctx->pd);
  memcpy(&(msg->rdma_mr), rdma_mr, sizeof(struct ibv_mr));

  return passive_query;
}

void fdmi_post_recv_msg_param(struct fdmi_post_recv_param* prparam)
{
  struct fdmi_domain		 *domain;
  struct fdmi_query_qp		 *query_qp;
  struct fdmi_query		 *passive_query;
  struct fdmi_peer		**peers;
  struct fdmi_peer		 *connected_peer;
  struct fdmi_connection	 *conn;
  struct fdmi_msg		 *msg;
  struct ibv_mr			 *rdma_mr;
  uint64_t			  reg_size;
  int				  errno;
  

  /* TODO: Access to peer through Hashtable, binary tree */
  peers = sendrecv_channel->peers;
  
  /** =======================
   * TODO: Multiple HCA support
   *      For now, use first fdmi_domain to use a single HCA
  */
  /*FOR EACH (*/domain = *(sendrecv_channel->domains); /*)*/
  /*==========================*/
  {

    struct fdmi_query *matched_active_query;
    
    fdmi_wait_ready();
    query_qp = domain->dctx->query_qp;
    fdmi_queue_lock(query_qp->aq);
    fdmi_queue_lock(query_qp->pq);

    /* Search aq(active queue) and find out If the message has alrady arrived */
    if ((matched_active_query = fdmi_get_matched_query2(prparam->rank, prparam->tag, prparam->comm, query_qp->aq)) !=NULL) {
      /*If already arrived, ...*/
      fdmi_queue_remove(query_qp->aq, matched_active_query);
      if (matched_active_query->msg->opcode == FDMI_EAGER) {
	/*If the data is in the eager buffer, cpy the data to 
	  a user specified receive buffer*/
	fdmi_cpy_eager_buff2(prparam->addr, &prparam->dtype, prparam->request_flag, matched_active_query);
	prparam->request->prank = 
	    matched_active_query->msg->rank;
	prparam->request->tag   = 
	    matched_active_query->msg->tag;
      } else if (matched_active_query->msg->opcode == FDMI_RDMA_READ) {
	/*If the data is too big for the eager buffer space, 
	  read the data using RDMA from remote send buffer*/
	passive_query = fdmi_init_passive_query(prparam);

	passive_query->active_query_to_free = matched_active_query;
	fdmi_post_rdma_read(passive_query, matched_active_query);
      }
    } else {
      /*==========================*/
      passive_query = fdmi_init_passive_query(prparam);
      passive_query->msg->epoch = ft_msg.epoch;
      fdmi_queue_enq(query_qp->pq, passive_query);
#ifdef DEBUG_QP
      fdmi_dbg("P: ENQ(P): rank:%lu, tag:%lu, laddr:%p, len:%lu, lkey:%lu",
	       msg->rank, msg->tag, msg->rdma_mr.addr, msg->rdma_mr.length, msg->rdma_mr.lkey);
#endif
    }
    fdmi_queue_unlock(query_qp->pq);
    fdmi_queue_unlock(query_qp->aq);
    /* =============================== */
    return;
  }
}


static int fdmi_cpy_eager_buff (struct fdmi_query *passive_query, struct fdmi_query *active_query)
{
  struct fdmi_datatype *dtype;
  char *recv_buff, *eager_buff;
  int i,j;
  dtype = &(passive_query->msg->dtype);
  recv_buff = passive_query->msg->addr;
  eager_buff = active_query->msg->eager_buff;
  if (/*TODOP: if there is no stride, consecutive*/dtype->count == 1) {
    memcpy(recv_buff, eager_buff, dtype->blength * dtype->size * active_query->msg->count);
  } else {
    for (i = 0; i < active_query->msg->count; i++) {
      /*TODOP: if dtype->ccount == 1, skip "for (j = 0; j < dtype->count; j++)" */
      for (j = 0; j < dtype->count; j++) {
	memcpy(recv_buff, eager_buff, dtype->blength * dtype->size);
	recv_buff += dtype->stride * dtype->size;
	eager_buff += dtype->blength * dtype->size;
      }
    }
  }

  *(passive_query->request_flag) = 1;
  passive_query->request->prank = active_query->msg->rank;
  passive_query->request->tag   = active_query->msg->tag;
  //  fdmi_dbg("debug dao: rank %d, tag: %d", active_query->msg->rank, active_query->msg->tag);

  fdmi_queue_lock_remove(locked_request_sem_q, &(passive_query->request_flag));

  fdmi_destroy_query(passive_query);
  fdmi_destroy_query(active_query);
}

static int fdmi_cpy_eager_buff2(char *recv_buff, struct fdmi_datatype *dtype, volatile int* request_flag, struct fdmi_query *active_query)
{
  char *eager_buff;
  int i,j;
  //  dtype = &(passive_query->msg->dtype);
  //  recv_buff = passive_query->msg->addr;
  eager_buff = active_query->msg->eager_buff;
  if (/*TODOP: if there is no stride, consecutive*/dtype->count == 1) {
    memcpy(recv_buff, eager_buff, dtype->blength * dtype->size * active_query->msg->count);
  } else {
    for (i = 0; i < active_query->msg->count; i++) {
      /*TODOP: if dtype->ccount == 1, skip "for (j = 0; j < dtype->count; j++)" */
      for (j = 0; j < dtype->count; j++) {
	memcpy(recv_buff, eager_buff, dtype->blength * dtype->size);
	recv_buff += dtype->stride * dtype->size;
	eager_buff += dtype->blength * dtype->size;
      }
    }
  }

  *(request_flag) = 1;
  fdmi_queue_lock_remove(locked_request_sem_q, &request_flag);


  fdmi_destroy_query(active_query);
}

void fdmi_post_send_msg_param(struct fdmi_post_send_param* psparam)
{
  struct fdmi_peer		*peer;
  struct fdmi_connection	*conn;
  struct fdmi_query		*query;
  struct fdmi_msg		*msg;
  struct ibv_mr			*rdma_mr;
  uint64_t			 reg_size;

   /* TODO: Access to peer through Hashtable, binary tree */
  peer = *(sendrecv_channel->peers + psparam->rank);
  /* =======================
   * TODO: Multiple HCA support
   *      For now, use first fdmi_connection to use a single HCA
   */

  conn = *(peer->conn);
  /* if (conn == NULL) { */
  /*   fdmi_exit(1); */
  /* } else { */
  /*   fdmi_dbg("OK !"); */
  /* } */

  /* ========================== */
  query = fdmi_create_query(conn);
  query->request_flag  = psparam->request_flag;
  query->request      = psparam->request;
  query->msg->type    = FDMI_COMM_INIT;
  query->msg->opcode  = FDMI_RDMA_READ;
  query->msg->epoch   = ft_msg.epoch;
  query->msg->addr    = (void *)psparam->addr;
  query->msg->rank    = prank;
  query->msg->order   = 0;	/* TODO: We need order ? Yes, for multple HCA*/
  query->msg->tag     = psparam->tag;
  query->msg->count   = psparam->count;
  query->msg->comm_id = psparam->comm->id;
  query->msg->dtype   = psparam->dtype;
  //  query->msg->comm_id = psparam->comm->id;
  query->msg->op      = psparam->op;
  /*TODOP: if this is eager*/
  //  rdma_mr = fdmi_reg_mr(psparam->addr, psparam->length, conn->dctx->pd);
  reg_size = (query->msg->dtype.stride * (query->msg->dtype.count - 1) + query->msg->dtype.blength) * query->msg->dtype.size * query->msg->count;
  //  fdmi_dbg("%d %d %d %d %d", query->msg->dtype.stride, query->msg->dtype.count, query->msg->dtype.blength, query->msg->dtype.size, query->msg->count);
  /*TODOB: do not use rdma_mr.addr, because rdma_mr.addr can be different from psparam->addr*/
  rdma_mr = fdmi_reg_mr(psparam->addr, reg_size, conn->dctx->pd);
  memcpy(&(query->msg->rdma_mr), rdma_mr, sizeof(struct ibv_mr));


#ifdef DEBUG_SEND
  fdmi_dbg("C: Reg_mr   : rdma.addr=%p, rdma.len=%lu, rmda.rkey=%p",
	   rdma_mr->addr, rdma_mr->length, rdma_mr->rkey);
#endif

#ifdef DEBUG_TEST
  fdmi_test(query, __FILE__, __func__, __LINE__);
#endif
  fdmi_post_send_msg(query);

  return;
}

static void* fdmi_post_rdma_read(struct fdmi_query* passive_query, struct fdmi_query* active_query)
{
  struct ibv_send_wr swr;
  struct ibv_send_wr *bad_swr;
  struct ibv_sge sge;
  struct ibv_mr *local_mr, *remote_mr;
  struct fdmi_connection* conn;

  int errno;

  /* ============================
    - passive_query->conn = NULL;
    - active_query->conn> = <initialized value>;
    So Initialize passive_query->conn for conn_to_post_recv.
  */
  passive_query->conn = conn = active_query->conn;
  passive_query->msg_mr = fdmi_reg_mr(passive_query->msg, sizeof(struct fdmi_msg), conn->dctx->pd);
  /*=========================*/
  local_mr = &(passive_query->msg->rdma_mr);
  remote_mr = &(active_query->msg->rdma_mr);

  memset(&swr, 0, sizeof(struct ibv_send_wr));
  swr.wr_id = (uint64_t)passive_query;

  swr.opcode = IBV_WR_RDMA_READ;
  swr.sg_list = &sge;
  swr.num_sge = 1;
  swr.send_flags = IBV_SEND_SIGNALED;
  swr.wr.rdma.remote_addr = (uint64_t)remote_mr->addr;
  swr.wr.rdma.rkey        = remote_mr->rkey;
  swr.imm_data = prank; /*TODO: send more better data*/
  /* ================== */


  sge.addr   = (uint64_t)local_mr->addr;
  sge.length = (uint32_t)local_mr->length;
  sge.lkey   = (uint32_t)local_mr->lkey;


#if  defined(DEBUG_RECV) || defined(DEBUG_SEND)
  fdmi_dbg("C: Posting R: id=%lu, remot_addr=%p rkey=%p, imm_data=%d, local_addr=%p, length=%u, lkey=%p",
	   swr.wr_id, swr.wr.rdma.remote_addr, swr.wr.rdma.rkey, swr.imm_data, sge.addr, sge.length, sge.lkey);
#endif

  if ((errno = ibv_post_send(conn->qp, &swr, &bad_swr)) != 0) {
    fdmi_err ("ibv_post_send RDMA failed: %s(%d)  (%s:%s:%d)", rdma_err_status_str(errno), errno,  __FILE__, __func__, __LINE__);
  }
  /* if (prank == 2) { */
  /*   fdmi_dbg("C: Posting R: id=%lu, remot_addr=%p rkey=%p, imm_data=%d, local_addr=%p, length=%u, lkey=%p",  */
  /* 	     swr.wr_id, swr.wr.rdma.remote_addr, swr.wr.rdma.rkey, swr.imm_data, sge.addr, sge.length, sge.lkey); */
  /* } */

  return NULL;
}

static size_t fdmi_get_ctl_msg_size (struct fdmi_msg *msg) {
  size_t size = 0;
  size = sizeof(struct fdmi_msg) - FDMI_EAGER_BUFF_SIZE;
  return size;
}


static void* fdmi_post_send_msg(struct fdmi_query* query)
{
  struct ibv_send_wr	 swr;
  struct ibv_send_wr	*bad_swr;
  struct ibv_sge	 sge[2];	/*sge[0]:control message, sge[1]:eager data*/
  struct fdmi_query	*post_query;
  uint32_t		 total_send_size;
  struct fdmi_datatype	*dtype;
  int			 errno;

  //  double s = fdmi_get_time();
  //  double s1 = fdmi_get_time();
  memset(&swr, 0, sizeof(struct ibv_send_wr));
  swr.wr_id = (uint64_t)query;
  swr.opcode = IBV_WR_SEND;
  swr.sg_list = sge;
  swr.num_sge = 1;
  swr.send_flags = IBV_SEND_SIGNALED;
  swr.imm_data = prank; /*TODO: ib0, ib1, ib2, ... ?*/

#ifdef DEBUG_TEST
  fdmi_test(query, __FILE__, __func__, __LINE__);
  fdmi_test(query->msg, __FILE__, __func__, __LINE__);
  fdmi_test(query->msg_mr, __FILE__, __func__, __LINE__);
#endif
  sge[0].addr   = (uint64_t)query->msg;
  sge[0].length = (uint32_t)fdmi_get_ctl_msg_size(query->msg);//(uint32_t)sizeof(struct fdmi_msg);
  sge[0].lkey   = (uint32_t)query->msg_mr->lkey;
  /*TODO: do eager buffer related operation before calling this functioin*/
  dtype = &(query->msg->dtype);
  total_send_size =  dtype->blength * (dtype->count) * (query->msg->count) * (query->msg->dtype.size);
  if (total_send_size <= FDMI_EAGER_BUFF_SIZE) {
    swr.num_sge += 1;
    query->msg->opcode = FDMI_EAGER;
    query->msg->eager_size = total_send_size; /*TODO: eager_size is not used on a recv side*/
    if (/*TODO: if sending data is consecutive,*/dtype->count == 1) {
      sge[1].addr   = (uint64_t)query->msg->addr;
      sge[1].length = (uint32_t)total_send_size;
      sge[1].lkey   = (uint32_t)query->msg->rdma_mr.lkey;
    } else {
      /*TODO: define as a function below, which does a same thing in cpy_eager*/
      char *send_buff, *eager_buff;
      int i,j;
      dtype	 = &(query->msg->dtype);
      send_buff	 = query->msg->addr;
      eager_buff = query->msg->eager_buff;
      for (i = 0; i < query->msg->count; i++) {
	/*TODOP: if dtype->ccount == 1, skip "for (j = 0; j < dtype->count; j++)" */
	for (j = 0; j < dtype->count; j++) {
	  memcpy(eager_buff, send_buff, dtype->blength * dtype->size);
	  send_buff  += dtype->stride * dtype->size;
	  eager_buff += dtype->blength * dtype->size;
	}
      }
      sge[1].addr   = (uint64_t)eager_buff;
      sge[1].length = (uint32_t)total_send_size;
      sge[1].lkey   = (uint32_t)query->msg_mr->lkey;
    }
  }
#if  defined(DEBUG_RECV) || defined(DEBUG_SEND)
  fdmi_dbg("C: Created Send[0]: id=%p, imm_data=%d, local_addr=%p, length=%u, lkey=%p, qp=%p, pd=%p", query, swr.imm_data, sge[0].addr, sge[0].length, sge[0].lkey, query->conn->qp, query->conn->dctx->pd);
  if (swr.num_sge > 1) {
    fdmi_dbg("C: Created Send[1]: id=%p, imm_data=%d, local_addr=%p, length=%u, lkey=%p, qp=%p", query, swr.imm_data, sge[1].addr, sge[1].length, sge[1].lkey, query->conn->qp);
  }
#endif

  //  double e = fdmi_get_time();
  //  fdmi_dbg("2.1===> %f", e - s);
  //  s = fdmi_get_time();
#ifdef DEBUG_PERF_1
  fe = fdmi_get_time();
  fdmi_dbg("A:CALL_SEND - CL_POST= %f", fe - fs);
  ps = fdmi_get_time();
#endif

  if ((errno = ibv_post_send(query->conn->qp, &swr, &bad_swr)) != 0) {
    fdmi_err("ibv_post_send(active side) failed: qp=%p, &swr=%p bad_swr=%p errno=%d (%s:%s:%d)", query->conn->qp, &swr, bad_swr, errno,  __FILE__, __func__, __LINE__);
  }

#ifdef DEBUG_PERF_1
  pe = fdmi_get_time();
  fdmi_dbg("A:CALL_POST - ED_POST= %f", pe - ps);
#endif
  //  e = fdmi_get_time();
  //  fdmi_dbg("2.2===> %f", e - s);
#if  defined(DEBUG_RECV) || defined(DEBUG_SEND)
  fdmi_dbg("C: Post Send[0]: id=%p, imm_data=%d, local_addr=%p, length=%u, lkey=%p, qp=%p", query, swr.imm_data, sge[0].addr, sge[0].length, sge[0].lkey, query->conn->qp);
  if (swr.num_sge > 1) {
    fdmi_dbg("C: Post Send[1]: id=%p, imm_data=%d, local_addr=%p, length=%u, lkey=%p, qp=%p", query, swr.imm_data, sge[1].addr, sge[1].length, sge[1].lkey, query->conn->qp);
  }
#endif
#if   defined(DEBUG_PERF) || defined(DEBUG_WC_SEND)
  gs = fdmi_get_time();
#endif
  return NULL;
}

static struct fdmi_query* fdmi_post_recv_msg(struct fdmi_query* query)
{
  struct ibv_recv_wr rwr;
  struct ibv_recv_wr *bad_rwr = NULL;
  struct ibv_sge sge[2]; /*sge[0]: control message, sge[1]:eager buffer*/
  int errno;

  memset(&rwr, 0, sizeof(struct ibv_recv_wr));
  rwr.wr_id   = (uint64_t)query;
  rwr.next    = NULL;
  rwr.sg_list = sge;
  rwr.num_sge = 2;

  sge[0].addr	= (uint64_t)query->msg;
  sge[0].length = (uint32_t)fdmi_get_ctl_msg_size(query->msg);	//(uint32_t)sizeof(struct fdmi_msg);
  sge[0].lkey	= query->msg_mr->lkey;

  sge[1].addr	= (uint64_t)query->msg->eager_buff;
  sge[1].length = FDMI_EAGER_BUFF_SIZE;
  sge[1].lkey	= query->msg_mr->lkey;

#ifdef DEBUG_TEST
  fdmi_test(query->conn->qp, __FILE__, __func__, __LINE__); /* <===== */
  fdmi_test(query->conn->qp->context, __FILE__, __func__, __LINE__);
  fdmi_test(query->conn->qp->context->ops, __FILE__, __func__, __LINE__);
  fdmi_test(query->conn->qp->context->ops.post_recv, __FILE__, __func__, __LINE__);
#endif
  if ((errno = ibv_post_recv(query->conn->qp, &rwr, &bad_rwr)) != 0) {
    fdmi_err ("ibv_post_recv failed (%s:%s:%d)", __FILE__, __func__, __LINE__);
  }
#if  defined(DEBUG_RECV) || defined(DEBUG_SEND)
  fdmi_dbg("C: Post Recv[0]: id=%lu, local_addr=%p, length=%u, lkey=%p, qp=%p", (uintptr_t)query,  sge[0].addr, sge[0].length, sge[0].lkey, query->conn->qp);
  fdmi_dbg("C: Post Recv[1]: id=%lu, local_addr=%p, length=%u, lkey=%p, qp=%p", (uintptr_t)query,  sge[1].addr, sge[1].length, sge[1].lkey, query->conn->qp);
#endif
  return query;

}


int conc = 0;

static void* fdmi_poll_event_recv_channel(void* arg)
{
  struct rdma_cm_event				 *event = NULL;
  struct rdma_conn_param			  params;
  struct fdmi_poll_event_recv_channel_arg	 *pe_arg;
  struct fdmi_domain				 *domain;
  struct fdmi_peer				**peers;
  struct fdmi_peer				 *connected_peer;
  struct fdmi_connection			 *conn;
  struct fdmi_query				 *post_query;
  struct fdmi_query_qp				 *flush_query_qp;


  int	 domain_id;
  int	*failed_rank, frank;
  sem_t *request_sem_to_abort;
  //  int	*connected_rank;
  int	 rc = 0;
  int	 errno;
  int	 i;
  /*For Fault tolerance*/
  int    *disconnected_prank;
  struct fdmi_request ft_req;

  pe_arg    = (struct fdmi_poll_event_recv_channel_arg*)arg;
  domain_id = pe_arg->domain_id;
  domain    = pe_arg->domain;
  peers	    = pe_arg->peers;

  while (1) {
#if defined(DEBUG_P_INIT)
    fdmi_dbg("P:Waiting P_EVENT: domain:%p", domain);
#endif
    if ((rc = rdma_get_cm_event(domain->rec, &event))){
      fdmi_err("Get event failed: %d\n", rc);
      break;
    }

#if defined(DEBUG_P_INIT)
    fdmi_dbg("P:P_EVENT=\"%s\"", event_type_str(event->event));
#endif

    unsigned int v;
    switch (event->event){
    case RDMA_CM_EVENT_CONNECT_REQUEST:
      //      connected_rank  = (int *)event->param.conn.private_data;
      connected_peer  = peers[connected_rank];

      //      fdmi_dbg( "connection from %d", connected_rank);
      if (pthread_mutex_trylock(&(connected_peer->on_connecting_lock))) {
      	if ((errno = rdma_reject(event->id, NULL, 0)) > 0 ) {
      	  fdmi_err("P:rdma_reject failed\n", rc);
      	}
      	//	fdmi_dbg("rejected connection");
      	break;
      }

      fdmi_create_qp (&(domain->dctx), event->id);
      conn = fdmi_create_connection(event->id, domain->dctx);

      connected_peer->conn[domain_id] = conn;


      fdmi_build_params(&params);
      if ((errno = rdma_accept(event->id, &params)) > 0 ) {
	fdmi_err("P:rdma_accept failed\n", rc);
      }
      //kento
      /*Process mapping*/
      fdmi_rmap_itop_add((void*)conn->rcid, connected_rank);
      connected_rank++; /*Increment for next connecting rank*/

      fdmi_dbg("fdmi_size: %d, connected rank: %d, connected_peer: %p, domain_id: %d", fdmi_size, connected_rank, connected_peer, domain_id);
      break;
    case RDMA_CM_EVENT_ESTABLISHED:

#if defined(DEBUG_P_INIT)
      fdmi_dbg("P:Establish: host_id=%lu, rank:%d", (uintptr_t)event->id,  fdmi_rmap_itop_get(event->id));
#endif
      if (!is_recv_cq_polling) {
	is_recv_cq_polling = 1;
	if ((errno = pthread_create(&(domain->dctx->cq_poller_thread), NULL, fdmi_poll_sendrecv_cq, (void *)domain->dctx)) > 0) {
	  fdmi_err ("pthread_create failed (%s:%s:%d)", __FILE__, __func__, __LINE__);
	}
    
	if ((errno = pthread_detach(domain->dctx->cq_poller_thread)) > 0) {
	  fdmi_err ("pthread_detach failed (%s:%s:%d)",  __FILE__, __func__, __LINE__);
	}
      }

      conn = sendrecv_channel->peers[fdmi_rmap_itop_get(event->id)]->conn[domain_id];
      for (i = 0; i < FDMI_MAX_PENDING_RECV_QUERY; i++) {
	fdmi_post_recv_msg(fdmi_create_query(conn));
      }
      //      fdmi_dbg("===> post conn: %p qp: %p from %d", conn, conn->qp, fdmi_rmap_itop_get(event->id));
      sendrecv_channel->peers[fdmi_rmap_itop_get(event->id)]->is_connected = 1;
      conc++;
      //      fdmi_dbgi(i, "conc %d", conc);
      //if (conc == 8) return;
      break;
    case RDMA_CM_EVENT_DISCONNECTED:
      /*TODOB: Disconnect later*/
      /*      rdma_disconnect(event->id);*/
      /* frank = fdmi_rmap_itop_get(event->id); */
      /* if (prank/fdmi_numproc == frank/fdmi_numproc) { */
      /* 	fdmi_err("frank %d is on same node", frank); */
      /* } */
      if (fdmi_need_update_ft_msg(frank)) {
	fdmi_state = FDMI_FAILURE;
	disconnected_prank = (int*)fdmi_malloc(sizeof(int));
	*disconnected_prank = fdmi_rmap_itop_get(event->id);
      //      fdmi_dbg("P:Disconnect from rank: %d", *disconnected_prank);
	fdmi_queue_lock_enq(disconnected_prank_q, disconnected_prank);
      }
      //      fdmi_dbg("enq %d  disconnected(addr:%p)", *disconnected_prank, disconnected_prank);
      break;
#if defined(DEBUG_P_INIT)
      fdmi_dbg("P:Disconnect from rank: %d", fdmi_rmap_itop_get(event->id));
#endif
      /*TODO: Distinguish disconnection by failure or disconnection operation,
	for now, assume failure*/
      is_flushed = 0;
      fdmi_state = FDMI_FAILURE;

      /*TODOB: ongoing send/recv operation can:
	1. Recieve message after flush
	2. Recovery mesage can be flushed
       */
      usleep(400000);// /*For 1.*/
      flush_query_qp = domain->dctx->query_qp;
      fdmi_flush_query_qp(flush_query_qp);
      while (fdmi_queue_lock_deq(send_req_q) != NULL);
      //      fdmi_dbg("deqx: %d (ct:%d))", fdmi_queue_length(send_req_q), ct);
      is_flushed = 1;
      /*For 2. call before fdmi_queue_lock_enq(failed_prank_q, failed_rank);*/
      //      fdmi_dbgi(0, "Flushed");

      failed_rank = (int*)fdmi_malloc(sizeof(int));
      *failed_rank = fdmi_rmap_itop_get(event->id);
      fdmi_queue_lock_enq(failed_prank_q, failed_rank);
      //TODO: we need to report at a point later
      //      fdmi_rank_man_report_failure(failed_rank);
      while ((request_sem_to_abort = (sem_t*)fdmi_queue_lock_deq(locked_request_sem_q)) != NULL) {
      	if ((errno = sem_post(request_sem_to_abort)) > 0) {
      	  fdmi_err ("sem_post failed: errno=%d(%d)  (%s:%s:%d)", errno, EPERM, __FILE__, __func__, __LINE__);
      	}
	if ((errno = sem_destroy(request_sem_to_abort)) > 0) {
	  fdmi_err ("sem_destroy failed  (%s:%s:%d)", __FILE__, __func__, __LINE__);
	}
      }
      //      fdmi_dbg("P:Disconnect from rank: %d", fdmi_rmap_itop_get(event->id));
      //      sleep(10);
      //      fdmi_exit(1);

      /*TODO: destroy the disconnected resource of event->id*/
      break;
    case RDMA_CM_EVENT_ADDR_RESOLVED:
      fdmi_create_qp(&((domain)->dctx), event->id);
      conn = fdmi_create_connection(event->id, domain->dctx);
      /* =====================================================
	1. If the conn[domain_id] is not NULL, 
	 then connection was already made with domain_id, and
	 this connection is rejected by a passive side, so do nothing.
	2. If the conn[domain_id] is NULL and previus connection is on going, 
	 then conn[domain_id] will be overwritten.
	3. If the conn[domain_id] is NULL and previus connection not is on going, 
	 then this connection will be used.	 
      ========================================================= */

      if (sendrecv_channel->peers[fdmi_rmap_itop_get(event->id)]->conn[domain_id] == NULL) {
	sendrecv_channel->peers[fdmi_rmap_itop_get(event->id)]->conn[domain_id] = conn;
	if (rdma_resolve_route(event->id, TIMEOUT_IN_MS)) {
	  fdmi_err ("rdma_resolve_route failed: (%s:%s:%d)",  __FILE__, __func__, __LINE__);
	}
      }
      //      fdmi_dbgi(0, "1. connect to id: %p", event->id);
      break;
    case RDMA_CM_EVENT_ROUTE_RESOLVED:
      fdmi_build_params(&params);

      if (rdma_connect(event->id, &params)) {
	fdmi_err ("rdma_connect failed (%s:%s:%d)", __FILE__, __func__, __LINE__);
      }
      //      fdmi_dbgi(0, "2. connect to id: %p", event->id);
      break;
    case RDMA_CM_EVENT_REJECTED:
      //fdmi_dbg("RDMA_CM_EVENT_REJECTED: id: %p, reason:%d", event->id, event->status);
      break;
    default:
#if defined(DEBUG_P_INIT) || defined(DEBUG_A_INIT)
      fdmi_err("\"%s\" was not handled\n", event_type_str(event->event));
#endif
      break;
    }
    rdma_ack_cm_event(event);
  }
}


//static void fdmi_flush_old_query()
//{

//}

void fdmi_flush_all_query_qp()
{

  struct fdmi_domain **domain;
  struct fdmi_query_qp *query_qp;
  int i;

  for (i = 0, domain = sendrecv_channel->domains;
       i < sendrecv_channel->numdomain;
       i++, domain++) {
    query_qp = (*domain)->dctx->query_qp;
    fdmi_flush_query_qp(query_qp);
  }
  return;
}


static void fdmi_flush_query_qp(struct fdmi_query_qp *qqp)
{
  struct fdmi_queue	*aq = qqp->aq;
  struct fdmi_queue	*pq = qqp->pq;
  struct fdmi_query	*query;
  struct fdmi_msg	*msg;

  //    fdmi_exit(1);
  //  fdmi_dbg("0. sleep");
  //  sleep(10);

  fdmi_queue_lock(aq);
  fdmi_queue_lock(pq);
  //  fdmi_dbgi(33, "0. ==>> query:%p len:%d",  query, fdmi_queue_length(aq));
  for (query = (struct fdmi_query*)fdmi_queue_iterate(aq, FDMI_QUEUE_HEAD); query != NULL;
       query = (struct fdmi_query*)fdmi_queue_iterate(aq, FDMI_QUEUE_NEXT)) {
    //    fdmi_dbgi(33, "0.5. ==>> query:%p len:%d",  query, fdmi_queue_length(aq));
    //    fdmi_dbg("1. ==>> query:%p, msg:%p",  query, query->msg);
    //    fdmi_dbg("2. ==>> %d, %d, %d",  query->msg->op, FDMI_PRESV, query->msg->op & FDMI_PRESV);
#ifdef DEBUG_TEST
    fdmi_test(query, __FILE__, __func__, __LINE__);
    fdmi_test(query->msg, __FILE__, __func__, __LINE__);
#endif

    if (!(query->msg->op & FDMI_PRESV)) {
      fdmi_queue_remove(aq, query);
      fdmi_destroy_query(query);
    }
  }

  for (query = (struct fdmi_query*)fdmi_queue_iterate(pq, FDMI_QUEUE_HEAD); query != NULL;
       query = (struct fdmi_query*)fdmi_queue_iterate(pq, FDMI_QUEUE_NEXT)) {
#ifdef DEBUG_TEST
    fdmi_test(query, __FILE__, __func__, __LINE__);
    fdmi_test(query->msg, __FILE__, __func__, __LINE__);
#endif
    if (!(query->msg->op & FDMI_PRESV)) {
      fdmi_queue_remove(pq, query);
      fdmi_destroy_query(query);
    }
  }
  fdmi_queue_unlock(aq);
  fdmi_queue_unlock(pq);

}

static void fdmi_run_event_channel_poller(struct fdmi_sendrecv_channel *sendrecv_channel)
{
  struct fdmi_domain **domain;
  struct fdmi_poll_event_recv_channel_arg *arg;
  int errno;
  int i;

  for (i = 0, domain = sendrecv_channel->domains;
       i < sendrecv_channel->numdomain;
       i++, domain++) {

    arg = (struct fdmi_poll_event_recv_channel_arg*)fdmi_malloc(sizeof(struct fdmi_poll_event_recv_channel_arg));
    arg->domain_id = i;
    arg->domain    = *domain;
    arg->peers     =  sendrecv_channel->peers;
    //if ((errno = pthread_create(&((*domain)->ec_poller_thread), NULL, fdmi_poll_event_recv_channel, (void*)(*domain))) > 0) {
    if ((errno = pthread_create(&((*domain)->ec_poller_thread), NULL, fdmi_poll_event_recv_channel, (void*)arg)) > 0) {
          fdmi_err ("pthread_create failed (%s:%s:%d)", __FILE__, __func__, __LINE__);
    }
    
    if ((errno = pthread_detach((*domain)->ec_poller_thread)) > 0) {
      fdmi_err ("pthread_detach failed (%s:%s:%d)",  __FILE__, __func__, __LINE__);
      
    }
  }
  return;
}

/* static void fdmi_run_event_channel_poller(struct fdmi_recv_channel *recv_channel) OLD*/
/* { */
/*   struct fdmi_domain **domain; */
/*   struct fdmi_poll_event_recv_channel_arg *arg; */
/*   int errno; */
/*   int i; */

/*   for (i = 0, domain = recv_channel->domains; */
/*        i < recv_channel->numdomain;  */
/*        i++, domain++) { */
/*     arg = (struct fdmi_poll_event_recv_channel_arg*)fdmi_malloc(sizeof(struct fdmi_poll_event_recv_channel_arg)); */
/*     arg->domain_id = i; */
/*     arg->domain    = *domain; */
/*     arg->peers     = recv_channel->peers; */
/*     //if ((errno = pthread_create(&((*domain)->ec_poller_thread), NULL, fdmi_poll_event_recv_channel, (void*)(*domain))) > 0) { */
/*     if ((errno = pthread_create(&((*domain)->ec_poller_thread), NULL, fdmi_poll_event_recv_channel, (void*)arg)) > 0) { */
/*           fdmi_err ("pthread_create failed (%s:%s:%d)", __FILE__, __func__, __LINE__); */
/*     } */
    
/*     if ((errno = pthread_detach((*domain)->ec_poller_thread)) > 0) { */
/*       fdmi_err ("pthread_detach failed (%s:%s:%d)",  __FILE__, __func__, __LINE__); */
      
/*     } */
/*   } */
/*   return; */
/* } */

static void fdmi_make_alltoall_connection(struct fdmi_sendrecv_channel* sendrecv_channel)
{
  struct fdmi_domain **domain;

  int	port_count;
  int	errno;
  int	rank;
  int   i,j,k;

  /* ====== */
  /* For each rank */
  rank = (prank + 1) % fdmi_size;
  for (j = 0; j < fdmi_size; j++, rank = (rank + 1) % fdmi_size) {
    struct rdma_conn_param	cm_params;
    struct rdma_cm_id*		new_rcid;
    struct fdmi_peer*		peer;
    struct fdmi_connection*	conn;
    struct addrinfo*		addr;

    char	port[32];

    if (prank == rank) continue;

    fdmi_connect(sendrecv_channel, rank);

#ifdef DEBUG_A_INIT
    fdmi_dbg("A:Connected: Domain:%p, Hostname:%s(%d):%d", *domain, hostname, j, rank);
#endif
  }

  /* For each domain */
  for (i = 0, domain = sendrecv_channel->domains;
       i < sendrecv_channel->numdomain;
       i++, domain++) {

    if ((errno = pthread_create(&((*domain)->dctx->cq_poller_thread), NULL, fdmi_poll_send_cq, (void *)((*domain)->dctx))) > 0) {
      fdmi_err ("pthread_create failed (%s:%s:%d)", __FILE__, __func__, __LINE__);
    }
    
    if ((errno = pthread_detach(((*domain)->dctx->cq_poller_thread))) > 0) {
      fdmi_err ("pthread_detach failed (%s:%s:%d)",  __FILE__, __func__, __LINE__);
    }

  }
  return;
}

static void fdmi_make_line_connection(struct fdmi_sendrecv_channel* sendrecv_channel)
{
  struct fdmi_domain**		domain;
  struct rdma_conn_param	cm_params;
  struct rdma_cm_id*		new_rcid;
  struct fdmi_peer*		peer;
  struct fdmi_connection*	conn;
  struct addrinfo*		addr;

  char	port[32];

  int	port_count;
  int	errno;
  int	rank;
  int   i,j,k;

  /* connect to neightber rank */
  if (prank != 0) {
    rank = (prank + (fdmi_size - 1)) % fdmi_size;
    fdmi_connect(sendrecv_channel, rank);
  }

#ifdef DEBUG_A_INIT
    fdmi_dbg("A:Connected: Domain:%p, Hostname:%s(%d):%d", *domain, hostname, j, rank);
#endif
  /* For each domain */
  /* for (i = 0, domain = sendrecv_channel->domains; */
  /*      i < sendrecv_channel->numdomain; */
  /*      i++, domain++) { */

  /*   if ((errno = pthread_create(&((*domain)->dctx->cq_poller_thread), NULL, fdmi_poll_send_cq, (void *)((*domain)->dctx))) > 0) { */
  /*     fdmi_err ("pthread_create failed (%s:%s:%d)", __FILE__, __func__, __LINE__); */
  /*   } */
    
  /*   if ((errno = pthread_detach(((*domain)->dctx->cq_poller_thread))) > 0) { */
  /*     fdmi_err ("pthread_detach failed (%s:%s:%d)",  __FILE__, __func__, __LINE__); */
  /*   } */

  /* } */

  return;
}

static void fdmi_make_ring_connection(struct fdmi_sendrecv_channel* sendrecv_channel)
{
  struct fdmi_domain**		domain;
  struct rdma_conn_param	cm_params;
  struct rdma_cm_id*		new_rcid;
  struct fdmi_peer*		peer;
  struct fdmi_connection*	conn;
  struct addrinfo*		addr;

  char	port[32];

  int	port_count;
  int	errno;
  int	rank;
  int   i,j,k;

  /* connect to neightber rank */

  rank = (prank + (fdmi_size - 1)) % fdmi_size;
  fdmi_connect(sendrecv_channel, rank);

  return;
}



static void fdmi_init_proc_man(void) {
  int i;
  /*TODO: I multipled size of rank_map_vtop by 2(extra) for dynamic scaling, but the magic number should be removed */
  fdmi_proc_man_init(1024 * 1024, 0, 1, 0);
  /**/
}


int fdmi_verbs_finalize()
{
  /*TODO*/
  return 0;
}
int fdmi_verbs_init(int *argc, char ***argv)
{
  struct rdma_cm_event *event = NULL;
  int errno;

  /* Initialize a  prefix messsage for debug/erro message */
  fdmi_err_init();

  /* Initialize configuration variables */
  fdmi_set_envs();

  /* Initialize a process manager*/
  fdmi_init_proc_man();

  ft_msg.id = 0;
  ft_msg.plength = 0;
  ft_msg.epoch = 0;

  /* ============================  */
  /*  TODO: Find a way */
  /*  To make sure any operations do not start before the initialization is finished  */
  /*  if ((errno = pthread_mutex_init(&(comm->post_mutex), NULL)) > 0) { */
  /*    fdmi_err ("pthread_mutex_init failed (%s:%s:%d)", __FILE__, __func__, __LINE__); */
  /*  } */
  /* ============================ */

  /* Create RDMA event channel. One channel is enough for most case except bunch of clients try to connect */
  sendrecv_channel = fdmi_create_sendrecv_channel();

  /* Bind to HCA/port, and create qpt */
  fdmi_bind_addr_and_listen(sendrecv_channel);

  /* Handle connection requests from other processes */
  fdmi_run_event_channel_poller(sendrecv_channel);

  /* ============== Barrier ================ */
  /* TODO: Make sure all processes reach and listneing the connection here */
  sleep(1);
  /* ============== Barrier ================ */

  /* Make line network where a proces connects a neightber */
  //  fdmi_make_line_connection(sendrecv_channel);
  /* Make ring network where a proces connects a neightber */
   //   if (prank == 0) sleep(1);
  //  fdmi_make_ring_connection(sendrecv_channel);



  /*If a process is standby one, the process is barriered in this 
    line until the process is elected for recovery process*/
  //  fdmi_barrier_standby();
  return 0;
}

/*
connect to "hostname". "rank" is associated with "hostname", 
so a user need to use "rank" for the succesive communication function, isend or irecv.
*/
struct fdmi_connection* fdmi_verbs_connect(int rank, char *hostname)
{
  struct rdma_conn_param	cm_params;
  struct rdma_cm_id*		new_rcid;
  struct fdmi_domain**		domain;
  struct fdmi_peer*		peer;
  struct fdmi_connection**	conn;
  struct addrinfo*		addr;
  
  char*		nodelist;
  char		port[32];
  int		fdmi_numnode;
  int           nodeid;
  int		i, k;


  peer	   = sendrecv_channel->peers[rank];
  for (i = 0, domain = sendrecv_channel->domains;
       i < sendrecv_channel->numdomain;
       i++, domain++)
    {
      int ret = 0;
      int is_locked = 0;

      conn = peer->conn + i;

      if (rdma_create_id((*domain)->rec, &new_rcid, NULL, RDMA_PS_TCP)){
	fdmi_err ("rdma_create_id failed (%s:%s:%d)", __FILE__, __func__, __LINE__);
      }
      sprintf(port, "%d", IBTL_LISTEN_PORT);
      fdmi_rmap_itop_add((void*)new_rcid, rank);

#ifdef DEBUG_P_INIT
      fdmi_dbg("A:Connecting: Domain:%p, Hostname:%s(%d), port:%s", *domain, hostname, rank, port);
#endif

      if(getaddrinfo(hostname, port, NULL, &addr)){
	fdmi_err("getaddrinfo failed (%s:%s:%d)", __FILE__, __func__, __LINE__);
      }
      if (rdma_resolve_addr(new_rcid, NULL, addr->ai_addr, TIMEOUT_IN_MS)) {
	fdmi_err ("rdma_resolve_addr failed (%s:%s:%d)", __FILE__, __func__, __LINE__);
      }
      while (fdmi_is_connected(rank) == 0) {
	usleep(1000);
	//	fdmi_dbg("waiting connection to rank:%d", rank);
      }
      //      fdmi_dbg("Connected: lock: %d, rank:%d, is_connected:%d", ret, rank, sendrecv_channel->peers[rank]->is_connected);
    }

  return *conn;
}

struct fdmi_connection* fdmi_connect(struct fdmi_sendrecv_channel* sendrecv_channel, int rank)
{
  struct rdma_conn_param	cm_params;
  struct rdma_cm_id*		new_rcid;
  struct fdmi_domain**		domain;
  struct fdmi_peer*		peer;
  struct fdmi_connection**	conn;
  struct addrinfo*		addr;
  
  char**	co_nodelist;
  char*		nodelist;
  char*         hostname;
  char		port[32];
  int		fdmi_numnode;
  int           nodeid;
  int		i, k;

  //  fdmi_dbg("try connection to %d", rank);

  return;
  /* Assumption:  fdmi_numnode == fdmi_size / fdmi_numproc; */
  fdmi_numnode = fdmi_size / fdmi_numproc;
  nodelist     = (char* )fdmi_malloc(sizeof(char ) * FDMI_HOSTNAME_LIMIT * fdmi_numnode);
  co_nodelist  = (char**)fdmi_malloc(sizeof(char*) * fdmi_numnode);

  sprintf(nodelist, "%s", fdmi_param_get("FDMI_NODELIST"));
  for (i = 0, hostname = strtok(nodelist, ","); hostname != NULL; hostname = strtok(NULL, ","), i++) {
    *(co_nodelist + i) = hostname;
  }

  nodeid   = rank / fdmi_numproc;
  hostname = co_nodelist[nodeid];//*(co_nodelist + nodeid);
  peer	   = sendrecv_channel->peers[rank];//*(sendrecv_channel->peers + rank);

  for (i = 0, domain = sendrecv_channel->domains;
       i < sendrecv_channel->numdomain;
       i++, domain++)
    {
      int ret = 0;
      int is_locked = 0;

      conn = peer->conn + i;
    
      if (prank < rank) {
	ret = 0;
      } else {
	ret = pthread_mutex_trylock(&(peer->on_connecting_lock));
	if (!ret) is_locked = 1;
      }


      if (!ret) {
	if (rdma_create_id((*domain)->rec, &new_rcid, NULL, RDMA_PS_TCP)){
	  fdmi_err ("rdma_create_id failed (%s:%s:%d)", __FILE__, __func__, __LINE__);
	}

	sprintf(port, "%d", IBTL_LISTEN_PORT);
	fdmi_rmap_itop_add((void*)new_rcid, rank);

#ifdef DEBUG_P_INIT
	fdmi_dbg("A:Connecting: Domain:%p, Hostname:%s(%d), port:%s", *domain, hostname, rank, port);
#endif
	if(getaddrinfo(hostname, port, NULL, &addr)){
	  fdmi_err("getaddrinfo failed (%s:%s:%d)", __FILE__, __func__, __LINE__);
	}
	if (rdma_resolve_addr(new_rcid, NULL, addr->ai_addr, TIMEOUT_IN_MS)) {
	  fdmi_err ("rdma_resolve_addr failed (%s:%s:%d)", __FILE__, __func__, __LINE__);
	}

	//	fdmi_dbg("lock: %d, rank:%d, is_connected:%d", ret, rank, sendrecv_channel->peers[rank]->is_connected);
      
      }
      
      while (fdmi_is_connected(rank) == 0) {
	usleep(1000);
	//	fdmi_dbg("waiting connection to rank:%d", rank);
      }
      //      fdmi_dbg("=====> lock: %d, rank:%d, is_connected:%d", ret, rank, sendrecv_channel->peers[rank]->is_connected);
    
    /*TODO: Do we need to unlock and free ?*/
    /* if (is_locked) { */
    /*   return; */
    /* } */
    }

  return *conn;
}

static int fdmi_is_connected(int prank)
{
  struct fdmi_peer *peer;

  if (prank == FMI_ANY_SOURCE) return 1;

  peer = sendrecv_channel->peers[prank];
  return peer->is_connected;
}

int fdmi_verbs_comm_size(struct fdmi_communicator *comm, int *size)
{
  *size = comm->comm_size;
  return 0;
}

int fdmi_verbs_comm_vrank(struct fdmi_communicator *comm, int *vrank)
{

  *vrank = fdmi_comm_get_vrank(comm, prank);
  //  *vrank = comm.vrank;
  return 0;
}

int fdmi_verbs_wait(struct fdmi_request* request, struct fdmi_status *status)
{
  int errno;

  while(!fdmi_verbs_test(request, status));

  if (status != NULL) {
    status->FMI_SOURCE = request->prank;
    status->FMI_TAG    = request->tag;
  }

  return FMI_SUCCESS;
}

int fdmi_verbs_test(struct fdmi_request *request, struct fdmi_status *status)
{
  return request->request_flag;
}

int fdmi_verbs_join(void)
{
  if (fdmi_state == FDMI_COMPUTE ||
      fdmi_state == FDMI_FAILURE ) return -1;

  if(fdmi_state == FDMI_RECOVERY) {
    fdmi_state = FDMI_COMPUTE;
    //    fdmi_dbg("epoch: %d", ft_msg.epoch);
    return 0;
  } else if (fdmi_state == FDMI_JOINING){
    fdmi_state = FDMI_COMPUTE;
    //    fdmi_dbg("Join in epoch: %d", ft_msg.epoch);
    return 1;
  }
  
  fdmi_err("Invalide state: %d", fdmi_state);
}

void show_query_qp_content()
{
  struct fdmi_queue *aq, *pq;
  struct fdmi_query *tmp_query;
  struct fdmi_msg *msg;

  aq = sendrecv_channel->domains[0]->dctx->query_qp->aq;
  fdmi_queue_lock(aq);
  fprintf(stderr, "A:");
  for (tmp_query = (struct fdmi_query*)fdmi_queue_iterate(aq, FDMI_QUEUE_HEAD); tmp_query != NULL;
       tmp_query = (struct fdmi_query*)fdmi_queue_iterate(aq, FDMI_QUEUE_NEXT)) {
    msg = tmp_query->msg;
    fprintf(stderr, "(e:%d, c:%d, r:%d, t:%d)", msg->epoch, msg->comm_id, msg->rank, msg->tag);
  }
  fdmi_queue_unlock(aq);
  fprintf(stderr, "\n");

  pq = sendrecv_channel->domains[0]->dctx->query_qp->pq;
  fdmi_queue_lock(pq);
  fprintf(stderr, "P:");
  for (tmp_query = (struct fdmi_query*)fdmi_queue_iterate(pq, FDMI_QUEUE_HEAD); tmp_query != NULL;
       tmp_query = (struct fdmi_query*)fdmi_queue_iterate(pq, FDMI_QUEUE_NEXT)) {
    msg = tmp_query->msg;
    fprintf(stderr, "(e:%d, c:%d, r:%d, t:%d)", msg->epoch, msg->comm_id, msg->rank, msg->tag);
  }
  fdmi_queue_unlock(pq);
  fprintf(stderr, "\n");

  return;
}



//TODO: NOT struct fdmi_communicator* but struct fdmi_communiocator
int fdmi_verbs_isend(const void *buf, int count, struct fdmi_datatype datatype, int vdest, int tag, struct fdmi_communicator *comm, struct fdmi_request *request) 
{
  struct fdmi_post_send_param psparam;
  int dest;
  int errno;

#ifdef DEBUG_PERF_1
  fs = fdmi_get_time();
#endif


  //  dest = fdmi_comm_get_prank(comm, vdest);
  dest = vdest;

  if (!fdmi_is_connected(dest)) {
    fdmi_connect(sendrecv_channel, dest);
  }

  //  psparam.rank = fdmi_rmap_vtop_get(dest);
  psparam.rank	      = dest;
  psparam.tag	      = tag;
  psparam.addr	      = buf;
  psparam.count	      = count;
  psparam.dtype	      = datatype;
  psparam.comm	      = comm;
  psparam.request_flag = &(request->request_flag);
  psparam.request     = request;

  fdmi_queue_lock_enq(send_req_q, psparam.request_flag);

  *(psparam.request_flag) = 0;
  fdmi_post_send_msg_param(&psparam);

  return FMI_SUCCESS;
}


//TODO: NOT struct fdmi_communicator* but struct fdmi_communiocator
int fdmi_verbs_irecv(const void *buf, int count, struct fdmi_datatype datatype,  int vsource, int tag, struct fdmi_communicator *comm, struct fdmi_request *request) 
{
  struct fdmi_post_recv_param prparam;
  int source;


  source = fdmi_comm_get_prank(comm, vsource);

  if (!fdmi_is_connected(source)) {
    fdmi_connect(sendrecv_channel, source);
  }

  prparam.rank	      = source;
  prparam.tag	      = tag;
  prparam.addr	      = buf;
  prparam.count	      = count;
  prparam.dtype	      = datatype;
  prparam.comm	      = comm;
  prparam.request_flag = &(request->request_flag);
  prparam.request     = request; 

  //  fdmi_dbg("irecv match: vsource %d source %d tag %d", vsource, prparam.rank, prparam.tag);

  *(prparam.request_flag) = 0;
  fdmi_post_recv_msg_param (&prparam);

  return FMI_SUCCESS;
}

int fdmi_verbs_iprobe(int source, int tag, struct fdmi_communicator* comm, int *flag, struct fdmi_status *status)
{
  struct fdmi_queue *aq;
  struct fdmi_query *tmp_query = NULL;
  struct fdmi_msg *msg;
  /*TODOB:
   *  We assume the cluster has just one HCA for now.
   */

  fdmi_wait_ready();
  //  fdmi_dbg("query_qp:%p", sendrecv_channel->domains[0]->dctx->query_qp);
  aq = sendrecv_channel->domains[0]->dctx->query_qp->aq;
  /*Initialize flag with 0, assuming no matched msg in the aq (active side queue)*/

  *flag = 0;
  fdmi_queue_lock(aq);
  for (tmp_query = (struct fdmi_query*)fdmi_queue_iterate(aq, FDMI_QUEUE_HEAD); tmp_query != NULL;
       tmp_query = (struct fdmi_query*)fdmi_queue_iterate(aq, FDMI_QUEUE_NEXT)) {
    msg = tmp_query->msg;

    /* Epoch matching*/
    if (msg->epoch < ft_msg.epoch) continue;
    /* Communicator matching*/
    if (!(comm->id == msg->comm_id)) continue;
    /* Rank matching */
    if (!(source == FMI_ANY_SOURCE || source == msg->rank)) continue;
    /* Tag matching */
    if (!(tag == FMI_ANY_TAG || tag == msg->tag)) continue;
    *flag = 1;
    if (status != NULL)  {
      status->FMI_SOURCE = msg->rank;
      status->FMI_TAG    = msg->tag;
      //      fdmi_dbg("temp_query: %p source:%d tag: %d", tmp_query, status->FMI_SOURCE, status->FMI_TAG);
    }
    break;
  }
  fdmi_queue_unlock(aq);
  //  status = NULL;
  return 0;
}

static int fdmi_set_envs ()
{
  char* value = NULL;
  if ((value = fdmi_param_get("FDRANK")) != NULL) {
    prank = atoi(value);
  }

  if ((value = fdmi_param_get("FDMI_SIZE")) != NULL) {
    fdmi_size = atoi(value);
  } else {
    fdmi_size = 1024 * 1024; //TODO:
  }

  if ((value = fdmi_param_get("FDMI_NUMPROC")) != NULL) {
    fdmi_numproc = atoi(value);
  }

  if ((value = fdmi_param_get("FDMI_NUMSPARE")) != NULL) {
    fdmi_numspare = atoi(value);
  }

  locked_request_sem_q = fdmi_queue_create();
  reg_mr_q	       = fdmi_queue_create();
  cached_query_q       = fdmi_queue_create();
  free_query_q	       = fdmi_queue_create();
  send_req_q	       = fdmi_queue_create();
  failed_prank_q       = fdmi_queue_create();
  disconnected_prank_q = fdmi_queue_create();

  return 0;
}

/**
 *
 * Wait for the rdma_cm event specified.
 * If another event comes in return an error.
 *
 **
 */
static int fdmi_wait_for_event(struct rdma_event_channel *channel, enum rdma_cm_event_type requested_event)
{
  struct rdma_cm_event *event;
  int                   rc = 0;
  int                   rv = -1;

#if defined(DEBUG_A_INIT)
  //  fdmi_dbg("Waiting A_Event");
#endif
  if ((rc = rdma_get_cm_event(channel, &event))) {
    fdmi_err("Get cm event failed");
  }
#if defined(DEBUG_A_INIT)
  fdmi_dbg("A:A_Event=\"%s\", event->id: %p, event->listen_id: %p ", event_type_str(event->event), event->id, event->listen_id);
#endif
  fdmi_dbg("A:A_Event=\"%s\", event->id: %p, event->listen_id: %p ", event_type_str(event->event), event->id, event->listen_id);
  if (event->event == requested_event) {
    rv = 0;
  } else {
    fdmi_err("Recived event: %s", event_type_str(event->event));
  }
  rdma_ack_cm_event(event);
  return (rv);
}


static void fdmi_build_params(struct rdma_conn_param *params)
{
  memset(params, 0, sizeof(struct rdma_conn_param));
  params->private_data = &prank;
  params->private_data_len = sizeof(int);
  params->initiator_depth = 10;
  params->responder_resources = 10;
  params->rnr_retry_count = 7; /*7= infinite retry */
  params->retry_count = 7;
}

static void fdmi_dereg_mr(struct ibv_mr *mr) 
{
  /* ibv_dereg_mr(mr); */
  /* fdmi_queue_lock(reg_mr_q); */
  /* fdmi_queue_remove(reg_mr_q, mr); */
  /* reg_mr_cached_size -= mr->length; */
  /* fdmi_queue_unlock(reg_mr_q); */
  /* return; */

  int retry = 100;
  if (reg_mr_cached_size > FDMI_REG_MR_CACHE_SIZE) {
    fdmi_err("exceeded");
    while (ibv_dereg_mr(mr) != 0) {
      retry--;
      if (retry < 0) {
	fdmi_err ("ibv_dereg_rm failed even with %d trials (%s:%s:%d)", retry,  __FILE__, __func__, __LINE__);
      }
    }
    /*TODO: make sure reg_mr_cached_size <= FDMI_REG_MR_CACHE_SIZE*/
  }
  return;
}


struct ibv_mr* fdmi_reg_mr (const void* addr, uint32_t size, struct ibv_pd* pd) 
{
  struct ibv_mr *mr;
  struct ibv_mr *tmp_mr;
  /*TODO: Detect duplicated registrations and skip the region to be registered twice.*/    
  fdmi_queue_lock(reg_mr_q);
  for (tmp_mr = (struct ibv_mr*)fdmi_queue_iterate(reg_mr_q, FDMI_QUEUE_HEAD); tmp_mr != NULL;
       tmp_mr = (struct ibv_mr*)fdmi_queue_iterate(reg_mr_q, FDMI_QUEUE_NEXT)) {
    /*TODO: multiple HCA support: also check if tmp_mr came from a same pd(protection domain)*/
    if (tmp_mr->pd != pd) continue;
    //    if (tmp_mr->addr <= addr && addr + size <= tmp_mr->addr + tmp_mr->length) {
    if (tmp_mr->addr == addr &&  addr + size <= tmp_mr->addr + tmp_mr->length) {
      //   fdmi_dbg("rem_mr hit: %p: %p <= %p && %p <= %p", tmp_mr, tmp_mr->addr, addr, addr + size, tmp_mr->addr + tmp_mr->length);
      fdmi_queue_unlock(reg_mr_q);
      return tmp_mr;
    }
  }
  fdmi_queue_unlock(reg_mr_q);
  //  if (size == 65680) fdmi_dbgi(0, "miss: %p %d %p", addr, size, pd);


  int try = 1000;

  do {
    mr = ibv_reg_mr(
		    pd,
		    (void*)addr,
		    size,
		    IBV_ACCESS_LOCAL_WRITE
		    | IBV_ACCESS_REMOTE_READ);

    if (mr == NULL) {
      try--;
      if (try < 0) {
	//*(int*)addr = 1;
	fdmi_err ("ibv_reg_mr failed after several trial: pd:%p, addr:%p, size:%lu error:%s (%s:%s:%d)", pd, addr, size, strerror(errno),  __FILE__, __func__, __LINE__);
      }
    }
  } while(mr == NULL);

  fdmi_queue_lock(reg_mr_q);
  fdmi_queue_enq(reg_mr_q, mr);
  reg_mr_cached_size += mr->length;
  fdmi_queue_unlock(reg_mr_q);
  //  fdmi_dbg("cache: %p %d %p", addr, size, pd);
  return mr;
}

int fdmi_get_state(void)
{
  return fdmi_state;
}

void fdmi_set_state(enum fdmi_state state)
{
  fdmi_state = state;
}

int fdmi_inform_loop (int prank, int loop)
{
  struct fdmi_send_channel *send_channel;
  struct fdmi_peer *peer;
  struct fdmi_connection *conn;
  struct fdmi_query *query;
  struct fdmi_msg *msg;
  struct ibv_mr *rdma_mr;

  /* TODO: Access to peer through Hashtable, binary tree */
  peer = *(sendrecv_channel->peers + prank);
  /* =======================
   * TODO: Multiple HCA support
   *      For now, use first fdmi_connection to use a single HCA
   */
  conn = *(peer->conn);
  /* ========================== */
  query = fdmi_create_query(conn);
  query->request_flag = NULL;
  query->msg->opcode = FDMI_LOOP;
  //  query->msg->rank = prank;
  //  query->msg->order = 0;  /* TODO: We need order ? */
  //  query->msg->tag   = psparam->tag;
  query->msg->loop  = loop;

#ifdef DEBUG_SEND
  fdmi_dbg("C: Reg_mr   : rdma.addr=%p, rdma.len=%lu, rmda.rkey=%p", 
	   rdma_mr->addr, rdma_mr->length, rdma_mr->rkey);
#endif


#ifdef DEBUG_TEST
  fdmi_test(query, __FILE__, __func__, __LINE__);
#endif
  fdmi_post_send_msg(query);
}


int fdmi_verbs_get_failure_status(struct fdmi_failure_status *fstatus)
{
  int *failed_prank;
  int count = 0;


  if (fdmi_state == FDMI_COMPUTE) {
    /*Do nothing*/
  } else if (fdmi_state == FDMI_FAILURE) {
    //    fdmi_dbg("is_flushed wait");
    while(fdmi_state == FDMI_FAILURE) {
      sleep(1);
      //      fdmi_dbgi(5, "is_flushed:%d", is_flushed);      
    };
    
    //    fdmi_dbg("is_flushed fin");
    fdmi_queue_lock(failed_prank_q);
    /* for (failed_prank = (int*)fdmi_queue_iterate(failed_prank_q, FDMI_QUEUE_HEAD), count = 0; */
    /*      failed_prank != NULL; */
    /*      failed_prank = (int*)fdmi_queue_iterate(failed_prank_q, FDMI_QUEUE_NEXT), count++) { */
    while ((failed_prank = (int*)fdmi_queue_deq(failed_prank_q)) != NULL) {
      fstatus->pranks[count] = *failed_prank;
      fdmi_free(failed_prank);
      count++;
    }
    fdmi_queue_unlock(failed_prank_q);
  }
  fstatus->length = count;
  return 1;
}

int fdmi_verbs_accept_failure_status(struct fdmi_failure_status *fstatus) {
  int *failed_prank;
  int count;
  fdmi_queue_lock(failed_prank_q);
  for (failed_prank = (int*)fdmi_queue_iterate(failed_prank_q, FDMI_QUEUE_HEAD), count = 0;
       failed_prank != NULL;
       failed_prank = (int*)fdmi_queue_iterate(failed_prank_q, FDMI_QUEUE_NEXT), count++) {
    /*TODOB: make sure prank order in fstatus->pranks[count] and failed_prank are same */
    if(fstatus->pranks[count] == *failed_prank) {
      fdmi_queue_remove(failed_prank_q, failed_prank);
    }
  }
  fdmi_queue_unlock(failed_prank_q);
  return 1;
}

/*TODO: move follwoings to fdmi_err.c*/
const char *ibv_wc_opcode_str(enum ibv_wc_opcode opcode)
{
  switch (opcode) 
    {
    case IBV_WC_SEND: return("IBV_WC_SEND");
    case IBV_WC_RDMA_WRITE: return("IBV_WC_RDMA_WRITE");
    case IBV_WC_RDMA_READ: return("IBV_WC_RDMA_READ");
    case IBV_WC_COMP_SWAP: return("IBV_WC_COMP_SWAP");
    case IBV_WC_FETCH_ADD: return("IBV_WC_FETCH_ADD");
    case IBV_WC_BIND_MW: return("IBV_WC_BIND_MW");
    case IBV_WC_RECV: return("IBV_WC_RECV");
    case IBV_WC_RECV_RDMA_WITH_IMM: return("IBV_WC_RECV_RDMA_WITH_IMM");
    default: return ("Unknown");
    }
  return ("Unknown");
}

const char *rdma_err_status_str(enum ibv_wc_status status)
{ 
  switch (status)
    {
    case IBV_WC_SUCCESS: return ("IBV_WC_SUCCESS");
    case IBV_WC_LOC_LEN_ERR: return("IBV_WC_LOC_LEN_ERR");
    case IBV_WC_LOC_QP_OP_ERR: return("IBV_WC_LOC_QP_OP_ERR");
    case IBV_WC_LOC_EEC_OP_ERR: return("IBV_WC_LOC_EEC_OP_ERR"); 
    case IBV_WC_LOC_PROT_ERR: return("IBV_WC_LOC_PROT_ERR");
    case IBV_WC_WR_FLUSH_ERR: return("IBV_WC_WR_FLUSH_ERR");
    case IBV_WC_MW_BIND_ERR: return("IBV_WC_MW_BIND_ERR");
    case IBV_WC_BAD_RESP_ERR: return("IBV_WC_BAD_RESP_ERR");
    case IBV_WC_LOC_ACCESS_ERR: return("IBV_WC_LOC_ACCESS_ERR");
    case IBV_WC_REM_INV_REQ_ERR: return("IBV_WC_REM_INV_REQ_ERR");
    case IBV_WC_REM_ACCESS_ERR: return("IBV_WC_REM_ACCESS_ERR");
    case IBV_WC_REM_OP_ERR: return("IBV_WC_REM_OP_ERR");
    case IBV_WC_RETRY_EXC_ERR: return("IBV_WC_RETRY_EXC_ERR");
    case IBV_WC_RNR_RETRY_EXC_ERR: return("IBV_WC_RNR_RETRY_EXC_ERR");
    case IBV_WC_LOC_RDD_VIOL_ERR: return("IBV_WC_LOC_RDD_VIOL_ERR");
    case IBV_WC_REM_INV_RD_REQ_ERR: return("IBV_WC_REM_INV_RD_REQ_ERR");
    case IBV_WC_REM_ABORT_ERR: return("IBV_WC_REM_ABORT_ERR");
    case IBV_WC_INV_EECN_ERR: return("IBV_WC_INV_EECN_ERR");
    case IBV_WC_INV_EEC_STATE_ERR: return("IBV_WC_INV_EEC_STATE_ERR");
    case IBV_WC_FATAL_ERR: return("IBV_WC_FATAL_ERR");
    case IBV_WC_RESP_TIMEOUT_ERR: return("IBV_WC_RESP_TIMEOUT_ERR");
    case IBV_WC_GENERAL_ERR: return("IBV_WC_GENERAL_ERR");
    default: return ("Unknown");
    }
  return ("Unknown");
}


const char *event_type_str(enum rdma_cm_event_type event)
{ 
  switch (event)
    {
    case RDMA_CM_EVENT_ADDR_RESOLVED: return ("Addr resolved");
    case RDMA_CM_EVENT_ADDR_ERROR: return ("RDMA_CM_EVENT_ADDR_ERROR");
    case RDMA_CM_EVENT_ROUTE_RESOLVED: return ("Route resolved");
    case RDMA_CM_EVENT_ROUTE_ERROR: return ("Route Error");
    case RDMA_CM_EVENT_CONNECT_REQUEST: return ("RDMA_CM_EVENT_CONNECT_REQUEST");
    case RDMA_CM_EVENT_CONNECT_RESPONSE: return ("Connect response");
    case RDMA_CM_EVENT_CONNECT_ERROR: return ("Connect Error");
    case RDMA_CM_EVENT_UNREACHABLE: return ("Unreachable");
    case RDMA_CM_EVENT_REJECTED: return ("RDMA_CM_EVENT_REJECTED");
    case RDMA_CM_EVENT_ESTABLISHED: return ("Established");
    case RDMA_CM_EVENT_DISCONNECTED: return ("RDMA_CM_EVENT_DISCONNECTED");
    case RDMA_CM_EVENT_DEVICE_REMOVAL: return ("Device removal");
    case RDMA_CM_EVENT_MULTICAST_JOIN: return ("RDMA_CM_EVENT_MULTICAST_JOIN");
    case RDMA_CM_EVENT_MULTICAST_ERROR: return ("RDMA_CM_EVENT_MULTICAST_ERROR");
    case RDMA_CM_EVENT_ADDR_CHANGE: return ("RDMA_CM_EVENT_ADDR_CHANGE");
    case RDMA_CM_EVENT_TIMEWAIT_EXIT: return ("RDMA_CM_EVENT_TIMEWAIT_EXIT");
    default: return ("Unknown");
    }
  return ("Unknown");
}
