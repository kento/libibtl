#include <pthread.h>


struct _list_data{
  struct _list_data *next;
  void* data;
};
typedef struct _list_data scr_lq_d;

struct _list_q{
  scr_lq_d* head;
  scr_lq_d* tail;
  scr_lq_d* cur;
  pthread_mutex_t mut;
};
typedef struct _list_q scr_lq;

void scr_lq_init(scr_lq *q);
void scr_lq_enq (scr_lq *q, void *data);
void* scr_lq_deq (scr_lq *q);
void scr_lq_remove(scr_lq* q, void *data);
void scr_lq_init_it (scr_lq *q);
void scr_lq_fin_it (scr_lq *q);
void* scr_lq_next(scr_lq* q);





