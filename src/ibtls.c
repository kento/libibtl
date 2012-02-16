#include "ibtls.h"



int RDMA_Send(void *buf, int size, void* datatype, int dest, int tag, struct RDMA_communicator *rdma_com)
{
  struct RDMA_request request;
  int recv_size = 0;

  rdma_isend_r(buf, size, datatype, dest, tag, rdma_com, &request);
  rdma_wait(&request);
  return recv_size;
}


int RDMA_Isend(void *buf, int size, void* datatype, int dest, int tag, struct RDMA_communicator *rdma_com, struct RDMA_request *request)
{
  rdma_isend_r(buf, size, datatype, dest, tag, rdma_com, request);
  return 1;
}


int RDMA_Recv(void *buf, int size, void* datatype, int source, int tag, struct RDMA_communicator *rdma_com)
{
  struct RDMA_request request;
  int recv_size = 0;

  rdma_irecv_r (buf, size, datatype, source, tag, rdma_com, &request);
  rdma_wait(&request);
  return recv_size;
}


int RDMA_Irecv(void *buf, int size, void* datatype, int source, int tag, struct RDMA_communicator *rdma_com, struct RDMA_request *request)
{
  rdma_irecv_r (buf, size, datatype, source, tag, rdma_com, request);
  return 1;
}

int RDMA_Wait(struct RDMA_request *request)
{
  int recv_size = 0;
  rdma_wait(request);
  return recv_size;
}


