#include <pthread.h>


struct _list_data{
  struct _list_data *next;
  void* data;
};
typedef struct _list_data lq_d;

struct _list_q{
  lq_d* head;
  lq_d* tail;
  lq_d* cur;
  pthread_mutex_t mut;
};
typedef struct _list_q lq;

void lq_init(lq *q);
void lq_enq (lq *q, void *data);
void* lq_deq (lq *q);
void lq_remove(lq* q, void *data);
void lq_init_it (lq *q);
void lq_fin_it (lq *q);
void* lq_next(lq* q);





