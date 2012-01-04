#include <unistd.h>
#include <sys/mman.h>

#include "common.h"
#include "rdma_common.h"
#include "hashtable.h"



static int post_send_ctl_msg(struct connection *conn, enum ctl_msg_type cmt, uint64_t data);

const int TIMEOUT_IN_MS = 500; /* ms */
struct ibv_cq *cq = NULL;
static struct context *s_ctx = NULL;
//struct connection *conn;

struct ibv_mr **rdma_msg_mr;

/*For active side*/
static uint32_t mr_number = 0;

/*Just for passive side valiables*/
int accepted = 0;
int connections = 0;
//struct ibv_mr *peer_mr;
struct hashtable ht;
static uint32_t allocated_mr_size = 0;




static int wait_for_event(struct rdma_event_channel *channel, enum rdma_cm_event_type requested_event);
static void build_connection(struct rdma_cm_id *id);

//static struct connection* create_connection(struct connection* conn_old);

static void build_context(struct ibv_context *verbs);
static void build_params(struct rdma_conn_param *params);
static void build_qp_attr(struct ibv_qp_init_attr *qp_attr);
static void register_memory(struct connection *conn);
static void dereg_mr(struct ibv_mr *mr);
static struct ibv_mr* reg_mr(void* addr, uint32_t size);

/*Just for a pasive side*/
static void accept_connection(struct rdma_cm_id *id);
static int wait_msg_sent(void);

void die(const char *reason)
{
  fprintf(stderr, "%s\n", reason);
  exit(EXIT_FAILURE);
}

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


const char *rdma_ctl_msg_type_str(enum ctl_msg_type cmt)
{ 
  switch (cmt)
    {
    case MR_INIT: return("MR_INIT");
    case MR_INIT_ACK: return("MR_INIT_ACK");
    case MR_CHUNK: return("MR_CHUNK");
    case MR_CHUNK_ACK: return("MR_CHUNK_ACK");
    case MR_FIN: return("MR_FIN");
    case MR_FIN_ACK: return("MR_FIN_ACK");
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
    case RDMA_CM_EVENT_ADDR_ERROR: return ("Addr Error");
    case RDMA_CM_EVENT_ROUTE_RESOLVED: return ("Route resolved");
    case RDMA_CM_EVENT_ROUTE_ERROR: return ("Route Error");
    case RDMA_CM_EVENT_CONNECT_REQUEST: return ("Connect request");
    case RDMA_CM_EVENT_CONNECT_RESPONSE: return ("Connect response");
    case RDMA_CM_EVENT_CONNECT_ERROR: return ("Connect Error");
    case RDMA_CM_EVENT_UNREACHABLE: return ("Unreachable");
    case RDMA_CM_EVENT_REJECTED: return ("Rejected");
    case RDMA_CM_EVENT_ESTABLISHED: return ("Established");
    case RDMA_CM_EVENT_DISCONNECTED: return ("Disconnected");
    case RDMA_CM_EVENT_DEVICE_REMOVAL: return ("Device removal");
    default: return ("Unknown");
    }
  return ("Unknown");
}

void* rdma_passive_init(void * arg /*(struct RDMA_communicator *comm)*/)
{
  struct RDMA_communicator *comm;
  struct sockaddr_in addr;
  struct rdma_cm_event *event = NULL;
  //  struct rdma_cm_id *listener = NULL;                                                                                                                             
  //  struct rdma_event_channel *ec = NULL;                                                                                                                           
  uint16_t port = 0;

  comm = (struct RDMA_communicator *) arg;

 

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port   = htons(RDMA_PORT);

  if (!(comm->ec = rdma_create_event_channel())) {
    fprintf(stderr, "RDMA lib: ERROR: RDMA event channel creation failed @ %s:%d\n", __FILE__, __LINE__);
    exit(1);
  }

  if (rdma_create_id(comm->ec, &(comm->cm_id), NULL, RDMA_PS_TCP)) {
    fprintf(stderr, "RDMA lib: ERROR: RDMA ID creation failed @ %s:%d\n", __FILE__, __LINE__);
    exit(1);
  }

  if (rdma_bind_addr(comm->cm_id, (struct sockaddr *)&addr)) {
    fprintf(stderr, "RDMA lib: ERROR: RDMA address binding failede: PORT number conflict !! @ %s:%d\n", __FILE__, __LINE__);
    exit(1);
  }

  /* TODO: Determine appropriate backlog value */
  /*       backlog=10 is arbitrary */
  if (rdma_listen(comm->cm_id, 10)) {
    fprintf(stderr, "RDMA lib: ERROR: RDMA address binding failede @ %s:%d\n", __FILE__, __LINE__);
    exit(1);
  };

  port = ntohs(rdma_get_src_port(comm->cm_id));
  debug(printf("listening on port %d ...\n", port), 1);

  while (1) {
    int rc =0;
    debug(printf("Waiting for cm_event... "),1);
    if ((rc = rdma_get_cm_event(comm->ec, &event))){
      debug(printf("get event failed : %d\n", rc), 1);
      break;
    }
    debug(printf("\"%s\"\n", event_type_str(event->event)), 1);
    switch (event->event){
    case RDMA_CM_EVENT_CONNECT_REQUEST:
      accept_connection(event->id);
      //accept_connection(comm->cm_id);
      comm->cm_id = event->id;
      accepted = 1;
      break;
    case RDMA_CM_EVENT_ESTABLISHED:
      debug(printf("Establish: host_id=%lu\n", (uintptr_t)event->id), 2);
      write_log("Establish\n");
      break;
    case RDMA_CM_EVENT_DISCONNECTED:
      debug(printf("Disconnect from id : %p \n", event->id), 1);
      break;
    default:
      break;
    }
    rdma_ack_cm_event(event);
  }
  return 0;
}

static void accept_connection(struct rdma_cm_id *id)
{
  struct rdma_conn_param   conn_param;
  debug(printf("Accepting connection on id == %p (total connections %d)\n", id, ++connections), 1);
  build_connection(id);
  build_params(&conn_param);
  TEST_NZ(rdma_accept(id, &conn_param));
  post_recv_ctl_msg(id->context);
  return;
}


int wait_accept()
{
  while (accepted == 0) {
    usleep(1000);
  }
  return 0;
}


int rdma_active_init(struct RDMA_communicator *comm, struct RDMA_param *param, uint32_t mr_num)
{
  struct addrinfo *addr;
  //  struct rdma_cm_id *cm_id= NULL;
 //  struct rdma_event_channel *ec = NULL;
  struct rdma_conn_param cm_params;
  char port[8];
  int i;//,j;                                                                                                                                                        

  sprintf(port, "%d", RDMA_PORT);

  if(getaddrinfo(param->host, port, NULL, &addr)){
    fprintf(stderr, "RDMA lib: SEND: ERROR: getaddrinfo failed @ %s:%d\n", __FILE__, __LINE__);
    exit(1);
  }

  if(!(comm->ec = rdma_create_event_channel())){
    fprintf(stderr, "RDMA lib: SEND: ERROR: rdma event channel create failed @ %s:%d\n", __FILE__, __LINE__);
    exit(1);
  }

  if (rdma_create_id(comm->ec, &(comm->cm_id), NULL, RDMA_PS_TCP)){
    fprintf(stderr, "RDMA lib: SEND: ERROR: rdma id create failed @ %s:%d\n", __FILE__, __LINE__);
    exit(1);
  }
  if (rdma_resolve_addr(comm->cm_id, NULL, addr->ai_addr, TIMEOUT_IN_MS)) {
    fprintf(stderr, "RDMA lib: SEND: ERROR: rdma address resolve failed @ %s:%d\n", __FILE__, __LINE__);
    exit(1);
  }

  if (wait_for_event(comm->ec, RDMA_CM_EVENT_ADDR_RESOLVED)) {
    fprintf(stderr, "RDMA lib: SEND: ERROR: event wait failed: RDMA_CMEVENT_ADDR_RESOLVED: port = %s @ %s:%d\n", port, __FILE__, __LINE__);
    exit(1);
  }
  freeaddrinfo(addr);

  build_connection(comm->cm_id);

  if (rdma_resolve_route(comm->cm_id, TIMEOUT_IN_MS)) {
    fprintf(stderr, "RDMA lib: SEND: ERROR: rdma route resolve failed @ %s:%d\n", __FILE__, __LINE__);
    exit(1);
  }
  if (wait_for_event(comm->ec, RDMA_CM_EVENT_ROUTE_RESOLVED)) {
    fprintf(stderr, "RDMA lib: SEND: ERROR: event wait failed: port = %s @ %s:%d\n", port, __FILE__, __LINE__);
    exit(1);
  }

  build_params(&cm_params);

  if (rdma_connect(comm->cm_id, &cm_params)) {
    fprintf(stderr, "RDMA lib: SEND: ERROR: rdma connection failed @ %s:%d\n", __FILE__, __LINE__);
    exit(1);
  }

  if (wait_for_event(comm->ec, RDMA_CM_EVENT_ESTABLISHED)) {
    fprintf(stderr, "RDMA lib: SEND: ERROR: event wait failed @ %s:%d\n", __FILE__, __LINE__);
    exit(1);
  }

  rdma_msg_mr = (struct ibv_mr **) malloc(sizeof(struct ibv_mr*) * mr_num);
  mr_number = mr_num;
  for (i = 0; i < mr_num; i++){ 
    rdma_msg_mr[i] = NULL;
  }
  //  conn = comm->cm_id->context;

  return 0;
}

int rdma_active_finalize(struct RDMA_communicator *comm)
{
  TEST_NZ(wait_for_event(comm->ec, RDMA_CM_EVENT_DISCONNECTED));
  rdma_destroy_id(comm->cm_id);
  rdma_destroy_event_channel(comm->ec);
  return 0;
}




/*************************************************************************                                                                                             * Wait for the rdma_cm event specified.                                                                                                                               * If another event comes in return an error.                                                                                                                      
 */
static int wait_for_event(struct rdma_event_channel *channel, enum rdma_cm_event_type requested_event)
{
  struct rdma_cm_event *event;
  int                   rc = 0;
  int                   rv = -1;

  if ((rc = rdma_get_cm_event(channel, &event)))
    {
      debug(printf("get event failed : %d\n", rc), 1);
      return (rc);
    }
  debug(printf("got \"%s\" event\n", event_type_str(event->event)), 1);

  if (event->event == requested_event)
    rv = 0;
  rdma_ack_cm_event(event);
  return (rv);
}

static void build_context(struct ibv_context *verbs)
{
  if (s_ctx) {
    if (s_ctx->ctx != verbs) {
      die("cannot handle events in more than one context.");
    }
    return;
  }

  s_ctx = (struct context *)malloc(sizeof(struct context));
  s_ctx->ctx = verbs;

  TEST_Z(s_ctx->pd = ibv_alloc_pd(s_ctx->ctx));
  TEST_Z(s_ctx->comp_channel = ibv_create_comp_channel(s_ctx->ctx));
  //  TEST_Z(s_ctx->cq = ibv_create_cq(s_ctx->ctx, 100, NULL, s_ctx->comp_channel, 0)); /* cqe=10 is arbitrary/ comp_vector:0 */
  TEST_Z(s_ctx->cq = ibv_create_cq(s_ctx->ctx, 1000, NULL, s_ctx->comp_channel, 0)); /* cqe=10 is arbitrary up to 131071 (36 nodes =>200 cq) */
  TEST_NZ(ibv_req_notify_cq(s_ctx->cq, 0));



  //  TEST_NZ(pthread_create(&s_ctx->cq_poller_thread, NULL, poll_cq, NULL));
}

static void build_params(struct rdma_conn_param *params)
{
  memset(params, 0, sizeof(*params));

  params->initiator_depth = 10;
  params->responder_resources = 10;
  params->rnr_retry_count = 7; /*7= infinite retry */
  params->retry_count = 7;
}


static void build_qp_attr(struct ibv_qp_init_attr *qp_attr)
{
  memset(qp_attr, 0, sizeof(*qp_attr));

  qp_attr->send_cq = s_ctx->cq;
  qp_attr->recv_cq = s_ctx->cq;
  qp_attr->qp_type = IBV_QPT_RC;
  qp_attr->sq_sig_all = 0;

  qp_attr->cap.max_send_wr = 1000;// 10
  qp_attr->cap.max_recv_wr = 1000;//10
  qp_attr->cap.max_send_sge = 5;//1
  qp_attr->cap.max_recv_sge = 5;//1
}


static void build_connection(struct rdma_cm_id *id)
{
  struct connection *conn, *clone;
  struct ibv_qp_init_attr qp_attr;

  build_context(id->verbs);
  build_qp_attr(&qp_attr);

  TEST_NZ(rdma_create_qp(id, s_ctx->pd, &qp_attr));

  /*
  id->context = conn = (struct connection *)malloc(sizeof(struct connection));

  conn->id = id;
  conn->qp = id->qp;

  
  conn->connected = 0;

  register_memory(conn);
  */

  //TODO: create_connection in rdma_client.c and rdma_server.c
  id->context = conn = create_connection(id);
  return;
}


struct connection* create_connection(struct rdma_cm_id *id)
{
  static uint64_t count = 0;
  struct connection* conn = (struct connection *)malloc(sizeof(struct connection));

  conn->count = count++;
  
  conn->id = id;
  conn->qp = id->qp;
  
  conn->connected = 0;

  conn->send_msg = malloc(sizeof(struct control_msg));
  conn->recv_msg = malloc(sizeof(struct control_msg));

  conn->send_mr = reg_mr(conn->send_msg, sizeof(struct control_msg));
  conn->recv_mr = reg_mr(conn->recv_msg, sizeof(struct control_msg));

  conn->rdma_msg_mr=NULL;
  // struct ibv_mr peer_mr; 
  //  char *rdma_msg_region;
  return conn;
}

int free_connection(struct connection* conn) 
{
  free(conn->send_msg);
  free(conn->recv_msg);
  dereg_mr(conn->send_mr);
  dereg_mr(conn->recv_mr);
  if (conn->rdma_msg_mr != NULL) {
    dereg_mr(conn->rdma_msg_mr);
  }
  free(conn);
  return 0;
}

/*
static void register_memory(struct connection *conn)
{
  conn->send_msg = malloc(sizeof(struct control_msg));
  conn->recv_msg = malloc(sizeof(struct control_msg));

  TEST_Z(conn->send_mr = ibv_reg_mr(
                                    s_ctx->pd,
                                    conn->send_msg,
                                    sizeof(struct control_msg),
                                    IBV_ACCESS_LOCAL_WRITE));

  TEST_Z(conn->recv_mr = ibv_reg_mr(
                                    s_ctx->pd,
                                    conn->recv_msg,
                                    sizeof(struct control_msg),
                                    IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ));

  return;
}
*/

void register_rdma_msg_mr(struct connection* conn, void* addr, uint32_t size)
{
  struct ibv_mr *rmr;
  double ss,ee;
  ss = get_dtime();
  conn->rdma_msg_mr = reg_mr(addr, size);
  ee = get_dtime();
  rmr =  conn->rdma_msg_mr;
  debug(printf("RDMA lib: COMM: Rgst: id=%d, addr=%lu(%p), length=%lu, rkey=%u, lkey=%lu, time=%f\n", (uintptr_t)conn, rmr->addr, rmr->addr, rmr->length, rmr->rkey, rmr->lkey, ee - ss), 2);
  /*
  uint64_t s = 1024 * 1024;
  char* a;

  while (s < 1024 * 1024 * 1024) {
    a = malloc(s);
    ss = get_dtime();
    ibv_reg_mr(
	       s_ctx->pd,
	       a,
	       s,
	       IBV_ACCESS_LOCAL_WRITE
	       | IBV_ACCESS_REMOTE_READ);
    ee = get_dtime();
    fprintf(stderr,"Size: %lu Time: %f\n", s, ee-ss);
    free(a);
    s *= 2;
    }*/
  /*
  uint64_t s = 1024 * 1024;
  char* a;
  char* b;

  while (s < 1024 * 1024 * 1024) {
    a = (char*)malloc(s);
    //    b = (char*)malloc(s);
    ss = get_dtime();
    mlock(a,s);
    ee = get_dtime();
    fprintf(stderr,"Size: %lu Time: %f\n", s, ee-ss);
    free(a);
    s *= 2;
  }
  */




  return;
}

void deregister_rdma_msg_mr()
{


}

static void dereg_mr(struct ibv_mr *mr) 
{
  int retry = 100;
  while (ibv_dereg_mr(mr) != 0) {
    fprintf(stderr, "RDMA lib: COMM: FAILED: memory region dereg again: retry = %d @ %s:%d\n", retry, __FILE__, __LINE__);
    exit(1);
    if (retry < 0) {
      fprintf(stderr, "RDMA lib: COMM: ERROR: memory region deregistration failed @ %s:%d\n",  __FILE__, __LINE__);
      exit(1);
    }
    retry--;
  }
}


static struct ibv_mr* reg_mr (void* addr, uint32_t size) 
{


  struct ibv_mr *mr;
  mr = ibv_reg_mr(
		  s_ctx->pd,
		  addr,
		  size,
		  IBV_ACCESS_LOCAL_WRITE
		  | IBV_ACCESS_REMOTE_READ);
  if (mr == NULL) {
    fprintf(stderr, "RDMA lib: COMM: ERROR: Memory region registration failed @ %s:%d\n",  __FILE__, __LINE__);
    exit(1);
  }



  return mr;
}



/*Output: slid (soruce local id: 16 bytes)*/
/*Note: If cmt=MR_CHUNK, rdma_read_req has to be called before the next recv_ctl_msg call*/
/*
int init_ctl_msg (uint32_t **cmt, uint64_t **data)
{
  *cmt = (uint32_t *)malloc (sizeof(uint32_t));
  *data = (uint64_t *)malloc (sizeof(uint64_t));

  return 0;
}
*/

int init_rdma_buffs(uint32_t num_client) {
  create_ht(&ht, num_client);
  return 0;
}

int alloc_rdma_buffs(uint16_t id, uint64_t size)
{
  int64_t retry=0;
  struct RDMA_buff *rdma_buff = NULL;
  
  //  while ((rdma_buff = (struct RDMA_buff *) malloc(sizeof(struct RDMA_buff))) == NULL) {
  while ((rdma_buff = (struct RDMA_buff *) malloc(sizeof(struct RDMA_buff))) == NULL) {                                                                         
    fprintf(stderr, "RDMA lib: RECV: No more buffer space !!\n");                                                                                               
    usleep(10000);                                                                                                                                              
    retry++;
    exit(1);
  }

  //          alloc_size += sizeof(struct RDMA_buff);
  retry=0;
  while ((rdma_buff->buff = (char *)malloc(size)) == NULL) {
    fprintf(stderr, "RDMA lib: RECV: No more buffer space for size %lu bytes!!\n", size);
    usleep(10000);
    retry++;
    exit(1);
  }  
  double ss,ee;
  //  ss = get_dtime();
  //   memset(rdma_buff->buff, 0, size);

  //  ee = get_dtime();
  //  printf("====%f \n", ee - ss);
  //   exit(1);

  rdma_buff->buff_size = size;
  rdma_buff->recv_base_addr = rdma_buff->buff;
  rdma_buff->mr = NULL;
  //          rdma_buff->mr_size = 0;
  rdma_buff->recv_size = 0;
  add_ht(&ht, id, rdma_buff);
}

int rdma_read(struct connection* conn, uint16_t id, uint64_t offset, uint64_t mr_size, int signal)
{

  struct RDMA_buff *rdma_buff = NULL;
  struct ibv_wc wc;
  struct ibv_send_wr wr, *bad_wr;
  struct ibv_sge sge;

  rdma_buff = get_ht(&ht, id);
  debug(fprintf(stderr, "Registering RDMA MR: %lu\n", rdma_buff->mr_size), 1);




  double ss = get_dtime();
  conn->rdma_msg_mr = rdma_buff->mr = reg_mr(rdma_buff->recv_base_addr, mr_size);
  double ee = get_dtime();
  debug(printf("RDMA lib: RECV: Rgst RDMA_MR: local_addr=%lu(%p), lkey=%lu, size=%lu, time=%f\n", rdma_buff->recv_base_addr, rdma_buff->recv_base_addr, rdma_buff->mr->lkey, mr_size, ee - ss), 2);
  rdma_buff->mr_size = mr_size;
  allocated_mr_size += mr_size;
  debug(fprintf(stderr,"Preparing RDMA transfer\n"), 1);

  /* !! memset must be called !!*/
  memset(&wr, 0, sizeof(wr));
  wr.wr_id = (uintptr_t)conn;
  wr.opcode = IBV_WR_RDMA_READ;
  wr.sg_list = &sge;
  wr.num_sge = 1;
  if (signal == 1) {
    wr.send_flags = IBV_SEND_SIGNALED;
  }
  wr.wr.rdma.remote_addr = (uintptr_t)conn->peer_mr.addr + offset; // wr.wr.rdma.remote_addr => uint64_t
  wr.wr.rdma.rkey = conn->peer_mr.rkey;

  sge.addr = (uintptr_t)rdma_buff->recv_base_addr;
  sge.length = (uint32_t)mr_size;

  sge.lkey = (uint32_t)rdma_buff->mr->lkey;


  debug(printf("RDMA lib: Preparing RDMA transfer: Done\n"), 1);
  if ((ibv_post_send(conn->qp, &wr, &bad_wr))) {
    fprintf(stderr, "RDMA lib: ERROR: post send failed @ %s:%d\n", __FILE__, __LINE__);
    exit(1);
  }

  debug(printf("RDMA lib: RECV: Post RDMA_READ: id=%lu(%d), remote_addr=%lu, rkey=%u, sge.addr=%lu, sge.length=%u,  sge.lkey=%lu\n", conn->count, (uintptr_t)conn, wr.wr.rdma.remote_addr, wr.wr.rdma.rkey, sge.addr, sge.length, sge.lkey), 2);


  rdma_buff->recv_base_addr += (uintptr_t)mr_size;
  rdma_buff->recv_size += (uint64_t)mr_size;
  //          cmsg.type=MR_CHUNK_ACK;                                                                                                                           
  //          post_receives(conn);                                                                                                                              
  //          send_control_msg(conn, &cmsg); 
}

int get_rdma_buff(uint16_t id, char** addr, uint64_t *size)
{
  struct RDMA_buff *rdma_buff;


  rdma_buff = get_ht(&ht, id);

  *addr = rdma_buff->buff;
  *size = rdma_buff->buff_size;

  //  wait_msg_sent();

  /*
  usleep(1000);
  if (ibv_dereg_mr(rdma_buff->mr)) {
    fprintf(stderr, "RDMA lib: ERROR: memory region deregistration failed (allocated_mr_size: %d bytes) @ %s:%d\n", allocated_mr_size, __FILE__, __LINE__);
    exit(1);
  }
  */
  allocated_mr_size = allocated_mr_size - rdma_buff->mr_size;
  ///  free(rdma_buff); 
  return 0;
}

int finalize_ctl_msg (uint32_t *cmt, uint64_t *data) 
{
  free(cmt);
  free(data);
  return 0;
}


/*
int recv_ctl_msg(uint32_t *cmt, uint64_t *data, struct connection** conn_output)
{
  void* ctx;
  struct ibv_wc wc;
  uint16_t slid;
  struct connection* conn;


  while(1) {
    if (cq != NULL) {

      while (ibv_poll_cq(cq, 1, &wc)) {

	conn = (struct connection *)(uintptr_t)wc.wr_id;
	*conn_output = conn; 
	slid = wc.slid;
	//Check if the request was successed
	if (wc.status != IBV_WC_SUCCESS) {
	  const char* err_str = rdma_err_status_str(wc.status);
	  fprintf(stderr, "RDMA lib: COMM: ERROR: status is not IBV_WC_SUCCESS: Erro=%s(%d) @ %s:%d\n", err_str, wc.status, __FILE__, __LINE__);
	  exit(1);
	}

	//Check which request was successed
	if (wc.opcode == IBV_WC_RECV) {
	  *cmt = conn->recv_msg->cmt;

	  switch (conn->recv_msg->cmt) 
	    {
	    case MR_INIT:
	      *data = conn->recv_msg->data1.buff_size;
	      //	      memcpy(data, &conn->recv_msg->data1.buff_size, sizeof(uint64_t));
	      break;
	    case MR_INIT_ACK:
	      break;
	    case MR_CHUNK:
	      *data = conn->recv_msg->data1.mr_size;
	      //	      memcpy(data, &conn->recv_msg->data1.mr_size, sizeof(uint64_t));
	      memcpy(&conn->peer_mr, &conn->recv_msg->data.mr, sizeof(struct ibv_mr));
	      break;    
	    case MR_CHUNK_ACK:
	      break;
	    case MR_FIN:
	      *data = conn->recv_msg->data1.tag;
	      //	      memcpy(data, &conn->recv_msg->data1.tag, sizeof(uint64_t));
	      break;
	    case MR_FIN_ACK:
	      break;
	    default:
	      fprintf(stderr, "unknow msg type @ %s:%d", __FILE__, __LINE__);
	      exit(1);
	    }
	  debug(printf("RDMA lib: COMM: Recv %s: id=%lu, wc.slid=%u |%f\n", rdma_ctl_msg_type_str(*cmt), conn->id, slid, get_dtime()), 2);
	  return (int)slid;
	} else if (wc.opcode == IBV_WC_SEND) {
	  debug(printf("RDMA lib: COMM: Sent IBV_WC_SEND: id=%lu |%f\n", (uintptr_t)conn, get_dtime()), 2);
	  free_connection(conn);
	  continue;
	} else if (wc.opcode == IBV_WC_RDMA_READ) {
	  debug(printf("RDMA lib: COMM: Sent IBV_WC_RDMA_READ: id=%lu\n", (uintptr_t)conn), 2);
	  send_ctl_msg (conn, MR_CHUNK_ACK, 0);
	  //	  free_connection(conn);
	  continue;
	} else {
	  die("unknow opecode.");
	  continue;
	}
      }
    }

    if (ibv_get_cq_event(s_ctx->comp_channel, &cq, &ctx)) {
      fprintf(stderr, "RDMA lib: SEND: ERROR: get cq event  failed @ %s:%d\n", __FILE__, __LINE__);
      exit(1);
    }
    
    ibv_ack_cq_events(cq, 1);
    
    if (ibv_req_notify_cq(cq, 0)) {
      fprintf(stderr, "RDMA lib: SEND: ERROR: request notification failed @ %s:%d\n", __FILE__, __LINE__);
      exit(1);
    }
  }
  return 0;
}
*/

int recv_wc (int num_entries, struct connection** conn)
{
  void* ctx;
  struct ibv_wc wc;
  struct connection* c;
  int length;


  while(1) {
    if (cq != NULL) {

      while ((length = ibv_poll_cq(cq, num_entries, &wc))){
	/*Check if a request was successed*/
	if (wc.status != IBV_WC_SUCCESS) {
	  const char* err_str = rdma_err_status_str(wc.status);
	  fprintf(stderr, "RDMA lib: COMM: ERROR: status is not IBV_WC_SUCCESS: Erro=%s(%d) @ %s:%d\n", err_str, wc.status, __FILE__, __LINE__);
	  exit(1);
	}
	c = (struct connection *)(uintptr_t)wc.wr_id;
	c->slid = wc.slid;
	c->opcode = wc.opcode;
	c->cmt = c->recv_msg->cmt;
	*conn = c;

	debug(printf("RDMA lib: COMM: Recv %s(%s): id=%lu(%lu), wc.slid=%u |%f\n",  rdma_ctl_msg_type_str(c->cmt), ibv_wc_opcode_str(c->opcode), c->count, c->id, wc.slid, get_dtime()), 1);
	return length;
      }
    }

    if (ibv_get_cq_event(s_ctx->comp_channel, &cq, &ctx)) {
      fprintf(stderr, "RDMA lib: SEND: ERROR: get cq event  failed @ %s:%d\n", __FILE__, __LINE__);
      exit(1);
    }
    
    ibv_ack_cq_events(cq, 1);
    
    if (ibv_req_notify_cq(cq, 0)) {
      fprintf(stderr, "RDMA lib: SEND: ERROR: request notification failed @ %s:%d\n", __FILE__, __LINE__);
      exit(1);
    }
  }
  return 0;
}

/*Note: If cmt=MR_CHUNK, a register_rdma_msg_mr(...) function has to be called to register [mr_index]th memory region  */
int send_ctl_msg (struct connection* conn, enum ctl_msg_type cmt, uint64_t data)
{ 
  post_send_ctl_msg(conn, cmt, data) ;
  post_recv_ctl_msg(conn);
  //TODO: free_connection(conn);
  //free_connection(conn);
  //  debug(printf("RDMA lib: COMM: Post %s\n",  rdma_ctl_msg_type_str(cmt)), 2);
  return 0;
}

static int post_send_ctl_msg(struct connection *conn, enum ctl_msg_type cmt, uint64_t data)
{ 
  static uint64_t post_count = 0;
  struct ibv_send_wr wrs, *bad_wrs = NULL;
  struct ibv_sge sges;

  conn->send_msg->cmt = cmt;

  switch (cmt) {
  case MR_INIT:
    conn->send_msg->data1.buff_size = data;
    break;
  case MR_INIT_ACK:
    break;
  case MR_CHUNK:
    memcpy(&conn->send_msg->data.mr, conn->rdma_msg_mr, sizeof(struct ibv_mr));
    conn->send_msg->data1.mr_size = data;
    break;    
  case MR_CHUNK_ACK:
    break;
  case MR_FIN:
    conn->send_msg->data1.tag = data;
    break;
  case MR_FIN_ACK:
    break;
  default:
    fprintf(stderr, "unknow msg type @ %s:%d", __FILE__, __LINE__);
    exit(1);
  }
  memset(&wrs, 0, sizeof(wrs));

  wrs.wr_id = (uintptr_t)conn;
  wrs.opcode = IBV_WR_SEND;
  wrs.sg_list = &sges;
  wrs.num_sge = 1;
  wrs.send_flags = IBV_SEND_SIGNALED;
  //  wrs.send_flags = IBV_SEND_INLINE;
  wrs.imm_data = post_count++;
  sges.addr = (uintptr_t)conn->send_msg;
  sges.length = (uint32_t)sizeof(struct control_msg);
  sges.lkey = (uint32_t)conn->send_mr->lkey;
  //  fprintf(stderr, "%p, %p\n", conn_old->id->qp, conn->id->qp);
  TEST_NZ(ibv_post_send(conn->qp, &wrs, &bad_wrs));
  debug(printf("RDMA lib: COMM: Post %s: id=%lu(%d),  post_count=%lu, local_addr=%lu, length=%u, lkey=%lu\n", rdma_ctl_msg_type_str(cmt), conn->count, (uintptr_t)conn, wrs.imm_data, sges.addr, sges.length, sges.lkey), 2);
  return 0;
}

int post_recv_ctl_msg(struct connection *conn)
{ 
  struct ibv_recv_wr wrr, *bad_wrr = NULL;
  struct ibv_sge sger;

  wrr.wr_id = (uintptr_t)conn;
  wrr.next = NULL;
  wrr.sg_list = &sger;
  wrr.num_sge = 1;

  sger.addr = (uintptr_t)conn->recv_msg;
  sger.length = sizeof(struct control_msg);
  sger.lkey = conn->recv_mr->lkey;

  TEST_NZ(ibv_post_recv(conn->qp, &wrr, &bad_wrr));


  return 0;
}






