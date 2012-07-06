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

static void set_envs () {
  /*Letf empty for future requirement*/
  return;
}

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

  /*Wait untile previous send post request is finished and "rdma_isend_r" call is completed*/
  pthread_mutex_lock(&(rdma_com->post_mutex));

  if (pthread_create(&thread, NULL,(void *)poll_cq, args)) {
    fprintf(stderr, "RDMA lib: SEND: ERROR: pthread create failed @ %s:%d", __FILE__, __LINE__);
    exit(1);
  }
  
  if (pthread_detach(thread)) {
    fprintf(stderr, "RDMA lib: SEND: ERROR: pthread detach failed @ %s:%d", __FILE__, __LINE__);
    exit(1);
  }
  
  return;
}

int RDMA_Active_Init(struct RDMA_communicator *comm, struct RDMA_param *param)
{
  set_envs();

  rdma_active_init(comm, param);
  return 0;
}

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void* poll_cq(struct poll_cq_args* args)
{
  struct RDMA_communicator *comm;
  struct connection* conn_send;
  struct connection* conn_recv;

  int num_entries;

  double s, e;
  double mm,ee,ss, te, ts;
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
  struct rdma_read_request_entry rrre;
  struct ibv_mr *passive_mr;

  comm = args->comm;

  //TODO: allocation ID
  rrre.id = 1;
  //TODO: allocate order
  rrre.order = 2;
  rrre.tag = args->tag;
  //TODO: change the member name of passive_mr because passive_mr is used in alos active side.

  passive_mr = reg_mr(args->buf, args->size);
  memcpy(&rrre.mr, passive_mr, sizeof(struct ibv_mr));
  ts = get_dtime();
  rrre.passive_mr =  passive_mr;
  rrre.is_rdma_completed = args->is_rdma_completed;
  conn_send = create_connection(comm->cm_id);

  //To free resources after the ack.
  conn_send->active_rrre = &rrre;
  ee = get_dtime();

  send_ctl_msg(conn_send, MR_INIT, &rrre);

  /* Now we have posted the request, a next asynchronous send function can be called
   * We can safely unlock the post_mutex here.
   */
  pthread_mutex_unlock(&(comm->post_mutex));

  s = get_dtime();
  while (1) {
    //TODO 1: mr and data is NULL
    ss = get_dtime();
    num_entries = recv_wc(1, &conn_recv);
    mm = ss - ee;
    ee = get_dtime();
    debug(printf("RDMA lib: SEND: recv_wc time = %f(%f) (%s)\n", ee - ss, mm, ibv_wc_opcode_str(conn_recv->opcode)),10);
    /*Check which request was successed*/
    if (conn_recv->opcode == IBV_WC_RECV) {
      struct rdma_read_request_entry *active_rrre;

      te = get_dtime();    
      active_rrre = conn_recv->active_rrre;

      /*I dont know the best loation of sem_post()*/

      /*Option: 1 => error when using nbcr_finilize()*/
      //sem_post(args->is_rdma_completed);

      /*TODO: more sophisticated free*/
      dereg_mr(conn_recv->active_rrre->passive_mr);

      /*Option: 2 => error when using nbcr_finilize()*/
      // sem_post(args->is_rdma_completed);

      conn_recv->active_rrre = NULL;
      conn_recv->passive_rrre = NULL;
      free_connection(conn_recv);
      /*----------------*/

      /*Option: 3*/
      sem_post(args->is_rdma_completed);

      debug(fprintf(stderr, "RDMA lib: SEND: Recv REQ: id=%lu, count=%lu,  slid=%u recv_wc time=%f(%f) total_time=%f\n",   conn_recv->id, conn_recv->count, conn_recv->slid, ee - ss, mm, te - ts), 2);

      return;
    } else if (conn_recv->opcode == IBV_WC_SEND) {
      debug(printf("RDMA lib: SEND: Sent IBV_WC_SEND: id=%lu(%lu) recv_wc time=%f(%f)\n", conn_recv->count, (uintptr_t)conn_recv, ee - ss, mm), 20);
      continue;
    } else {
      die("unknow opecode.");
      continue;
    }
  }
  return NULL;
}

int RDMA_Active_Finalize(struct RDMA_communicator *comm)
{
  rdma_active_finalize(comm);
  return 0;
}
