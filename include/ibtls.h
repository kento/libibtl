#include "rdma_client.h"
#include "rdma_server.h"



/**
 * Similar to MPI_Send(...), but datatype, dest are not used internaly
 */
int RDMA_Send(void *buf, int size, void* datatype, int dest, int tag, struct RDMA_communicator *rdma_com);
/**
 * Similar to MPI_Isend(...), but datatype, dest are not used internaly
 */
int RDMA_Isend(void *buf, int size, void* datatype, int dest, int tag, struct RDMA_communicator *rdma_com, struct RDMA_request *request);

/**
 * Similar to MPI_Recv(...), but datatype is not used internaly
 */
int RDMA_Recv(void *buf, int size, void* datatype, int source, int tag, struct RDMA_communicator *rdma_com);

/**
 * Similar to MPI_Irecv(...), but datatype is not used internaly
 */
int RDMA_Irecv(void *buf, int size, void* datatype, int source, int tag, struct RDMA_communicator *rdma_com, struct RDMA_request *request);

/**
 * Similar to MPI_Irecv(...), but datatype is not used internaly, and this function does not notify the completion to its sender side
 * 
 */
int RDMA_Irecv_silent(void *buf, int size, void* datatype, int source, int tag, struct RDMA_communicator *rdma_com, struct RDMA_request *request);

/**
 * Same as RDMA_Irecv_silent(...)
 * Read sender side memory from send_buf + offset, and write to local buff from recv_buf + offset
 */
int RDMA_Irecv_silent_offset (void *buf, int offset, int size, void* datatype, int source, int tag, struct RDMA_communicator *rdma_com, struct RDMA_request *request);


/**
 *  Similart to MPI_Wait(...)
 */
int RDMA_Wait(struct RDMA_request *request);


/**
 * Allocated date by this function is never de-registrated unless freed by RDMA_Free(...)
*/
void* RDMA_Alloc (size_t size);

/**
 * Free an memory allocated by RDMA_Alloc(...);
 */
void RDMA_Free(void* ptr);


/*For debugging*/
double RDMA_Latency(int source, struct RDMA_communicator *rdma_com);
int RDMA_Iprobe(int source, int tag, struct RDMA_communicator *rdma_com);
int RDMA_Reqid(struct RDMA_communicator *rdma_com, int tag);
