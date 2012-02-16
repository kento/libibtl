#include <unistd.h>
#include <sys/mman.h>

#include "common.h"
#include "rdma_common.h"
//#include "hashtable.h"

const int TIMEOUT_IN_MS = 500; /* ms */
struct ibv_cq *cq = NULL;
static struct context *s_ctx = NULL;

struct ibv_mr **rdma_msg_mr;

/*For active side*/
static uint32_t mr_number = 0;

/*Just for passive side valiables*/
int accepted = 0;
int connections = 0;
//struct ibv_mr *peer_mr;
//struct hashtable ht;
static uint32_t allocated_mr_size = 0;

/*For registerd memory size count*/
static uint64_t regmem_sum = 0;
pthread_mutex_t regmem_sum_mutex = PTHREAD_MUTEX_INITIALIZER;


static int post_send_ctl_msg(struct connection *conn, enum ctl_msg_type cmt, struct rdma_read_request_entry *rrre);

static int wait_for_event(struct rdma_event_channel *channel, enum rdma_cm_event_type requested_event);
static void build_connection(struct rdma_cm_id *id);

//static struct connection* create_connection(struct connection* conn_old);

static void build_context(struct ibv_context *verbs);
static void build_params(struct rdma_conn_param *params);
static void build_qp_attr(struct ibv_qp_init_attr *qp_attr);
static void register_memory(struct connection *conn);


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
  struct sockaddr_in * local_addr;

  comm = (struct RDMA_communicator *) arg;

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port   = htons(RDMA_PORT);
  //inet_aton("10.1.6.177", &(addr.sin_addr.s_addr));

  if (!(comm->ec = rdma_create_event_channel())) {
    fprintf(stderr, "RDMA lib: ERROR: RDMA event channel creation failed @ %s:%d\n", __FILE__, __LINE__);
    exit(1);
  }

  if (rdma_create_id(comm->ec, &(comm->cm_id), NULL, RDMA_PS_TCP)) {
    fprintf(stderr, "RDMA lib: ERROR: RDMA ID creation failed @ %s:%d\n", __FILE__, __LINE__);
    exit(1);
  }

  if (rdma_bind_addr(comm->cm_id, (struct sockaddr *)&addr)) {
    fprintf(stderr, "RDMA lib: ERROR: RDMA address binding failede: Invalid binding address or port !! @ %s:%d\n", __FILE__, __LINE__);
    exit(1);
  }

  /**/


  /* TODO: Determine appropriate backlog value */
  /*       backlog=10 is arbitrary */
  if (rdma_listen(comm->cm_id, 10)) {
    fprintf(stderr, "RDMA lib: ERROR: RDMA address binding failede @ %s:%d\n", __FILE__, __LINE__);
    exit(1);
  };

  port = ntohs(rdma_get_src_port(comm->cm_id));
  //  local_addr = (struct sockaddr_in*)rdma_get_local_addr(comm->cm_id);
  //  debug(printf("listening on port %s:%d ...\n", inet_ntoa(local_addr->sin_addr), port), 2);
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

  build_context(id->verbs);/*build_context() runs only once internally*/
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
  
  conn->send_msg = malloc(sizeof(struct rdma_read_request_entry));
  conn->recv_msg = malloc(sizeof(struct rdma_read_request_entry));

  conn->send_mr = reg_mr(conn->send_msg, sizeof(struct rdma_read_request_entry));
  conn->recv_mr = reg_mr(conn->recv_msg, sizeof(struct rdma_read_request_entry));

  conn->active_rrre = NULL;
  conn->passive_rrre = NULL;
  return conn;
}

int rdma_wait(struct RDMA_request *request)
{
  sem_wait(&(request->is_rdma_completed));
  return 1;
}

int free_connection(struct connection* conn) 
{
  if (conn->active_rrre != NULL) {free(conn->active_rrre);}
  if (conn->passive_rrre != NULL) {free(conn->passive_rrre);}


  dereg_mr(conn->send_mr);
  dereg_mr(conn->recv_mr);

  free(conn->send_msg);
  free(conn->recv_msg);

  free(conn);

  return 0;
}

void dereg_mr(struct ibv_mr *mr) 
{
  int retry = 100;
  uint64_t size = mr->length;
  void* addr = mr->addr;

  while (ibv_dereg_mr(mr) != 0) {
    fprintf(stderr, "RDMA lib: COMM: FAILED: memory region dereg again: retry = %d @ %s:%d\n", retry, __FILE__, __LINE__);
    exit(1);
    if (retry < 0) {
      fprintf(stderr, "RDMA lib: COMM: ERROR: memory region deregistration failed @ %s:%d\n",  __FILE__, __LINE__);
      exit(1);
    }
    retry--;
  }
  pthread_mutex_lock(&regmem_sum_mutex);
  regmem_sum = regmem_sum - size/1000000;
  //  printf(" ---%lu\n", regmem_sum);
  pthread_mutex_unlock(&regmem_sum_mutex);
  debug(fprintf(stderr, "RDMA lib: COMM: Dereg: addr=%p, length=%lu\n", addr, size), 2);
  return;
}


struct ibv_mr* reg_mr (void* addr, uint32_t size) 
{
  struct ibv_mr *mr;
  int try = 1000;
  /*TODO: Detect duplicated registrations and skip the region to be registered twice.*/
  do {
    mr = ibv_reg_mr(
		    s_ctx->pd,
		    addr,
		    size,
		    IBV_ACCESS_LOCAL_WRITE
		    | IBV_ACCESS_REMOTE_READ);
    try--;
    if (try < 0) {
      fprintf(stderr, "RDMA lib: COMM: ERROR: Memory region registration failed (regmem_sum= %lu[MB])@ %s:%d\n",  regmem_sum, __FILE__, __LINE__);
      exit(1);
    }
  } while(mr == NULL);
  debug(fprintf(stderr, "RDMA lib: COMM: Reg: addr=%p, length=%lu, lkey=%p, rkey=%p\n", mr->addr, mr->length, mr->lkey, mr->rkey), 2);
  pthread_mutex_lock(&regmem_sum_mutex);
  regmem_sum = regmem_sum +  size/1000000;

  pthread_mutex_unlock(&regmem_sum_mutex);
  return mr;
}


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
	//c->cmt = c->recv_msg->cmt;
	*conn = c;

	debug(printf("RDMA lib: COMM: Recv REQ(%s): id=%lu(%lu), wc.slid=%u |%f\n",  ibv_wc_opcode_str(c->opcode), c->count, c->id, wc.slid, get_dtime()), 1);
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
int send_ctl_msg (struct connection* conn, enum ctl_msg_type cmt, struct rdma_read_request_entry *entry)
{ 
  post_send_ctl_msg(conn, cmt, entry);
  post_recv_ctl_msg(conn);
  //TODO: free_connection(conn);
  //free_connection(conn);
  //  debug(printf("RDMA lib: COMM: Post %s\n",  rdma_ctl_msg_type_str(cmt)), 2);
  return 0;
}

static int post_send_ctl_msg(struct connection *conn, enum ctl_msg_type cmt, struct rdma_read_request_entry *entry)
{ 
  static uint64_t post_count = 0;
  struct ibv_send_wr wrs, *bad_wrs = NULL;
  struct ibv_sge sges;

  //  conn->send_msg->cmt = cmt;

  switch (cmt) {
  case MR_INIT:
    memcpy(conn->send_msg, entry, sizeof(struct rdma_read_request_entry));
    break;
  case MR_INIT_ACK:
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
  sges.length = (uint32_t)conn->send_mr->length;//sizeof(struct rdma_read_request_entry);
  sges.lkey = (uint32_t)conn->send_mr->lkey;
  //  fprintf(stderr, "%p, %p\n", conn_old->id->qp, conn->id->qp);
  TEST_NZ(ibv_post_send(conn->qp, &wrs, &bad_wrs));
  debug(printf("RDMA lib: COMM: Post %s: id=%lu,  post_count=%lu, imm_data=%d, local_addr=%p, length=%u, lkey=%p\n", rdma_ctl_msg_type_str(cmt), (uintptr_t)conn, conn->count, wrs.imm_data, sges.addr, sges.length, sges.lkey), 2);
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
  sger.length = sizeof(struct rdma_read_request_entry);
  sger.lkey = conn->recv_mr->lkey;

  TEST_NZ(ibv_post_recv(conn->qp, &wrr, &bad_wrr));
  debug(printf("RDMA lib: COMM: Post Recv: id=%lu,  post_count=%lu, local_addr=%p, length=%u, lkey=%p\n", (uintptr_t)conn, conn->count,  sger.addr, sger.length, sger.lkey), 2);

  return 0;
}






