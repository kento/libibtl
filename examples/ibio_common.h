
#define IBVIO_OP_NOOP        (0)
#define IBVIO_OP_OPEN        (1)
#define IBVIO_OP_WRITE       (2)
#define IBVIO_OP_WRITE_BEGIN (3)
#define IBVIO_OP_WRITE_CHUNK (4)
#define IBVIO_OP_READ        (5)
#define IBVIO_OP_CLOSE       (6)

#define IBVIO_CHUNK_SIZE (64 * 1024 * 1024)

#define IBVIO_DELAYED_WRITE (0)
#define IBVIO_CACHE_READ (0)

struct ibvio_open {
  int fd;
  int stat;
  int count;

  char path[256];
  int flags;
  int mode;
};


void ibvio_serialize_tag(int fd, int op, int *tag);
void ibvio_deserialize_tag(int *fd, int *op, int tag);
