#include <unistd.h>

#include "common.h"
#include "rdma_common.h"
#include "hashtable.h"


static int post_send_ctl_msg(struct connection *conn, enum ctl_msg_type cmt, struct ibv_mr *mr, uint64_t data);

const int TIMEOUT_IN_MS = 500; /* ms */
struct ibv_cq *cq = NULL;
static struct context *s_ctx = NULL;
struct connection *conn;

struct ibv_mr **rdma_msg_mr;


/*Just for passive side valiables*/
int accepted = 0;
int connections = 0;
//struct ibv_mr *peer_mr;
struct hashtable ht;
static uint32_t allocated_mr_size = 0;


static int wait_for_event(struct rdma_event_channel *channel, enum rdma_cm_event_type requested_event);
static void build_connection(struct rdma_cm_id *id);
static struct connection* create_connection(struct rdma_cm_id *id);
static int free_connection(struct connection* conn);
static void build_context(struct ibv_context *verbs);
static void build_params(struct rdma_conn_param *params);
static void build_qp_attr(struct ibv_qp_init_attr *qp_attr);
static void register_memory(struct connection *conn);
static void dereg_mr(struct ibv_mr *mr);

/*Just for a pasive side*/
static void accept_connection(struct rdma_cm_id *id);
static int wait_msg_sent(void);

void die(const char *reason)
{
  debug(printf("%s\n", reason), 1);
  exit(EXIT_FAILURE);
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
  for (i = 0; i < mr_num; i++){ 
    rdma_msg_mr[i] = NULL;
  }
  conn = comm->cm_id->context;

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

  params->initiator_depth = 2;
  params->responder_resources = 2;
  params->rnr_retry_count = 7; /*7= infinite retry */
  params->retry_count = 7;
}


static void build_qp_attr(struct ibv_qp_init_attr *qp_attr)
{
  memset(qp_attr, 0, sizeof(*qp_attr));

  qp_attr->send_cq = s_ctx->cq;
  qp_attr->recv_cq = s_ctx->cq;
  qp_attr->qp_type = IBV_QPT_RC;

  qp_attr->cap.max_send_wr = 100;// 10                                                                                                                                 
  qp_attr->cap.max_recv_wr = 100;//10                                                                                                                                  
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

  id->context = conn = create_connection(id);
  return;
}

static struct connection* create_connection(struct rdma_cm_id *id) 
{
  struct connection* conn = (struct connection *)malloc(sizeof(struct connection));

  conn->id = id;
  conn->qp = id->qp;
  conn->connected = 0;

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

  conn->rdma_msg_mr=NULL;
  // struct ibv_mr peer_mr; 
  //  char *rdma_msg_region;
  return conn;
}

static int free_connection(struct connection* conn) 
{
  free(conn->send_msg);
  free(conn->recv_msg);

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

void register_rdma_msg_mr(int mr_index, void* addr, uint32_t size)
{
  if (rdma_msg_mr[mr_index] != NULL) {

    dereg_mr(rdma_msg_mr[mr_index]);
    debug(printf("RDMA lib: SEND: Derg RDMA_MR: mr_idx=%d\n", mr_index),2);
    /*
    int retry = 100;
    while (ibv_dereg_mr(rdma_msg_mr[mr_index]) != 0) {
      fprintf(stderr, "RDMA lib: SEND: FAILED: memory region dereg again: retry = %d @ %s:%d\n", retry, __FILE__, __LINE__);
      exit(1);
      if (retry < 0) {
        fprintf(stderr, "RDMA lib: SEND: ERROR: memory region deregistration failed @ %s:%d\n",  __FILE__, __LINE__);
        exit(1);
      }
      retry--;
      debug(printf("RDMA lib: SEND: Derg RDMA_MR: mr_idx=%d\n", mr_index),2);
    }
    */
  }
  
  TEST_Z(rdma_msg_mr[mr_index] = ibv_reg_mr(
					    s_ctx->pd,
					    addr,
					    size,
                                  IBV_ACCESS_LOCAL_WRITE
                                  | IBV_ACCESS_REMOTE_READ
					    | IBV_ACCESS_REMOTE_WRITE));
  debug(printf("RDMA lib: SEND: Rgst RDMA_MR: mr_idx=%d, remote_addr=%lu(%p), rkey=%u, size=%u\n", mr_index, addr, addr, rdma_msg_mr[mr_index]->rkey, size),2);
  return;
}

static void dereg_mr(struct ibv_mr *mr) 
{
  int retry = 100;
  while (ibv_dereg_mr(mr) != 0) {
    fprintf(stderr, "RDMA lib: SEND: FAILED: memory region dereg again: retry = %d @ %s:%d\n", retry, __FILE__, __LINE__);
    exit(1);
    if (retry < 0) {
      fprintf(stderr, "RDMA lib: SEND: ERROR: memory region deregistration failed @ %s:%d\n",  __FILE__, __LINE__);
      exit(1);
    }
    retry--;

  }
}


/*Output: slid (soruce local id: 16 bytes)*/
/*Note: If cmt=MR_CHUNK, rdma_read_req has to be called before the next recv_ctl_msg call*/
int init_ctl_msg (uint32_t **cmt, uint64_t **data)
{
  *cmt = (uint32_t *)malloc (sizeof(uint32_t));
  *data = (uint64_t *)malloc (sizeof(uint64_t));

  return 0;
}

int init_rdma_buffs(uint32_t num_client) {
  create_ht(&ht, num_client);
  return 0;
}

int alloc_rdma_buffs(uint16_t id, uint64_t size)
{
  int64_t retry=0;
  struct RDMA_buff *rdma_buff = NULL;

  while ((rdma_buff = (struct RDMA_buff *) malloc(sizeof(struct RDMA_buff))) == NULL) {                                                                         
    fprintf(stderr, "RDMA lib: RECV: No more buffer space !!\n");                                                                                               
    usleep(10000);                                                                                                                                              
    retry++;                                                                                                                                                    
    //          exit(1);                                                                                                                                        
  }                                                                                                                                                             
  //          alloc_size += sizeof(struct RDMA_buff);                                                                                                           
  retry=0;                                                                                                                                                      
  while ((rdma_buff->buff = (char *)malloc(size)) == NULL) {                                                                                               
    fprintf(stderr, "RDMA lib: RECV: No more buffer space !!\n");                                                                                               
    usleep(10000);                                                                                                                                              
    retry++;                                                                                                                                                    
    //          exit(1);                                                                                                                                        
  }                                                                                                                                                             

  rdma_buff->buff_size = size;                                                                                                                             
  rdma_buff->recv_base_addr = rdma_buff->buff;                                                                                                                  
  rdma_buff->mr = NULL;                                                                                                                                         
  //          rdma_buff->mr_size = 0;                                                                                                                           
  rdma_buff->recv_size = 0;                                                                                                                                     
  add_ht(&ht, id, rdma_buff);
}

int rdma_read(uint16_t id, uint64_t mr_size)
{

  struct RDMA_buff *rdma_buff = NULL;
  struct ibv_wc wc;
  struct ibv_send_wr wr, *bad_wr;
  struct ibv_sge sge;

  rdma_buff = get_ht(&ht, id);
  memcpy(&conn->peer_mr, &conn->recv_msg->data.mr, sizeof(conn->peer_mr));
  /*
  if (rdma_buff->mr != NULL) {
   debug(fprintf(stderr,"Deregistering RDMA MR: %lu\n", rdma_buff->mr_size), 1);
   int retry=1000;
   while (ibv_dereg_mr(rdma_buff->mr)) {
     fprintf(stderr, "RDMA lib: FAILED: memory region dereg again (allocated_mr_size: %d bytes): retry = %d @ %s:%d\n", allocated_mr_size, retry, __FILE__, __LINE__);
     if (retry < 0) {
       fprintf(stderr, "RDMA lib: ERROR: memory region deregistration failed (allocated_mr_size: %d bytes) @ %s:%d\n", allocated_mr_size, __FILE__, __LINE__);
       exit(1);
      }
      retry--;
    }
    allocated_mr_size = allocated_mr_size - rdma_buff->mr_size;
  } else {
    debug(fprintf(stderr,"Not deregistering RDMA MR: %lu\n", rdma_buff->mr_size), 1);
  }
  */
  debug(fprintf(stderr, "Registering RDMA MR: %lu\n", rdma_buff->mr_size), 1);
  if (!(rdma_buff->mr = ibv_reg_mr(
				   s_ctx->pd,
				   rdma_buff->recv_base_addr,
				   mr_size,
				   IBV_ACCESS_LOCAL_WRITE)
	)
      )                                                                                                        
    {
      fprintf(stderr, "RDMA lib: ERROR: memory region registration failed (allocated_mr_size: %d bytes) @ %s:%d\n", allocated_mr_size, __FILE__, __LINE__);
      exit(1);
    }
  debug(printf("RDMA lib: RECV: Rgst RDMA_MR: local_addr=%lu(%p), lkey=%lu, size=%lu\n", rdma_buff->recv_base_addr, rdma_buff->recv_base_addr, rdma_buff->mr->lkey, mr_size), 2);
  rdma_buff->mr_size = mr_size;
  allocated_mr_size += mr_size;
  debug(fprintf(stderr,"Preparing RDMA transfer\n"), 1);

  /* !! memset must be called !!*/
  memset(&wr, 0, sizeof(wr));
  wr.wr_id = (uintptr_t)conn;
  wr.opcode = IBV_WR_RDMA_READ;
  wr.sg_list = &sge;
  wr.num_sge = 1;
  wr.send_flags = IBV_SEND_SIGNALED;
  wr.wr.rdma.remote_addr = (uintptr_t)conn->peer_mr.addr; // wr.wr.rdma.remote_addr => uint64_t
  wr.wr.rdma.rkey = conn->peer_mr.rkey;

  sge.addr = (uintptr_t)rdma_buff->recv_base_addr;
  sge.length = (uint32_t)mr_size;

  sge.lkey = (uint32_t)rdma_buff->mr->lkey;


  debug(printf("RDMA lib: Preparing RDMA transfer: Done\n"), 1);
  if ((ibv_post_send(conn->qp, &wr, &bad_wr))) {
    fprintf(stderr, "RDMA lib: ERROR: post send failed @ %s:%d\n", __FILE__, __LINE__);
    exit(1);
  }
  debug(printf("RDMA lib: RECV: Post RDMA_READ: remote_addr=%lu, rkey=%u, sge.addr=%lu, sge.length=%lu,  sge.lkey=%lu\n", wr.wr.rdma.remote_addr, wr.wr.rdma.rkey, sge.addr, sge.length, sge.lkey), 2);


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

  int wait_msg_sent();
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

int recv_ctl_msg(uint32_t *cmt, uint64_t *data)
{
  void* ctx;
  struct ibv_wc wc;
  uint16_t slid;


  while(1) {
    if (cq != NULL) {

      while (ibv_poll_cq(cq, 1, &wc)) {

	conn = (struct connection *)(uintptr_t)wc.wr_id;
	slid = wc.slid;

	/*Check if a request was successed*/
	if (wc.status != IBV_WC_SUCCESS) {
	  const char* err_str = rdma_err_status_str(wc.status);
	  fprintf(stderr, "RDMA lib: COMM: ERROR: status is not IBV_WC_SUCCESS: Erro=%s(%d) @ %s:%d\n", err_str, wc.status, __FILE__, __LINE__);
	  exit(1);
	}

	/*Check which request was successed*/
	if (wc.opcode == IBV_WC_RECV) {
	  *cmt = conn->recv_msg->cmt;

	  switch (conn->recv_msg->cmt) 
	    {
	    case MR_INIT:
	      *data = conn->recv_msg->data1.buff_size;
	      break;
	    case MR_INIT_ACK:
	      break;
	    case MR_CHUNK:
	      *data = conn->recv_msg->data1.mr_size;
	      memcpy(&conn->peer_mr, &conn->recv_msg->data.mr, sizeof(struct ibv_mr));
	      break;    
	    case MR_CHUNK_ACK:
	      break;
	    case MR_FIN:
	      *data = conn->recv_msg->data1.tag;
	      break;
	    case MR_FIN_ACK:
	      break;
	    default:
	      fprintf(stderr, "unknow msg type @ %s:%d", __FILE__, __LINE__);
	      exit(1);
	    }
	  
	} else if (wc.opcode == IBV_WC_SEND) {
	  debug(printf("RDMA lib: COMM: Sent IBV_WC_SEND: id=%lu |%f\n", conn->id, get_dtime()), 2);
	  continue;
	} else if (wc.opcode == IBV_WC_RDMA_READ) {
	  debug(printf("RDMA lib: COMM: Sent IBV_WC_RDMA_READ: id=%lu\n", conn->id), 2);
	  continue;
	} else {
	  die("unknow opecode.");
	  continue;
	}
	debug(printf("RDMA lib: COMM: Recv %s: id=%lu, wc.slid=%u |%f\n", rdma_ctl_msg_type_str(*cmt), conn->id, slid, get_dtime()), 2);
	return (int)slid;
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

static int wait_msg_sent()
{
  void* ctx;
  struct ibv_wc wc;
  uint16_t slid;
  
  while(1) {
    if (cq != NULL) {
      while (ibv_poll_cq(cq, 1, &wc)) {
	conn = (struct connection *)(uintptr_t)wc.wr_id;
	slid = wc.slid;
	/*Check if a request was successed*/
	if (wc.status != IBV_WC_SUCCESS) {
	  const char* err_str = rdma_err_status_str(wc.status);
	  fprintf(stderr, "RDMA lib: COMM: ERROR: status is not IBV_WC_SUCCESS: Erro=%s(%d) @ %s:%d\n", err_str, wc.status, __FILE__, __LINE__);
	  exit(1);
	}
	/*Check which request was successed*/
	if (wc.opcode == IBV_WC_RECV) {
	  die("unknow opecode.");
	  continue;
	} else if (wc.opcode == IBV_WC_SEND) {
	  debug(printf("RDMA lib: COMM: Sent IBV_WC_SEND: id=%lu |%f\n", conn->id, get_dtime()), 2);
	  return 0;
	} else if (wc.opcode == IBV_WC_RDMA_READ) {
	  die("unknow opecode.");
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




/*Note: If cmt=MR_CHUNK, a register_rdma_msg_mr(...) function has to be called to register [mr_index]th memory region  */
int send_ctl_msg (enum ctl_msg_type cmt, uint32_t mr_index, uint64_t data)
{ 


  if (cmt == MR_CHUNK) {
    post_send_ctl_msg(conn, cmt, rdma_msg_mr[mr_index], data) ;
  } else {
    post_send_ctl_msg(conn, cmt, NULL, data) ;
  }
  post_recv_ctl_msg(conn);
  //  debug(printf("RDMA lib: COMM: Post %s\n",  rdma_ctl_msg_type_str(cmt)), 2);

  return 0;
}





static int post_send_ctl_msg(struct connection *conn, enum ctl_msg_type cmt, struct ibv_mr *mr, uint64_t data)
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
    memcpy(&conn->send_msg->data.mr, mr, sizeof(struct ibv_mr));
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


  TEST_NZ(ibv_post_send(conn->qp, &wrs, &bad_wrs));
  debug(printf("RDMA lib: COMM: Post %s: post_count=%lu, local_addr=%lu, length=%lu, lkey=%lu\n", rdma_ctl_msg_type_str(cmt), wrs.imm_data, sges.addr, sges.length, sges.lkey), 2);

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






