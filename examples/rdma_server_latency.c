#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "ibtls.h"
#include "common.h"


int main(int argc, char **argv) {
  struct RDMA_communicator comm;
  double latency;

  RDMA_Passive_Init(&comm);

  while (1) {
    latency = RDMA_Latency(RDMA_ANY_SOURCE, &comm);
    fprintf(stderr, "Latency: %.10f (%.10f)\n", latency);
    sleep(1);
  }
  return 0;
}

