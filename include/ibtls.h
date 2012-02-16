#include "rdma_client.h"
#include "rdma_server.h"

int RDMA_Send(void *buf, int size, void* datatype, int dest, int tag, struct RDMA_communicator *rdma_com);
int RDMA_Isend(void *buf, int size, void* datatype, int dest, int tag, struct RDMA_communicator *rdma_com, struct RDMA_request *request);

int RDMA_Recv(void *buf, int size, void* datatype, int source, int tag, struct RDMA_communicator *rdma_com);
int RDMA_Irecv(void *buf, int size, void* datatype, int source, int tag, struct RDMA_communicator *rdma_com, struct RDMA_request *request);
int RDMA_Wait(struct RDMA_request *request);
