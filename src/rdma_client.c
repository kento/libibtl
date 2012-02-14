#include "common.h"
#include "rdma_client.h"
#include "assert.h"
#include "arpa/inet.h"
#include "time.h"


struct poll_cq_args{
  void *buf;
  int size;
  void* datatype;
  int dest;
  int tag;
  struct RDMA_communicator *comm;
  sem_t *is_rdma_completed;
};


static void* poll_cq(struct poll_cq_args* args);
static void set_envs (void);
static int send_rdma_read_req (struct connection* conn, char* addr, uint64_t size, uint64_t* total_sent_size, int tag);


static pthread_t cq_poller_thread;


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

/*
int RDMA_Wait (int *flag) {
  //  int count = 0;
  while (*flag == 0) {
    usleep(100 * 1000);
    //    count++;
  };
  //  fprintf(stderr, "WCOUNT: %d\n", count);
  return 0;
}
*/

/*
int RDMA_Sendr (char *buff, uint64_t size, int tag, struct RDMA_communicator *comm)
{
  int flag=0;
  RDMA_Isendr(buff, size, tag, &flag, comm);
  //  int count = 0;
  RDMA_Wait (&flag) ;

  //  fprintf(stderr, "WCOUNT_S: %d\n", count);
  return 0;
}
*/
 /*
int RDMA_Sendr_ns (char *buff, uint64_t size, int tag, struct RDMA_communicator *comm)
{
  int flag=0;

  RDMA_Isendr(buff, size, tag, &flag, comm);
  while (flag == 0) {
    usleep(1);
  } ;
  //  fprintf(stderr, "WCOUNT_S\n");
  //  fprintf(stderr, "WCOUNT_S: %d\n", count);
  return 0;
  }*/

/*
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

  if (pthread_create(&cq_poller_thread, NULL,(void *)poll_cq, args)) {
    fprintf(stderr, "RDMA lib: SEND: ERROR: pthread create failed @ %s:%d", __FILE__, __LINE__);
    exit(1);
  }

  return 0;
  }*/

void rdma_isend_r(void *buf, int size, void* datatype, int dest, int tag, struct RDMA_communicator *rdma_com, struct RDMA_request *request)
{
  struct poll_cq_args *args = (struct poll_cq_args*)malloc(sizeof(struct poll_cq_args));
  pthread_t thread;
  args->buf = buf;
  args->size = size;
  args->datatype = datatype;
  args->dest = dest;
  args->tag = tag;
  args->comm = rdma_com;
  sem_init(&(request->is_rdma_completed), 0, 0);
  args->is_rdma_completed  = &(request->is_rdma_completed);

  if (pthread_create(&thread, NULL,(void *)poll_cq, args)) {
    fprintf(stderr, "RDMA lib: SEND: ERROR: pthread create failed @ %s:%d", __FILE__, __LINE__);
    exit(1);
  }
  
  if (pthread_detach(thread)) {
    fprintf(stderr, "RDMA lib: SEND: ERROR: pthread detach failed @ %s:%d", __FILE__, __LINE__);
    exit(1);
    }

  //  sem_wait(&request->is_rdma_completed);
  return;
}


//static int run(int argc, char **argv)
//int RDMA_Connect(struct RDMA_communicator *comm, struct RDMA_param *param)
int RDMA_Active_Init(struct RDMA_communicator *comm, struct RDMA_param *param)
{
  int i ;

  set_envs();
  rdma_active_init(comm, param, RDMA_BUF_NUM_C);

  //  for (i = 0; i < RDMA_BUF_NUM_C; i++){ rdma_msg_mr[i] = NULL;}

  return 0;
}


static void* poll_cq(struct poll_cq_args* args)
{
  struct ibv_cq *cq;
  struct ibv_wc wc;
  //  struct connection *conn;
  struct RDMA_communicator *comm;
  //  struct RDMA_message *msg;
  double s, e;
  char* ip;

  struct connection* conn_send;
  struct connection* conn_recv;
  int num_entries;
  struct control_msg cmsg;
  void* ctx;
  char* buff; 
  uint32_t buff_size;
  int tag;

  uint32_t mr_size=0;
  uint64_t sent_size=0;
  char* send_base_addr;
  uint64_t total_sent_size=0;
  int fin_flag=0;

  int mr_index;

  //  uint32_t *cmt;
  struct ibv_mr *mr;
  //  uint64_t *data;
  
  uint64_t waiting_msg_count = 0;
  uint64_t i;


  /*
    args: (+) used (-) use in future
      + args->buf = buf;
      + args->size = size;
      - args->datatype = datatype;
      - args->dest = dest;
      + args->tag = tag;
      - args->comm = rdma_com;
      + args->is_rdma_completed;
  */

  comm = args->comm;
  struct rdma_read_request_entry rrre;
  struct ibv_mr *passive_mr;
  //TODO: allocation ID
  rrre.id = 1;
  //TODO: allocate order
  rrre.order = 2;
  rrre.tag = args->tag;
  //TODO: change the member name of passive_mr because passive_mr is used in alos active side.
  passive_mr = reg_mr(args->buf, args->size);
  memcpy(&rrre.mr, passive_mr, sizeof(struct ibv_mr));
  rrre.passive_mr =  passive_mr;
  printf("addr=%p, length=%lu, rkey=%lu\n", rrre.mr.addr, rrre.mr.length, rrre.mr.rkey);
  rrre.is_rdma_completed = args->is_rdma_completed;
  //   fprintf(stderr,":= %p\n", &(rrre.is_rdma_completed));

  conn_send = create_connection(comm->cm_id);

  //To free resources after the ack.
  conn_send->active_rrre = &rrre;
  
  send_ctl_msg(conn_send, MR_INIT, &rrre);
  waiting_msg_count++;
  s = get_dtime();
  
  while (1) {
    double mm,ee,ss;
    //TODO 1: mr and data is NULL
    ss = get_dtime();
    num_entries = recv_wc(1, &conn_recv);
    waiting_msg_count--;
    mm = ss - ee;
    ee = get_dtime();
    debug(printf("RDMA lib: SEND: recv_wc time = %f(%f) (%s)\n", ee - ss, mm, ibv_wc_opcode_str(conn_recv->opcode)),1);

    /*Check which request was successed*/
    if (conn_recv->opcode == IBV_WC_RECV) {
      struct rdma_read_request_entry *active_rrre;
      debug(printf("RDMA lib: SEND: Recv REQ: id=%lu(%lu), slid=%u recv_wc time=%f(%f)\n",  conn_recv->count, conn_recv->id, conn_recv->slid, ee - ss, mm), 2);
    
      active_rrre = conn_recv->active_rrre;
      sem_post(active_rrre->is_rdma_completed);
      
      /*TODO: more sophisticated free*/
      conn_recv->active_rrre = NULL;
      conn_recv->passive_rrre = NULL;
      free_connection(conn_recv);
      /*----------------*/
      return;
    } else if (conn_recv->opcode == IBV_WC_SEND) {
      debug(printf("RDMA lib: SEND: Sent IBV_WC_SEND: id=%lu(%lu) recv_wc time=%f(%f)\n", conn_recv->count, (uintptr_t)conn_recv, ee - ss, mm), 2);
      continue;
    } else {
      die("unknow opecode.");
      continue;
    }
  }
  return NULL;
}


static int send_rdma_read_req (struct connection* conn, char* addr, uint64_t addr_size, uint64_t* total_sent_size, int tag)
{
  uint64_t sent_size = 0;
  uint32_t mr_size;

  printf("addr_size: %lu\n", addr_size );
  if (addr_size == *total_sent_size) {
    /*sent all data*/
    /*TODO: Get this function work without a below usleep(10)*/
    //    usleep(10);
    //NEW:    send_ctl_msg (conn, MR_FIN, tag);
    return 0;
  } else {
    /*not sent all data yet*/
    if (*total_sent_size + rdma_buf_size > addr_size) {
      mr_size = (uint32_t)(addr_size - *total_sent_size);
    } else {
      mr_size = rdma_buf_size;
    }

    register_rdma_msg_mr(conn, addr, mr_size);
    *total_sent_size += (uint64_t)mr_size;
    //NEW:    send_ctl_msg (conn, MR_CHUNK, mr_size);
    sent_size = mr_size;
  }

  return sent_size;
}


int RDMA_Active_Finalize(struct RDMA_communicator *comm)
{
  rdma_active_finalize(comm);
  return 0;
}




