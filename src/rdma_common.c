#include "rdma_common.h"
#include <unistd.h>

static int post_send_ctl_msg(struct connection *conn, enum ctl_msg_type cmt, struct ibv_mr *mr, uint64_t data);

const int TIMEOUT_IN_MS = 500; /* ms */
struct ibv_cq *cq = NULL;
static struct context *s_ctx = NULL;
struct ibv_mr **rdma_msg_mr;

struct ibv_mr *peer_mr;


static int wait_for_event(struct rdma_event_channel *channel, enum rdma_cm_event_type requested_event);
static void build_connection(struct rdma_cm_id *id);
static void build_context(struct ibv_context *verbs);
static void build_params(struct rdma_conn_param *params);
static void build_qp_attr(struct ibv_qp_init_attr *qp_attr);
static void register_memory(struct connection *conn);

void die(const char *reason)
{
  debug(printf("%s\n", reason), 1);
  exit(EXIT_FAILURE);
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
  TEST_Z(s_ctx->cq = ibv_create_cq(s_ctx->ctx, 100, NULL, s_ctx->comp_channel, 0)); /* cqe=10 is arbitrary/ comp_vector:0 */
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

  qp_attr->cap.max_send_wr = 10;// 10                                                                                                                                 
  qp_attr->cap.max_recv_wr = 10;//10                                                                                                                                  
  qp_attr->cap.max_send_sge = 5;//1                                                                                                                                   
  qp_attr->cap.max_recv_sge = 5;//1                                                                                                                                   


}


static void build_connection(struct rdma_cm_id *id)
{
  struct connection *conn;
  struct ibv_qp_init_attr qp_attr;

  build_context(id->verbs);
  build_qp_attr(&qp_attr);

  TEST_NZ(rdma_create_qp(id, s_ctx->pd, &qp_attr));

  id->context = conn = (struct connection *)malloc(sizeof(struct connection));

  conn->id = id;
  conn->qp = id->qp;

  //  conn->send_state = SS_INIT;                                                                                                                                     
  //  conn->recv_state = RS_INIT;                                                                                                                                     

  conn->connected = 0;

  register_memory(conn);

}

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

void register_rdma_msg_mr(int mr_index, void* addr, uint32_t size)
{
  if (rdma_msg_mr[mr_index] != NULL) {
    int retry = 100;
    usleep(1000 * 100);
    while (ibv_dereg_mr(rdma_msg_mr[mr_index]) != 0) {
      fprintf(stderr, "RDMA lib: SEND: FAILED: memory region dereg again: retry = %d @ %s:%d\n", retry, __FILE__, __LINE__);
      exit(1);
      if (retry < 0) {
        fprintf(stderr, "RDMA lib: SEND: ERROR: memory region deregistration failed @ %s:%d\n",  __FILE__, __LINE__);
        exit(1);
      }
      retry--;
    }
  }
  
  TEST_Z(rdma_msg_mr[mr_index] = ibv_reg_mr(
					    s_ctx->pd,
					    addr,
					    size,
                                  IBV_ACCESS_LOCAL_WRITE
                                  | IBV_ACCESS_REMOTE_READ
					    | IBV_ACCESS_REMOTE_WRITE));
  return;
}




/*
int get_cqe (struct context *s_ctx, struct ibv_wc *wc) {

  struct ibv_cq *cq = NULL;
  void* ctx;
  int num_entries;

  //TODO: Retrieve multiple wc, for now, num_entries=1, which means to get only 1 wc at once 
  while ((num_entries = ibv_poll_cq(cq, 1, wc)) <= 0 || cq == NULL) {
    printf("%d\n", num_entries);
    if ((ibv_get_cq_event(s_ctx->comp_channel, &cq, &ctx))) {
      fprintf(stderr, "RDMA lib: ERROR: ibv get cq event failed @ %s:%d\n", __FILE__, __LINE__);
      exit(1);
    }

    ibv_ack_cq_events(cq, 1);

    if ((ibv_req_notify_cq(cq, 0))) {
      fprintf(stderr, "RDMA lib: ERROR: ibv request notification failed @ %s:%d\n", __FILE__, __LINE__);
      exit(1);
    }
  }

//  Check if the request is successed, otherwise print the reason
  if (wc->status != IBV_WC_SUCCESS) {
    const char* err_str = rdma_err_status_str(wc->status);
    fprintf(stderr, "RDMA lib: RECV: ERROR: status is not IBV_WC_SUCCESS: Error=%s(%d) @ %s:%d\n", err_str, wc->status, __FILE__, __LINE__);
    exit(1);
  }
}
*/
/*
int send_control_msg (struct connection *conn, struct control_msg *cmsg)
{
  struct ibv_send_wr wrs, *bad_wrs = NULL;
  struct ibv_sge sges;

  conn->send_msg->type = cmsg->type;
  memcpy(&conn->send_msg->data.mr, &cmsg->data.mr, sizeof(struct ibv_mr));
  conn->send_msg->data1 = cmsg->data1;

  memset(&wrs, 0, sizeof(wrs));

  wrs.wr_id = (uintptr_t)conn;
  //printf("wr.wr_id=%lu\n", wr.wr_id);
  //wr.wr_id = (uintptr_t)1;
  wrs.opcode = IBV_WR_SEND;
  wrs.sg_list = &sges;
  wrs.num_sge = 1;
  wrs.send_flags = IBV_SEND_SIGNALED;

  sges.addr = (uintptr_t)conn->send_msg;
  sges.length = (uint32_t)sizeof(struct control_msg);
  sges.lkey = (uint32_t)conn->send_mr->lkey;
  
  printf("RDMA: sge.addr=%lu, sge.length=%lu, sge.lkey=%lu\n", sges.addr, sges.length, sges.lkey);
  
  
  //  while (!conn->connected);

  TEST_NZ(ibv_post_send(conn->qp, &wrs, &bad_wrs));
  debug(printf("Post Send: TYPE=%d\n", conn->send_msg->type), 1);
  return 0;
}



void post_receives(struct connection *conn)
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
  debug(printf("Post Recive: id=%lu\n", wrr.wr_id), 1);
  return;
}
*/

/*Output: slid (soruce local id: 16 bytes)*/
/*Note: If cmt=MR_CHUNK, rdma_read_req has to be called before the next recv_ctl_msg call*/

int recv_ctl_msg(struct connection *conn, uint32_t *cmt, uint64_t *data)
{
  void* ctx;
  struct ibv_wc wc;
  uint16_t slid;

  cmt=NULL;
  data=NULL;
  
  while(1) {

    if (cq != NULL) {
      while (ibv_poll_cq(cq, 1, &wc)) {
	conn = (struct connection *)(uintptr_t)wc.wr_id;
	slid = wc.slid;
	debug(printf("Control MSG from: %lu\n", (uintptr_t)conn->id), 2);

	/*Check if a request was successed*/
	if (wc.status != IBV_WC_SUCCESS) {
	  const char* err_str = rdma_err_status_str(wc.status);
	  fprintf(stderr, "RDMA lib: COMM: ERROR: status is not IBV_WC_SUCCESS: Erro=%s(%d) @ %s:%d\n", err_str, wc.status, __FILE__, __LINE__);
	  exit(1);
	}
	
	/*Check which request was successed*/
	//	memcpy(cmt, conn->recv_msg->cmt, sizeof(cmt));
	printf("a====%d, %d\n", *cmt, conn->recv_msg->cmt);
	if (wc.opcode == IBV_WC_RECV) {
	  switch (conn->recv_msg->cmt) {
	  case MR_INIT:
	    *cmt = MR_INIT;
	    *data = conn->recv_msg->data1.buff_size;
	     break;
	  case MR_INIT_ACK:
	    printf("a====%d, %d\n", *cmt, conn->recv_msg->cmt);
	    *cmt = MR_INIT_ACK;
	    printf("b====%d, %d\n", *cmt, conn->recv_msg->cmt);
	    break;
	  case MR_CHUNK:
	    *cmt = MR_CHUNK;
	    *data = conn->recv_msg->data1.mr_size;
   	    memcpy(peer_mr, &conn->recv_msg->data.mr, sizeof(struct ibv_mr));
	    break;    
	  case MR_CHUNK_ACK:
	    *cmt = MR_CHUNK_ACK;
	    break;
	  case MR_FIN:
	    *cmt = MR_FIN;
	    *data = conn->recv_msg->data1.tag;
	    break;
	  case MR_FIN_ACK:
	    *cmt = MR_FIN_ACK;
	    break;
	  default:
	    fprintf(stderr, "unknow msg type @ %s:%d", __FILE__, __LINE__);
	    exit(1);
	  }

	} else if (wc.opcode == IBV_WC_SEND) {
	  debug(printf("RDMA lib: COMM: Sent: Sent out: TYPE=%d for wc.slid=%lu\n", conn->send_msg->cmt, (uintptr_t)wc.slid), 1);
	} else if (wc.opcode == IBV_WC_RDMA_READ) {
	  debug(printf("RDMA lib: COMM: Sent: RDMA: IBV_WC_RDMA_READ for wc.slid=%lu\n", (uintptr_t)wc.slid), 1);
	} else {
	  die("unknow opecode.");
	}
	return (int)slid;
      }
    }
    //cq = (struct ibv_cq *) malloc (sizeof(struct ibv_cq));
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
int send_ctl_msg (struct connection *conn, enum ctl_msg_type cmt, uint32_t mr_index, uint64_t data)
{ 
  post_recv_ctl_msg(conn);
  if (cmt == MR_CHUNK) {
    post_send_ctl_msg(conn, cmt, rdma_msg_mr[mr_index], data) ;
  } else {
    post_send_ctl_msg(conn, cmt, NULL, data) ;
  }
  return 0;
}


static int post_send_ctl_msg(struct connection *conn, enum ctl_msg_type cmt, struct ibv_mr *mr, uint64_t data)
{ 
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

  sges.addr = (uintptr_t)conn->send_msg;
  sges.length = (uint32_t)sizeof(struct control_msg);
  sges.lkey = (uint32_t)conn->send_mr->lkey;

  TEST_NZ(ibv_post_send(conn->qp, &wrs, &bad_wrs));
  printf("RDMA: sge.addr=%lu, sge.length=%lu, sge.lkey=%lu\n", sges.addr, sges.length, sges.lkey);
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


