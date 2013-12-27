#include <stdlib.h>
#include <stdio.h>
//#include <unistd.h>
//#include <fcntl.h>
//#include <sys/stat.h>
//#include <errno.h>

#include "fdmi.h"


int main(int argc, char **argv) 
{
  FMI_Request req;
  FMI_Status stat;
  int flag;
  char buff[256];

  fdmi_verbs_init(&argc, &argv);

  while (1) {
    fdmi_verbs_iprobe(FMI_ANY_SOURCE, FMI_ANY_TAG, FMI_COMM_WORLD, &flag, &stat);
    if (flag) {
      fprintf(stderr, "Receved message: source=%d , tag=%d\n", stat.FMI_SOURCE, stat.FMI_TAG);

      fdmi_verbs_irecv(buff, sizeof(buff), FMI_BYTE, stat.FMI_SOURCE, stat.FMI_TAG, FMI_COMM_WORLD, &req);
      fdmi_verbs_wait(&req, NULL);
      fprintf(stderr, "               : hostname=%s, assigned_id=%d\n", buff, stat.FMI_SOURCE);

      sprintf(buff, "%d", stat.FMI_SOURCE);

      fdmi_verbs_isend(buff, 1, FMI_INT, stat.FMI_SOURCE, stat.FMI_TAG, FMI_COMM_WORLD, &req);
      fdmi_verbs_wait(&req, NULL);   
      fprintf(stderr, "Send    message: destination_id(assigned_id)=%d\n",  stat.FMI_SOURCE);
    }
  }
}


