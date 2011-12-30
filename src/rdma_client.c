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
  //  fprintf(stderr, "WCOUNT_S\n");
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


  if (pthread_create(&cq_poller_thread, NULL,(void *)poll_cq, args)) {
    fprintf(stderr, "RDMA lib: SEND: ERROR: pthread create failed @ %s:%d", __FILE__, __LINE__);
    exit(1);
  }

  return 0;
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


/*
static void* poll_cq(struct poll_cq_args* args)
{
  struct ibv_cq *cq;
  struct ibv_wc wc;
  //  struct connection *conn;
  struct RDMA_communicator *comm;
  //  struct RDMA_message *msg;
  double s, e;
  char* ip;

  struct connection* conn;
  struct control_msg cmsg;
  void* ctx;
  char* buff; 
  uint64_t buff_size;
  int tag;

  uint32_t mr_size=0;
  uint64_t sent_size=0;
  char* send_base_addr;
  uint64_t total_sent_size=0;
  int fin_flag=0;

  int* flag = args->flag;
  int mr_index;


  uint32_t *cmt;
  struct ibv_mr *mr;
  uint64_t *data;
  
  
  uint64_t waiting_msg_count = 0;
  uint64_t i;


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
  
  init_ctl_msg(&cmt, &data);
  send_ctl_msg(comm->cm_id->context, MR_INIT, 0, buff_size);
  waiting_msg_count++;
  s = get_dtime();
  
  while (1) {
    //TODO 1: mr and data is NULL
    double ss = get_dtime();
    //    recv_ctl_msg (cmt, data);

    recv_ctl_msg (cmt, data, &conn);

    waiting_msg_count--;
    double ee = get_dtime();

    debug(printf("TIME=========> %f, %s\n", ee - ss, rdma_ctl_msg_type_str(*cmt)),2);
    switch (*cmt)
      {
      case MR_INIT_ACK:
	for (mr_index = 0; mr_index < RDMA_BUF_NUM_C;  mr_index++) {
	  if((sent_size = send_rdma_read_req (conn, mr_index, send_base_addr, buff_size, &total_sent_size, tag)) > 0) {
	    waiting_msg_count++;
	    send_base_addr += sent_size;
	  } else {
	    fin_flag = 1;
	    break;
	  }
	}
	mr_index = mr_index % RDMA_BUF_NUM_C;
	break;
      case MR_CHUNK_ACK:
	if(fin_flag == 0){
	  if((sent_size = send_rdma_read_req (conn, mr_index, send_base_addr, buff_size, &total_sent_size, tag)) > 0) {
	    waiting_msg_count++;
	    send_base_addr += sent_size;
	  } else {
	    fin_flag = 1;;
	    break;
	  }
	  mr_index = (mr_index+ 1) % RDMA_BUF_NUM_C;
	}
	break;
      case MR_FIN_ACK:

	e = get_dtime();
	free(args->msg);
	free(args);
	ip = get_ip_addr("ib0");
	printf("RDMA lib: SEND: %s: send time= %f secs, send size= %lu MB, throughput = %f MB/s\n", ip, e - s, buff_size/1000000, buff_size/(e - s)/1000000.0);
	//	for (i = 0; i < waiting_msg_count-1; i++) {
	//	recv_ctl_msg (cmt, data, &conn);
	  //	}
	finalize_ctl_msg(cmt, data);
	*flag = 1;
	return NULL;
      default:
	debug(printf("Unknown TYPE"), 1);
	return NULL;
      }

  }
  return NULL;
}
*/

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
  uint64_t buff_size;
  int tag;

  uint32_t mr_size=0;
  uint64_t sent_size=0;
  char* send_base_addr;
  uint64_t total_sent_size=0;
  int fin_flag=0;

  int* flag = args->flag;
  int mr_index;

  //  uint32_t *cmt;
  struct ibv_mr *mr;
  //  uint64_t *data;
  
  
  uint64_t waiting_msg_count = 0;
  uint64_t i;


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
  
  //  init_ctl_msg(&cmt, &data);
  conn_send = create_connection(comm->cm_id);
  send_ctl_msg(conn_send, MR_INIT, buff_size);
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
      debug(printf("RDMA lib: SEND: Recv %s: id=%lu(%lu), slid=%u recv_wc time=%f(%f)\n",  rdma_ctl_msg_type_str(conn_recv->cmt), conn_recv->count, conn_recv->id, conn_recv->slid, ee - ss, mm), 2);
      switch (conn_recv->cmt)
	{
	case MR_INIT_ACK:
	  for (mr_index = 0; mr_index < RDMA_BUF_NUM_C;  mr_index++) {
	    conn_send = create_connection(comm->cm_id);
	    if((sent_size = send_rdma_read_req (conn_send, send_base_addr, buff_size, &total_sent_size, tag)) > 0) {
	      waiting_msg_count++;
	      send_base_addr += sent_size;
	    } else {
	      fin_flag = 1;
	      break;
	    }
	  }
	  mr_index = mr_index % RDMA_BUF_NUM_C;
	  free_connection(conn_recv);

	  break;
	case MR_CHUNK_ACK:
	  //	  sleep(3);
	  if(fin_flag == 0){
	    conn_send = create_connection(comm->cm_id);
	    if((sent_size = send_rdma_read_req (conn_send, send_base_addr, buff_size, &total_sent_size, tag)) > 0) {
	      waiting_msg_count++;
	      send_base_addr += sent_size;
	    } else {
	      fin_flag = 1;;
	      break;
	    }
	    mr_index = (mr_index+ 1) % RDMA_BUF_NUM_C;
	  }
	  free_connection(conn_recv);
	  break;
	case MR_FIN_ACK:
	  
	  e = get_dtime();
	  free(args->msg);
	  free(args);
	  ip = get_ip_addr("ib0");
	  printf("RDMA lib: SEND: %s: send time= %f secs, send size= %lu MB, throughput = %f MB/s\n", ip, e - s, buff_size/1000000, buff_size/(e - s)/1000000.0);
	  //	for (i = 0; i < waiting_msg_count-1; i++) {
	  //	recv_ctl_msg (cmt, data, &conn);
	  //	}
	  //	  finalize_ctl_msg(cmt, data);
	  *flag = 1;
	  free_connection(conn_recv);
	  return NULL;
	default:
	  debug(printf("Unknown TYPE"), 1);
	  return NULL;
	}

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

  if (addr_size == *total_sent_size) {
    /*sent all data*/
    /*TODO: Get this function work without a below usleep(10)*/
    //    usleep(10);
    send_ctl_msg (conn, MR_FIN, tag);
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
    send_ctl_msg (conn, MR_CHUNK, mr_size);
    sent_size = mr_size;
  }

  return sent_size;
}


int RDMA_Active_Finalize(struct RDMA_communicator *comm)
{
  rdma_active_finalize(comm);
  return 0;
}




