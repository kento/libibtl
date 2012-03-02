
struct psockaddr {
  char addr[16];
  int port;  
};

void join_passive_pool(char *addr, int port);

void find_passive_host(struct psockaddr *psa, int mode);
void find_passive_host_rd (struct psockaddr *psa);

