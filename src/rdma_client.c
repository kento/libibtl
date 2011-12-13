#include "common.h"
#include "rdma_client.h"
#include "assert.h"
#include "arpa/inet.h"
#include "time.h"

struct poll_cq_args{
  struct RDMA_communicator *comm;
  struct RDMA_message *msg;
  int *flag;
};

const int TIMEOUT_IN_MS = 500; /* ms */

static int wait_for_event(struct rdma_event_channel *channel, enum rdma_cm_event_type requested_event);
static void* poll_cq(struct poll_cq_args* args);
static void build_connection(struct rdma_cm_id *id);
static void build_context(struct ibv_context *verbs);
static void build_params(struct rdma_conn_param *params);
static void build_qp_attr(struct ibv_qp_init_attr *qp_attr);
static void register_memory(struct connection *conn);
static void register_rdma_region(struct connection *conn,  void* addr, uint32_t size);
static void register_rdma_msg_mr(int mr_index, void* addr, uint32_t size);
static void set_envs (void);

static struct context *s_ctx = NULL;
struct ibv_mr *rdma_msg_mr[RDMA_BUF_NUM_C];

static uint32_t rdma_buf_size = 0;

static void set_envs () {
  char *value;
  value = getenv("RDMA_CLIENT_NUM_S");
  if (value == NULL) {
    rdma_buf_size = RDMA_BUF_SIZE_C;
  } else {
    rdma_buf_size  =  MAX_RDMA_BUF_SIZE_C / atoi(value);
  }
  fprintf(stderr, "rdma_buf_size: %d\n", rdma_buf_size);
}

int RDMA_Wait (int *flag) {
  //  int count = 0;
  while (*flag == 0) {
    usleep(100 * 1000);
    //    count++;
  };
  //  fprintf(stderr, "WCOUNT: %d\n", count);
  return 0;
}

int RDMA_Sendr (char *buff, uint64_t size, int tag, struct RDMA_communicator *comm)
{
  int flag=0;
  RDMA_Isendr(buff, size, tag, &flag, comm);
  //  int count = 0;
  RDMA_Wait (&flag) ;

  //  fprintf(stderr, "WCOUNT_S: %d\n", count);
  return 0;
}

int RDMA_Sendr_ns (char *buff, uint64_t size, int tag, struct RDMA_communicator *comm)
{
  int flag=0;
  RDMA_Isendr(buff, size, tag, &flag, comm);
  while (flag == 0) {
    usleep(1);
  } ;
  fprintf(stderr, "WCOUNT_S\n");
  //  fprintf(stderr, "WCOUNT_S: %d\n", count);
  return 0;
}

int RDMA_Isendr(char *buff, uint64_t size, int tag, int *flag, struct RDMA_communicator *comm)
{
  struct poll_cq_args *args = (struct poll_cq_args*)malloc(sizeof(struct poll_cq_args));
  struct RDMA_message *msg = (struct RDMA_message*)malloc(sizeof(struct RDMA_message));

  args->comm = comm;
  args->msg = msg;
  args->flag = flag;

  msg->buff = buff;
  msg->size = size;
  msg->tag  = tag;

  if (pthread_create(&s_ctx->cq_poller_thread, NULL,(void *)poll_cq, args)) {
    fprintf(stderr, "RDMA lib: SEND: ERROR: pthread create failed @ %s:%d", __FILE__, __LINE__);
    exit(1);
  }

  return 0;
}


//static int run(int argc, char **argv)
//int RDMA_Connect(struct RDMA_communicator *comm, struct RDMA_param *param)
int RDMA_Active_Init(struct RDMA_communicator *comm, struct RDMA_param *param)
{
  struct addrinfo *addr;
  //  struct rdma_cm_id *cm_id= NULL;
  //  struct rdma_event_channel *ec = NULL;
  struct rdma_conn_param cm_params;
  char port[8];
  //  int i,j;

  sprintf(port, "%d", RDMA_PORT);

  set_envs();

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
  //  on_connect(cm_id->context);
  int i ;
  for (i = 0; i < RDMA_BUF_NUM_C; i++){ rdma_msg_mr[i] = NULL;}



  return 0;
}


//static void* poll_cq(struct RDMA_communicator* comm)
static void* poll_cq(struct poll_cq_args* args)
{
  struct ibv_cq *cq;
  struct ibv_wc wc;
  struct connection *conn;
  struct RDMA_communicator *comm;
  //  struct RDMA_message *msg;
  double s, e;
  char* ip;

  struct control_msg cmsg;
  void* ctx;
  char* buff; 
  uint64_t buff_size;
  int tag;

  uint32_t mr_size=0;
  uint64_t sent_size=0;
  char* send_base_addr;

  int* flag = args->flag;
  int mr_index;

  //for (i = 0; i < RDMA_BUF_NUM_C; i++){ rdma_msg_mr[i] = NULL;}
  
  comm = args->comm;
  buff = args->msg->buff;
  send_base_addr = args->msg->buff;
  buff_size= args->msg->size;
  tag= args->msg->tag;

  //cmsg.type=MR_INIT;
  //  cmsg.data1.buff_size=buff_size;
  //  send_control_msg(comm->cm_id->context, &cmsg);
  //  post_receives(comm->cm_id->context);

  send_ctl_msg (comm->cm_id->context, MR_INIT, NULL, buff_size);

  s = get_dtime();


  while (1) {
    if (ibv_get_cq_event(s_ctx->comp_channel, &cq, &ctx)) {
      fprintf(stderr, "RDMA lib: SEND: ERROR: get cq event  failed @ %s:%d\n", __FILE__, __LINE__);
      exit(1);
    }
    ibv_ack_cq_events(cq, 1);
    if (ibv_req_notify_cq(cq, 0)) {
      fprintf(stderr, "RDMA lib: SEND: ERROR: request notification failed @ %s:%d\n", __FILE__, __LINE__);
      exit(1);
    }

    int a;
    while (ibv_poll_cq(cq, 1, &wc)){
      conn = (struct connection *)(uintptr_t)wc.wr_id;
      debug(printf("Control MSG from: %lu\n", (uintptr_t)conn->id), 2);
      if (wc.status != IBV_WC_SUCCESS) {
	const char* err_str = rdma_err_status_str(wc.status);
        fprintf(stderr, "RDMA lib: RECV: ERROR: status is not IBV_WC_SUCCESS: Erro=%s(%d) @ %s:%d\n", err_str, wc.status, __FILE__, __LINE__);
        exit(1);
      }

      if (wc.opcode == IBV_WC_RECV) {
        switch (conn->recv_msg->cmt)
          {
          case MR_INIT_ACK:
	    for (mr_index = 0; mr_index < RDMA_BUF_NUM_C; mr_index++) {
	      debug(printf("Recived: Type=%d\n",  conn->recv_msg->cmt), 1);
	      if (sent_size == buff_size) {
		debug(printf("RDMA lib: SEND: Recieved MR_INIT_ACK => FIN: for tag=%d\n",  tag), 2);
		/*sent all data*/
		//		cmsg.type=MR_FIN;
		//		cmsg.data1.tag=tag;
		send_ctl_msg (conn, MR_FIN, NULL, tag);
	      } else {
		debug(printf("RDMA lib: SEND: Recieved MR_INIT_ACK: for tag=%d\n",  tag), 2);
		/*not sent all data yet*/
		if (sent_size + rdma_buf_size > buff_size) {
		  mr_size = (uint32_t)(buff_size - sent_size);
		} else {
		  mr_size = rdma_buf_size;
		}

		register_rdma_msg_mr(mr_index, send_base_addr, mr_size);
		printf("RDMA lib: SEND: send_base_addr=%lu\n", send_base_addr);
		send_base_addr += mr_size;
		sent_size += (uint64_t)mr_size;
		
		//cmsg.type=MR_CHUNK;
		//		memcpy(&cmsg.data.mr, rdma_msg_mr[mr_index], sizeof(struct ibv_mr));
		//		cmsg.data1.mr_size = mr_size;//cmsg.data.mr.length;//mr_size - 1 ;
		printf("RDMA lib: SEND:    remote_addr=%lu(%lu), rkey=%u, size=%u\n", cmsg.data.mr.addr, send_base_addr, cmsg.data.mr.rkey, cmsg.data.mr.length);
		send_ctl_msg (conn, MR_CHUNK, rdma_msg_mr[mr_index], mr_size);
	      }
	      debug(printf("RDMA lib: SEND: Done MR_INIT_ACK: for tag=%d\n",  tag), 2);
	    }
            break;
          case MR_CHUNK_ACK:

	    if (sent_size == buff_size) {
	      debug(printf("RDMA lib: SEND: Recieved MR_CHUNK_ACK => FIN: for tag=%d\n",  tag), 2);
              /*sent all data*/
	      //cmsg.type=MR_FIN;
	      //cmsg.data1.tag=tag;
	      send_ctl_msg (conn, MR_FIN, NULL, tag);
	    } else {
              /*not sent all data yet*/
	      debug(printf("RDMA lib: SEND: Recieved MR_CHUNK_ACK: for tag=%d\n",  tag), 2);
	      if (sent_size + rdma_buf_size > buff_size) {
		mr_size = (uint32_t)(buff_size - sent_size);
	      } else {
		mr_size = rdma_buf_size;
	      }

	      debug(printf("mr_size=%lu\n", mr_size),2);

	      mr_index = (mr_index+ 1) % RDMA_BUF_NUM_C;
	      debug(printf("mr_index=%d\n", mr_index),2);

	      register_rdma_msg_mr(mr_index, send_base_addr, mr_size);
	      printf("RDMA lib: SEND: send_base_addr=%lu\n", send_base_addr);

	      send_base_addr += mr_size;
	      sent_size += (uint64_t)mr_size;

	      //cmsg.type=MR_CHUNK;
	      //	      memcpy(&cmsg.data.mr, rdma_msg_mr[mr_index], sizeof(struct ibv_mr));
	      //	      cmsg.data1.mr_size = mr_size;//cmsg.data.mr.length;
	      printf("RDMA lib: SEND:    remote_addr=%lu(%lu), rkey=%u, size=%u, site_t=%ubyte\n", cmsg.data.mr.addr, send_base_addr, cmsg.data.mr.rkey, cmsg.data.mr.length, sizeof(mr_size));
	      send_ctl_msg (conn, MR_CHUNK, rdma_msg_mr[mr_index], mr_size);
	    }
	    //	    send_control_msg(conn, &cmsg);
	    //	    post_receives(conn);
	    debug(printf("RDMA lib: SEND: Done MR_CHUNK_ACK: for tag=%d\n",  tag), 2);
            break;
          case MR_FIN_ACK:
	    //	    sleep(1);
            debug(printf("RDMA lib: SEND: Recived MR_FIN_ACK: Type=%d\n",  conn->recv_msg->cmt),2);
	    *flag = 1;

	    // rdma_disconnect(comm->cm_id);
	    // rdma_disconnect(conn->id);
	    //exit(0);
	    e = get_dtime();
	    free(args->msg);
	    free(args);
	    //	    fprintf(stderr, "RDMA lib: SEND: FIN_ACK: tag=%d\n", tag);
	    //ip = get_ip_addr("ib0");
	    //	    printf("RDMA lib: SEND: %s: send time= %f secs, send size= %lu MB, throughput = %f MB/s\n", ip, e - s, buff_size/1000000, buff_size/(e - s)/1000000.0);
            debug(printf("RDMA lib: SEND: Done MR_FIN_ACK: Type=%d\n",  conn->recv_msg->cmt),1);
	    return NULL;
          default:
            debug(printf("Unknown TYPE"), 1);
	    return NULL;
          }
      } else if (wc.opcode == IBV_WC_SEND) {
	//fprintf(stderr, "RDMA lib: SENT: DONE: tag=%d\n", tag);
	debug(printf("RDMA lib: SEND: Sent: TYPE=%d, tag=%d\n", conn->send_msg->cmt, tag),2);
      } else {
	die("unknow opecode.");
      }

    }
  }
  return NULL;
}

static void register_rdma_msg_mr(int mr_index, void* addr, uint32_t size)
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


static void register_rdma_region(struct connection *conn,  void* addr, uint32_t size)
{ 
  if (conn->rdma_msg_mr != NULL) {
    ibv_dereg_mr(conn->rdma_msg_mr);
  }
  //  printf("Regist size=%d\n", size);                                                                                                                                                                                                                                                                                                                                                       
  TEST_Z(conn->rdma_msg_mr = ibv_reg_mr(
                                        s_ctx->pd,
                                        addr,
                                        size,
                                    IBV_ACCESS_LOCAL_WRITE
                                      | IBV_ACCESS_REMOTE_READ
                                        | IBV_ACCESS_REMOTE_WRITE));
  return;
}


/*************************************************************************
 * Wait for the rdma_cm event specified.
 * If another event comes in return an error.
 */
static int
wait_for_event(struct rdma_event_channel *channel, enum rdma_cm_event_type requested_event)
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


int RDMA_Active_Finalize(struct RDMA_communicator *comm)
{
  TEST_NZ(wait_for_event(comm->ec, RDMA_CM_EVENT_DISCONNECTED));
  rdma_destroy_id(comm->cm_id);
  rdma_destroy_event_channel(comm->ec);
  return 0;
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


void register_memory(struct connection *conn)
{
  conn->send_msg = malloc(sizeof(struct control_msg));
  conn->recv_msg = malloc(sizeof(struct control_msg));

  //  conn->rdma_local_region = malloc(RDMA_BUFFER_SIZE);
  //  conn->rdma_remote_region = malloc(RDMA_BUFFER_SIZE);
  //  conn->rdma_msg_region = malloc(RDMA_BUFFER_SIZE);

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

  //  TEST_Z(conn->rdma_msg_mr = ibv_reg_mr(
  //    s_ctx->pd, 
  //    conn->rdma_msg_region, 
  //    RDMA_BUFFER_SIZE, 
  //    IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ));

  //    IBV_ACCESS_LOCAL_WRITE | ((s_mode == M_WRITE) ? IBV_ACCESS_REMOTE_WRITE : IBV_ACCESS_REMOTE_READ)));

  /*
    TEST_Z(conn->rdma_local_mr = ibv_reg_mr(
    s_ctx->pd, 
    conn->rdma_local_region, 
    RDMA_BUFFER_SIZE, 
    IBV_ACCESS_LOCAL_WRITE));


  TEST_Z(conn->rdma_remote_mr = ibv_reg_mr(
    s_ctx->pd, 
    conn->rdma_remote_region, 
    RDMA_BUFFER_SIZE, 
    IBV_ACCESS_LOCAL_WRITE | ((s_mode == M_WRITE) ? IBV_ACCESS_REMOTE_WRITE : IBV_ACCESS_REMOTE_READ)));
  */
  return;
}
