#define CHUNK_SIZE (5 * 1024 * 1024)
#define PATH_SIZE (128)
#define NUM_BUFF (2)

//int id;

struct scr_transfer_ctl {
  char path[PATH_SIZE]; //path length
  uint64_t size;
  int id;
};



