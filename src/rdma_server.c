#include "common.h"
#include "rdma_server.h"
#include "buffer_table.h"
#include "hashtable.h"
#include <time.h>

/*
static int on_connect_request(struct rdma_cm_id *id);
static int on_connection(struct rdma_cm_id *id);
static int on_disconnect(struct rdma_cm_id *id);
static int on_event(struct rdma_cm_event *event);
*/

static void accept_connection(struct rdma_cm_id *id);
static void *poll_cq(void *ctx);
static void build_connection(struct rdma_cm_id *id);
static void build_context(struct ibv_context *verbs);
static void build_params(struct rdma_conn_param *params);
static void build_qp_attr(struct ibv_qp_init_attr *qp_attr);
static void register_memory(struct connection *conn);
static void register_rdma_region(struct connection *conn,  void* addr, uint64_t size);
static void append_rdma_msg(uint64_t conn_id, struct RDMA_message *msg);
static void* passive_init(void * arg /*(struct RDMA_communicator *comm)*/) ;


//int RDMA_Passive_Init(struct RDMA_communicator *comm);
//int RDMA_Irecv(char* buff, int* size, int* tag, struct RDMA_communicator *comm);
//int RDMA_Passive_Finalize(struct RDMA_communicator *comm);

static struct context *s_ctx = NULL;
static int connections = 0;
pthread_t listen_thread;
//static uint32_t allocated_mr_size = 0;
static  int client_num = 0;

//pthread_t poll_thread[RDMA_THREAD_NUM_S];
//int poll_thread_count = 0;

struct poll_cq_args{
  int thread_id;
};


/*
int main(int argc, char **argv) {
  struct RDMA_communicator comm;
  RDMA_Passive_Init(&comm);
  return 0;
}
*/

int RDMA_Passive_Init(struct RDMA_communicator *comm) 
{

  char *value;
  value = getenv("RDMA_CLIENT_NUM_S");
  if (value == NULL) {
    client_num = RDMA_CLIENT_NUM_S;
  } else {
    client_num = atoi(value);
  }
  fprintf(stderr, "client num: %d\n", client_num);
  create_hashtable(client_num);

  TEST_NZ(pthread_create(&listen_thread, NULL, (void *) rdma_passive_init, comm));
  wait_accept();
  TEST_NZ(pthread_create(&listen_thread, NULL, poll_cq, NULL));

  return 0;
}



int RDMA_Irecvr(char** buff, uint64_t* size, int* tag, struct RDMA_communicator *comm)
{
  struct RDMA_message *rdma_msg;
  rdma_msg = get_current();
  *buff = rdma_msg->buff;
  *size = rdma_msg->size;
  *tag = rdma_msg->tag;
  return 0;
}

int RDMA_Recvr(char** buff, uint64_t* size, int* tag, struct RDMA_communicator *comm)
{
  struct RDMA_message *rdma_msg;
  while ((rdma_msg = get_current()) == NULL) {
    usleep(1);
  }
  *buff = rdma_msg->buff;
  *size = rdma_msg->size;
  *tag = rdma_msg->tag;
  return 0;
}



void RDMA_free (void* data) {
  free(data);
}

static void * poll_cq(void *ctx /*ctx == NULL*/)
{  

  struct ibv_cq *cq;
  struct ibv_wc wc;
  struct ibv_send_wr wr, *bad_wr = NULL;
  struct ibv_sge sge;

  struct connection *conn;
  struct control_msg cmsg;
  struct RDMA_message *rdma_msg;

  int tag;

  uint32_t mr_size;
  //  uint64_t conn_id;


  struct RDMA_buff *rdma_buff;

  char log[256];

  double b_usage;
  double data_in_count = 0;
  char *ip;
  int retry=0;
  
  uint32_t *cmt;
  uint64_t *data;
  uint64_t buff_size;
  uint16_t slid;


  init_ctl_msg (&cmt, &data);
  init_rdma_buffs (client_num);

  while (1) {

    slid = recv_ctl_msg (cmt, data);

    switch (*cmt)
      {
      case MR_INIT:
	buff_size = *data;
	/*Allocat buffer for the client msg*/ 
	debug(printf("RDMA lib: RECV: Recieved MR_INI: for wc.slid=%lu\n",  slid), 2);

	retry=0;	
	while ((b_usage = buff_usage()) > 20 * 1000) {
	  fprintf(stderr, "RDMA lib: RECV: Buffer Overflows (usage: %f M bytes) => sleep ... for a while: retry %d !!\n", b_usage, retry);
	  usleep(1000 * 1000);
	  retry++;
	}

	data_in_count += buff_size / 1000.0;
	debug(fprintf(stderr, "RDMA lib: RECV: IPoIB=%s Time= %f , in_count= %f \n", get_ip_addr("ib0"), get_dtime(), data_in_count), 2);

	alloc_rdma_buffs(slid, buff_size);
	/*
	retry=0;
	while ((rdma_buff = (struct RDMA_buff *) malloc(sizeof(struct RDMA_buff))) == NULL) {
	  fprintf(stderr, "RDMA lib: RECV: No more buffer space !!\n");
	  usleep(10000);
	  retry++;
	  //	      exit(1);
	}
	//	    alloc_size += sizeof(struct RDMA_buff);
	retry=0;
	while ((rdma_buff->buff = (char *)malloc(buff_size)) == NULL) {
	  fprintf(stderr, "RDMA lib: RECV: No more buffer space !!\n");
	  usleep(10000);
	  retry++;
	  //	      exit(1);
	}

	
	rdma_buff->buff_size = buff_size;
	rdma_buff->recv_base_addr = rdma_buff->buff;
	rdma_buff->mr = NULL;
	//	    rdma_buff->mr_size = 0;
	rdma_buff->recv_size = 0;
	add_ht(&ht, (uintptr_t)wc.slid, rdma_buff);
	*/


	/**/
	//	    cmsg.type=MR_INIT_ACK;
	//	    post_receives(conn);
	//	    send_control_msg(conn, &cmsg);
	send_ctl_msg (MR_INIT_ACK, 0, 0);
	debug(printf("RDMA lib: RECV: Done MR_INI : for wc.slid=%u\n", slid), 2);
	break;
      case MR_CHUNK:
	//	    printf("a\n");

	mr_size= *data;


	rdma_read(slid, mr_size);
	  /*
	debug(printf("RDMA lib: RECV: Recieved MR_CHUNK : for wc.slid=%lu\n", (uintptr_t)wc.slid), 2);
	//	    sprintf(log, "RDMA lib: RECV: Recieved MR_CHUNK : for wc.slid=%lu\n", (uintptr_t)wc.slid);	    write_log(log);

	exit(1);
	debug(fprintf(stderr,"copy remote memory region\n"), 1);
	memcpy(&conn->peer_mr, &conn->recv_msg->data.mr, sizeof(conn->peer_mr));
	debug(fprintf(stderr," done\n"), 1);
	debug(fprintf(stderr,"rdma_buff:%p \n", rdma_buff), 1);
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
	debug(fprintf(stderr, "Registering RDMA MR: %lu\n", rdma_buff->mr_size), 1);
	if (!(rdma_buff->mr = ibv_reg_mr(
					 s_ctx->pd,
					 rdma_buff->recv_base_addr,
					 mr_size,
					 IBV_ACCESS_LOCAL_WRITE
					 )
	      )
	    )
	  {
	    fprintf(stderr, "RDMA lib: ERROR: memory region registration failed (allocated_mr_size: %d bytes) @ %s:%d\n", allocated_mr_size, __FILE__, __LINE__);
	    exit(1);
	  }
	debug(printf("RDMA lib: RECV: ibv_reg_mr: addr=%lu, length=%lu,  lkey=%lu\n", rdma_buff->recv_base_addr, mr_size, rdma_buff->mr->lkey), 2);
	rdma_buff->mr_size = mr_size;
	allocated_mr_size += mr_size;
	//	    fprintf(stderr, "(allocated_mr_size: %d bytes) @ %s:%d\n", allocated_mr_size, __FILE__, __LINE__);
	
	debug(fprintf(stderr,"Preparing RDMA transfer\n"), 1);
	// !! memset must be called !!
	memset(&wr, 0, sizeof(wr));
	//	    printf("c\n");
	wr.wr_id = (uintptr_t)conn;
	//	    wr.wr_id =  (uintptr_t)conn->rdma_msg_region;
	wr.opcode = IBV_WR_RDMA_READ;
	wr.sg_list = &sge;
	wr.num_sge = 1;
	wr.send_flags = IBV_SEND_SIGNALED;
	wr.wr.rdma.remote_addr = (uintptr_t)conn->peer_mr.addr; // wr.wr.rdma.remote_addr => uint64_t
	wr.wr.rdma.rkey = conn->peer_mr.rkey;
	
	
	//sge.addr = (uintptr_t)conn->rdma_msg_region;
	//sge.addr = (uintptr_t)rdma_buff->recv_base_addr;
	sge.addr = (uintptr_t)rdma_buff->recv_base_addr;
	sge.length = (uint32_t)mr_size;
	//	    sge.lkey = conn->rdma_msg_mr->lkey;
	sge.lkey = (uint32_t)rdma_buff->mr->lkey;
	debug(printf("RDMA lib: RECV: RDMA request satus: remote_addr=%lu, rkey=%u, sge.addr=%lu, sge.length=%lu,  sge.lkey=%lu\n", wr.wr.rdma.remote_addr, wr.wr.rdma.rkey, sge.addr, sge.length, sge.lkey), 2);
	debug(printf("RDMA lib: Preparing RDMA transfer: Done\n"), 1);
	
	if ((ibv_post_send(conn->qp, &wr, &bad_wr))) {
	  fprintf(stderr, "RDMA lib: ERROR: post send failed @ %s:%d\n", __FILE__, __LINE__);
	  exit(1);
	}
	debug(printf("Post send: RDMA: id=%lu\n", wr.wr_id), 1);
	//	    printf("d\n");
	rdma_buff->recv_base_addr += (uintptr_t)mr_size;
	rdma_buff->recv_size += (uint64_t)mr_size;
	//	    cmsg.type=MR_CHUNK_ACK;
	//	    post_receives(conn);
	//	    send_control_msg(conn, &cmsg);
	*/

	send_ctl_msg (MR_CHUNK_ACK, 0, 0);
	debug(printf("RDMA lib: RECV: Done MR_CHUNK: for wc.slid=%lu\n", (uintptr_t)wc.slid), 2);
	//	    sprintf(log, "RDMA lib: RECV: Recieved MR_CHUNK DONE: for wc.slid=%lu\n", (uintptr_t)wc.slid);	    write_log(log);
	break;
      case MR_FIN:
	//	tag = *conn->recv_msg->data1.tag;
	tag = *data;

	debug(printf("RDMA lib: RECV: Recieved MR_FIN: Tag=%d for wc.slid=%lu\n", tag, slid), 2);
	//	    cmsg.type=MR_FIN_ACK;
	//	    post_receives(conn);
	//	    send_control_msg(conn, &cmsg);	    
	send_ctl_msg (MR_CHUNK_ACK, 0, 0);
	
	
	
	//	    write_log(log);
	/*Post reveived data*/
	//	rdma_buff = get_ht(&ht, (uintptr_t)wc.slid);
	rdma_msg = (struct RDMA_message*) malloc(sizeof(struct RDMA_message));
	get_rdma_buff(slid, &rdma_msg->buff, &rdma_msg->size);
	//	rdma_msg->buff = rdma_buff->buff;
	//	rdma_msg->size = rdma_buff->buff_size;
	rdma_msg->tag = tag;
	append_rdma_msg(slid, rdma_msg);
	/*Deregistering*/
	debug(fprintf(stderr,"Deregistering RDMA MR: %lu\n", rdma_buff->mr_size), 1);
	/*
	if (ibv_dereg_mr(rdma_buff->mr)) {
	  fprintf(stderr, "RDMA lib: ERROR: memory region deregistration failed (allocated_mr_size: %d bytes) @ %s:%d\n", allocated_mr_size, __FILE__, __LINE__);
	  exit(1);
	}
	allocated_mr_size = allocated_mr_size - rdma_buff->mr_size;
	*/
	//	    fprintf(stderr, "(allocated_mr_size: %d bytes) @ %s:%d\n", allocated_mr_size, __FILE__, __LINE__);
	//	free(rdma_buff);
	//	   del_ht(&ht, (uintptr_t)wc.slid);
	//	    show_ht(&ht);
	//	    printf("%s\n", rdma_msg->buff);
	//	    rdma_disconnect(conn->id);
	debug(printf("RDMA lib: RECV: Done MR_FIN: Tag=%d for wc.slid=%lu\n", tag, (uintptr_t)wc.slid), 2);
	/*Log*/
	break;
      default:
	  debug(printf("Unknown TYPE"), 1);
	  exit(1);
	  break;
      }
  }
  return NULL;
}


static void append_rdma_msg(uint64_t slid, struct RDMA_message *msg)
{
  append(slid, msg);
  return;
}

void RDMA_show_buffer(void)
{
  show();
}


static void register_rdma_region(struct connection *conn,  void* addr, uint64_t size)
{
  if (conn->rdma_msg_mr != NULL) {
    ibv_dereg_mr(conn->rdma_msg_mr);
  }
                                                                                                                                                                                               
  TEST_Z(conn->rdma_msg_mr = ibv_reg_mr(
                                        s_ctx->pd,
                                        addr,
                                        size,
                                    IBV_ACCESS_LOCAL_WRITE
                                      | IBV_ACCESS_REMOTE_READ
                                        | IBV_ACCESS_REMOTE_WRITE));
  return;
}

/*
void build_connection(struct rdma_cm_id *id)
{
  struct connection *conn;
  struct ibv_qp_init_attr qp_attr;

  build_context(id->verbs);
  build_qp_attr(&qp_attr);

  TEST_NZ(rdma_create_qp(id, s_ctx->pd, &qp_attr));

  id->context = conn = (struct connection *)malloc(sizeof(struct connection));

  conn->id = id;
  conn->qp = id->qp;
  conn->connected = 0;

  register_memory(conn);
  return;
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
  TEST_Z(s_ctx->cq = ibv_create_cq(s_ctx->ctx, 1000, NULL, s_ctx->comp_channel, 0)); // cqe=10 is arbitrary up to 131071 (36 nodes =>200 cq)
  TEST_NZ(ibv_req_notify_cq(s_ctx->cq, 0));


}
*/

/*
static void build_params(struct rdma_conn_param *params)
{
  memset(params, 0, sizeof(*params));

  params->initiator_depth = 1;
  params->responder_resources = 1;
  params->rnr_retry_count = 7; 
  params->retry_count = 7; 

}

static void build_qp_attr(struct ibv_qp_init_attr *qp_attr)
{
  memset(qp_attr, 0, sizeof(*qp_attr));

  qp_attr->send_cq = s_ctx->cq;
  qp_attr->recv_cq = s_ctx->cq;
  qp_attr->qp_type = IBV_QPT_RC;

  qp_attr->cap.max_send_wr = 10; //10
  qp_attr->cap.max_recv_wr = 10; //10
  qp_attr->cap.max_send_sge = 5; //5
  qp_attr->cap.max_recv_sge = 5;//5

  
}

void register_memory(struct connection *conn)
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

}
*/


/*
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
*/


int RDMA_Passive_Finalize(struct RDMA_communicator *comm)
{
  rdma_destroy_id(comm->cm_id);
  rdma_destroy_event_channel(comm->ec);
  return 0;
}
