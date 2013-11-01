#ifndef FDMI_QUEUE_H
#define FDMI_QUEUE_H

#include <pthread.h>

enum fdmi_queue_iterator_state {
  FDMI_QUEUE_HEAD,
  FDMI_QUEUE_NEXT
};

struct fdmi_queue_data{
  struct fdmi_queue_data *next;
  void* data;
};

struct fdmi_queue{
  struct fdmi_queue_data* head;
  struct fdmi_queue_data* tail;
  struct fdmi_queue_data* ite;
  pthread_mutex_t mut;
  int length;
};

struct fdmi_queue* fdmi_queue_create (void);
void fdmi_queue_destroy (struct fdmi_queue *q);
int fdmi_queue_length(struct fdmi_queue *q);
void fdmi_queue_enq (struct fdmi_queue *q, void *data);
void fdmi_queue_lock_enq (struct fdmi_queue *q, void *data);
void* fdmi_queue_deq (struct fdmi_queue *q);
void* fdmi_queue_lock_deq (struct fdmi_queue *q);
void fdmi_queue_remove(struct fdmi_queue* q, void *data);
void* fdmi_queue_iterate(struct fdmi_queue* q, enum fdmi_queue_iterator_state state);

#endif
