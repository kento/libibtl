#include "rdma_common.h"

int RDMA_Active_Init(struct RDMA_communicator *comm, struct RDMA_param *param);
//int RDMA_Sendr (char *buff, uint64_t size, int tag, struct RDMA_communicator *comm);
//int RDMA_Sendr_ns (char *buff, uint64_t size, int tag, struct RDMA_communicator *comm);
//int RDMA_Isendr(char *buff, uint64_t size, int tag,  int *flag, struct RDMA_communicator *comm);
void rdma_isend_r(void *buf, int size, void* datatype, int dest, int tag, struct RDMA_communicator *rdma_com, struct RDMA_request *request);
//int RDMA_Wait(int *flag);
int RDMA_Active_Finalize(struct RDMA_communicator *comm);
