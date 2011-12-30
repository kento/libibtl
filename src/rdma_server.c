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
static void *poll_cq(struct RDMA_communicator *comm);
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
  static int thread_running = 0;
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
  if (thread_running == 0) {
    thread_running = 1;
    TEST_NZ(pthread_create(&listen_thread, NULL, (void *)poll_cq, comm));
  }
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

static void * poll_cq(struct RDMA_communicator *comm)
{  

  struct ibv_cq *cq;
  struct ibv_wc wc;
  struct ibv_send_wr wr, *bad_wr = NULL;
  struct ibv_sge sge;

  struct control_msg cmsg;
  struct RDMA_message *rdma_msg;

  int tag;

  uint32_t mr_size;

  struct RDMA_buff *rdma_buff;

  char log[256];

  double b_usage;
  double data_in_count = 0;
  char *ip;
  int retry=0;
  

  uint64_t buff_size;

  int num_entries;
  struct connection* conn_send;
  struct connection* conn_recv;


  init_rdma_buffs (client_num);

  while (1) {
    double mm, ss, ee;
    ss = get_dtime();
    num_entries = recv_wc(1, &conn_recv);
    mm = ss - ee;
    ee = get_dtime();
    debug(fprintf(stdout, "RDMA lib: RECV: recv_wc time: %f(%f) (%s)\n", ee - ss, mm, ibv_wc_opcode_str(conn_recv->opcode) ), 2);

    /*Check which request was successed*/
    if (conn_recv->opcode == IBV_WC_RECV) {
      debug(printf("RDMA lib: COMM: Recv %s: id=%lu, wc.slid=%u |%f\n", rdma_ctl_msg_type_str(conn_recv->cmt), conn_recv->id, conn_recv->slid, get_dtime()), 2);
      switch (conn_recv->cmt)
	{
	case MR_INIT:
	  buff_size = conn_recv->recv_msg->data1.buff_size;
	  /*Allocat buffer for the client msg*/ 
	  //	debug(printf("RDMA lib: RECV: Recieved MR_INI: for wc.slid=%lu\n",  slid), 2);
	  retry=0;	
	  while ((b_usage = buff_usage()) > 20 * 1000) {
	    fprintf(stderr, "RDMA lib: RECV: Buffer Overflows (usage: %f M bytes) => sleep ... for a while: retry %d !!\n", b_usage, retry);
	    usleep(1000 * 1000);
	    retry++;
	  }
	  
	  alloc_rdma_buffs(conn_recv->slid, buff_size);
	  data_in_count += buff_size / 1000.0;
	  debug(fprintf(stderr, "RDMA lib: RECV: IPoIB=%s Time= %f , in_count= %f \n", get_ip_addr("ib0"), get_dtime(), data_in_count), 2);
	  conn_send = create_connection(comm->cm_id);
	  send_ctl_msg (conn_send, MR_INIT_ACK, 0);
	  free_connection(conn_recv);
	   //	debug(printf("RDMA lib: RECV: Done MR_INI : for wc.slid=%u\n", slid), 2);
	   break;
	case MR_CHUNK:
	  mr_size= conn_recv->recv_msg->data1.mr_size;
	  conn_send = create_connection(comm->cm_id);
	  memcpy(&conn_send->peer_mr, &conn_recv->recv_msg->data.mr, sizeof(conn_send->peer_mr));
	  rdma_read(conn_send, conn_recv->slid, mr_size);
	  free_connection(conn_recv);
	  break;
	case MR_FIN:
	  //	tag = *conn->recv_msg->data1.tag;
	  tag = conn_recv->recv_msg->data1.tag;
	  //	debug(printf("RDMA lib: RECV: Recieved MR_FIN: Tag=%d for wc.slid=%lu\n", tag, slid), 2);
	  conn_send = create_connection(comm->cm_id);
	  send_ctl_msg (conn_send, MR_FIN_ACK, 0);
	  /*Post reveived data*/
	  rdma_msg = (struct RDMA_message*) malloc(sizeof(struct RDMA_message));
	  get_rdma_buff(conn_recv->slid, &rdma_msg->buff, &rdma_msg->size);
	  rdma_msg->tag = tag;
	  append_rdma_msg(conn_recv->slid, rdma_msg);
	  free_connection(conn_recv);
	  break;
	default:
	  debug(printf("Unknown TYPE"), 1);
	  exit(1);
	  break;
	}
      
    } else if (conn_recv->opcode == IBV_WC_SEND) {
      debug(printf("RDMA lib: COMM: Sent IBV_WC_SEND: id=%lu(%lu) | %f\n", conn_recv->count, (uintptr_t)conn_recv, get_dtime()), 2);
      //      free_connection(conn);
      continue;
    } else if (conn_recv->opcode == IBV_WC_RDMA_READ) {
      debug(printf("RDMA lib: COMM: Sent IBV_WC_RDMA_READ: id=%lu(%lu)\n", conn_recv->count, (uintptr_t)conn_recv), 2);
      //      conn_send = create_connection(comm->cm_id);
      send_ctl_msg (conn_recv, MR_CHUNK_ACK, 0);
      continue;
    } else {
      die("unknow opecode.");
      continue;
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


int RDMA_Passive_Finalize(struct RDMA_communicator *comm)
{
  rdma_destroy_id(comm->cm_id);
  rdma_destroy_event_channel(comm->ec);
  return 0;
}
