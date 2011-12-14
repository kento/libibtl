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


static pthread_t cq_poller_thread;
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
  rdma_active_init(comm, param, 1);

  //  for (i = 0; i < RDMA_BUF_NUM_C; i++){ rdma_msg_mr[i] = NULL;}

  return 0;
}


//static void* poll_cq(struct RDMA_communicator* comm)
static void* poll_cq(struct poll_cq_args* args)
{
  struct ibv_cq *cq;
  struct ibv_wc wc;
  //  struct connection *conn;
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

  struct connection *conn;
  uint32_t cmt;
  struct ibv_mr *mr;
  uint64_t data;


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

  send_ctl_msg (comm->cm_id->context, MR_INIT, 0, buff_size);

  s = get_dtime();
  
  while (1) {
    //TODO 1: mr and data is NULL
    //TODO 2: hide an existance of conn within recv_ctl_msg
    recv_ctl_msg (conn, &cmt, &data);
    printf("=======%d, %d\n",cmt, MR_INIT_ACK);
    switch (cmt)
      {
      case MR_INIT_ACK:

	for (mr_index = 0; mr_index < RDMA_BUF_NUM_C; mr_index++) {
	  debug(printf("Recived: Type=%d\n",  cmt), 1);
	  if (sent_size == buff_size) {
	    debug(printf("RDMA lib: SEND: Recieved MR_INIT_ACK => FIN: for tag=%d\n",  tag), 2);
	    /*sent all data*/
	    //		cmsg.type=MR_FIN;
	    //		cmsg.data1.tag=tag;
	    send_ctl_msg (conn, MR_FIN, 0, tag);
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
	    send_ctl_msg (conn, MR_CHUNK, mr_index, mr_size);
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
	  send_ctl_msg (conn, MR_FIN, 0, tag);
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
	  send_ctl_msg (conn, MR_CHUNK, mr_index, mr_size);
	}
	//	    send_control_msg(conn, &cmsg);
	//	    post_receives(conn);
	debug(printf("RDMA lib: SEND: Done MR_CHUNK_ACK: for tag=%d\n",  tag), 2);
	break;
      case MR_FIN_ACK:
	//	    sleep(1);
	debug(printf("RDMA lib: SEND: Recived MR_FIN_ACK: Type=%d\n",  cmt),2);
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
	debug(printf("RDMA lib: SEND: Done MR_FIN_ACK: Type=%d\n",  cmt),1);
	return NULL;
      default:
	debug(printf("Unknown TYPE"), 1);
	return NULL;
      }
  }
  return NULL;
}


int RDMA_Active_Finalize(struct RDMA_communicator *comm)
{
  rdma_active_finalize(comm);
  return 0;
}




