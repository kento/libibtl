#ifndef _RDMA_COMMON
#define _RDMA_COMMON

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <rdma/rdma_cma.h>
#include <semaphore.h>

#include "fdmi_config.h"
#include "fdmi_pmtv_common.h"
#include "fdmi_datatype.h"
#include "fdmi_proc_man.h"


#ifndef RDMA_PORT
#define RDMA_PORT 10150
#endif


struct fdmi_failure_status {
  int length; /*NOT IN USE*/
  int pranks[256]; /*TODO: 256 is enough*/
};
typedef struct fdmi_failure_status FMI_Failure_Status;


struct fdmi_post_send_param {
  int rank;
  int tag;
  const void* addr;
  int count;
  struct fdmi_datatype dtype;
  struct fdmi_communicator *comm;
  volatile int *request_flag;
  struct fdmi_request *request;
  int op;
};

struct fdmi_post_recv_param {
  int rank;
  int tag;
  void* addr;
  int count;
  struct fdmi_datatype dtype;
  struct fdmi_communicator *comm;
  //  sem_t *request_sem;
  //  pthread_mutex_t *request_mtx;
  volatile int *request_flag;
  struct fdmi_request *request;
  int op;
};


enum fdmi_option {
  FDMI_ABORT = (1 << 0),   /*On a failure, a message with FDMI_ABORT flag is discarded*/
  FDMI_FORCE = (1 << 1),   /*Even on a failure, a message with FDMI_FOREC can be sent/recieved*/
  FDMI_PRESV = (1 << 2)    /*Even on a failure, a meesage with FDMI_PRESV is not flushed and is preserved in a recieve queue*/
};

//extern FMI_Comm *FMI_COMM_WORLD;
//FMI_Comm	*FMI_COMM_WORLD;


int	fdmi_verbs_init(int *argc, char ***argv);
struct fdmi_connection* fdmi_verbs_connect(int rank, char *hostname);

int	fdmi_verbs_comm_size(struct fdmi_communicator *comm, int *size);
int	fdmi_verbs_comm_vrank(struct fdmi_communicator *comm, int *vrank);
int	fdmi_verbs_isend (const void* buf, int count, struct fdmi_datatype dataype, int dest, int tag, struct fdmi_communicator *comm, struct fdmi_request* request);
int	fdmi_verbs_irecv(const void* buf, int count, struct fdmi_datatype dataype,  int source, int tag, struct fdmi_communicator *comm, struct fdmi_request* request);
int     fdmi_verbs_test(struct fdmi_request *request, struct fdmi_status *staus);
int	fdmi_verbs_wait(struct fdmi_request* request, struct fdmi_status *status);
int     fdmi_verbs_iprobe(int source, int tag, struct fdmi_communicator* comm, int *flag, struct fdmi_status *status);
int     fdmi_verbs_barrier_dignose(void);

int	fdmi_verbs_get_failure_status(struct fdmi_failure_status *fstatus);
int	fdmi_verbs_accept_failure_status(struct fdmi_failure_status *fstatus);

int	fdmi_get_state(void);
void	fdmi_set_state(enum fdmi_state state);
int	fdmi_inform_loop (int prank, int loop);

void show_query_qp_content(void);

#endif //of _RDMA_COMMON


