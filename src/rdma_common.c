#include "rdma_common.h"
#include <unistd.h>

static int post_send_ctl_msg(struct connection *conn, enum ctl_msg_type cmt, struct ibv_mr *mr, uint64_t data);

struct ibv_cq *cq = NULL;

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


int recv_ctl_msg (struct connection *conn, enum ctl_msg_type *cmt, struct ibv_mr *mr, uint64_t *data)
{
  void* ctx;
  struct ibv_wc wc;
  
  while (ibv_poll_cq(cq, 1, &wc)){
    conn = (struct connection *)(uintptr_t)wc.wr_id;
    debug(printf("Control MSG from: %lu\n", (uintptr_t)conn->id), 2);
    
    /*Check if a request was successed*/
    if (wc.status != IBV_WC_SUCCESS) {
      const char* err_str = rdma_err_status_str(wc.status);
      fprintf(stderr, "RDMA lib: COMM: ERROR: status is not IBV_WC_SUCCESS: Erro=%s(%d) @ %s:%d\n", err_str, wc.status, __FILE__, __LINE__);
      exit(1);
    }

    /*Check which request was successed*/
    if (wc.opcode == IBV_WC_RECV) {

    } else if (wc.opcode == IBV_WC_SEND) {
      debug(printf("RDMA lib: COMM: Sent: Sent out: TYPE=%d for wc.slid=%lu\n", conn->send_msg->cmt, (uintptr_t)wc.slid), 1);
    } else if (wc.opcode == IBV_WC_RDMA_READ) {
      debug(printf("RDMA lib: COMM: Sent: RDMA: IBV_WC_RDMA_READ for wc.slid=%lu\n", (uintptr_t)wc.slid), 1);
    } else {
      die("unknow opecode.");
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


  return 0;

}

int send_ctl_msg (struct connection *conn, enum ctl_msg_type cmt, struct ibv_mr *mr, uint64_t data)
{ 
  post_recv_ctl_msg(conn);
  post_send_ctl_msg(conn, cmt, mr, data) ;
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


