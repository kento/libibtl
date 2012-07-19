#include "rdma_client.h"
#include "rdma_server.h"


void RDMA_Free(void* ptr);
void* RDMA_Alloc (size_t size);

int RDMA_Send(void *buf, int size, void* datatype, int dest, int tag, struct RDMA_communicator *rdma_com);
int RDMA_Isend(void *buf, int size, void* datatype, int dest, int tag, struct RDMA_communicator *rdma_com, struct RDMA_request *request);

int RDMA_Recv(void *buf, int size, void* datatype, int source, int tag, struct RDMA_communicator *rdma_com);
int RDMA_Irecv(void *buf, int size, void* datatype, int source, int tag, struct RDMA_communicator *rdma_com, struct RDMA_request *request);
int RDMA_Irecv_silent(void *buf, int size, void* datatype, int source, int tag, struct RDMA_communicator *rdma_com, struct RDMA_request *request);

double RDMA_Latency(int source, struct RDMA_communicator *rdma_com);

int RDMA_Wait(struct RDMA_request *request);
int RDMA_Iprobe(int source, int tag, struct RDMA_communicator *rdma_com);
int RDMA_Reqid(struct RDMA_communicator *rdma_com, int index);
