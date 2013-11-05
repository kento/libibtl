#define CHUNK_SIZE (128 * 1024 * 1024)
#define PATH_SIZE (128)
#define NUM_BUFF (2)

#define TEST_BUFF_SIZE (128 * 1024 * 1024)
//#define TEST_BUFF_SIZE (4)

//int id;

struct scr_transfer_ctl {
  char path[PATH_SIZE]; //path length
  uint64_t size;
  int id;
};



