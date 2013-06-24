#include "rdma_common.h"


//int alloc_size = 0;

#define RDMA_ANY_SOURCE (-1)
#define RDMA_ANY_TAG (-2)


int RDMA_Passive_Init(struct RDMA_communicator *comm);
//int RDMA_Irecvr(char** buff, uint64_t* size, int* tag, struct RDMA_communicator *comm);
//int RDMA_Recvr(char** buff, uint64_t* size, int* tag, struct RDMA_communicator *comm);
int RDMA_Passive_Finalize(struct RDMA_communicator *comm);
//void RDMA_show_buffer(void);
int rdma_irecv_r (void *buf, int size, void* datatype, int source, int tag, struct RDMA_communicator *rdma_com, struct RDMA_request *request);
int rdma_irecv_r_silent (void *buf, int size, void* datatype, int source, int tag, struct RDMA_communicator *rdma_com, struct RDMA_request *request);
int rdma_irecv_r_silent_offset (void *buf, int offset, int size, void* datatype, int source, int tag, struct RDMA_communicator *rdma_com, struct RDMA_request *request);

long double rdma_latency_r (int source, struct RDMA_communicator *rdma_com);
//int rdma_wait(struct RDMA_request *request);
//int rdma_irecv_r (void *buf, int size, void* datatype, int source, int tag, RDMA_communicator rdma_com, RDMA_request *request);
int rdma_iprobe(int source, int tag, struct RDMA_communicator *rdma_com);
int rdma_reqid(struct RDMA_communicator *rdma_com, int tag) ;
