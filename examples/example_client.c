#include <stdio.h>
#include <stdlib.h>

#include "fdmi.h"


int main(int argc, char **argv)
{
  FMI_Request req;
  FMI_Status stat;

  char server_hostname[256];
  char buff[256];
  int  pid;
  int  server_hostid = 0;
  int  tag = 5;
  
  if (argc != 2) {
    fdmi_err("example_client <server_hostname>");
  }
  sprintf(server_hostname, "%s", argv[1]);

  /* Initialization */
  fdmi_verbs_init(&argc, &argv);

  /**
   *  Connect to a server(server_hostname).
   *  Now I can exchange messages via id (server_hostid) 
   *  Note:
   *     Any server_hostid can be assigned to server_hostname.
   *     But the server_hostid must be a unique number amoung servers.
   */
  fdmi_verbs_connect(server_hostid, server_hostname); /*Mapping: server_hostid -> server_hostname*/
  printf("Connected: server_hostname=%s, server_hostid=%d\n", server_hostname, server_hostid);

  gethostname(buff, sizeof(buff));
  
  /* Send my hostname to the connected server*/
  fdmi_verbs_isend(buff, sizeof(buff), FMI_BYTE, server_hostid, tag, FMI_COMM_WORLD, &req);
  fdmi_verbs_wait(&req, &stat);
  printf("Send    : msg=%s\n", buff);

  /* Receive assigned id by the connected server */
  fdmi_verbs_irecv(buff, 1, FMI_INT, server_hostid, FMI_ANY_TAG, FMI_COMM_WORLD, &req);
  fdmi_verbs_wait(&req, &stat);
  printf("Recv    : assigned_id=%s\n", buff);
  
  fdmi_verbs_finalize();
  return 0;
}
