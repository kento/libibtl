#ifndef _FDMI_CONST
#define _FDMI_CONST

enum fdmi_err {
  FMI_SUCCESS,
  FMI_DEST_FAILURE,
  FMI_FAILURE,
  FMI_RECOVERY
};

struct fdmi_status {
  int FMI_SOURCE;
  int FMI_TAG;
  int FMI_ERROR;
  int _count;
  int _canceled;
};
typedef struct fdmi_request FMI_Status;

struct fdmi_request {
  int prank;
  struct fdmi_communicator *comm;
  int tag;
  /**
   * TODO: Use union
   *   For now, I leave it as struct for compatibility
   **/
  volatile int request_flag; /*for ibverbs*/
};
typedef struct fdmi_request FMI_Request;

#endif
