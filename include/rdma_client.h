#include "rdma_common.h"

/** 
 *  INPUT: comm, param (not used internaly)
 *
 **/
int RDMA_Active_Init(struct RDMA_communicator *comm, struct RDMA_param *param);

/*Not implemented yet*/
int RDMA_Active_Finalize(struct RDMA_communicator *comm);

void rdma_isend_r(void *buf, int size, void* datatype, int dest, int tag, struct RDMA_communicator *rdma_com, struct RDMA_request *request);


