#include <stdlib.h>
#include <stdio.h>

#include "fdmi_queue.h"
#include "fdmi_mem.h"


struct fdmi_queue* fdmi_queue_create (void)
{
  struct fdmi_queue *q;
  q = (struct fdmi_queue*)fdmi_malloc(sizeof(struct fdmi_queue));
  q->head = NULL;
  q->tail = NULL;
  pthread_mutex_init(&(q->mut), NULL);
  q->length = 0;
  return q;
}

void fdmi_queue_destroy (struct fdmi_queue* q)
{
  void* data;
  pthread_mutex_destroy(&(q->mut));

  /*TODO: Also free each element*/

  while ((data = fdmi_queue_deq(q)) != NULL) {

  }

  fdmi_free(q);
  return;
}

int fdmi_queue_length(struct fdmi_queue *q)
{
  return q->length;
  
}

void fdmi_queue_lock(struct fdmi_queue* q)
{
  int errno;
  if ((errno =  pthread_mutex_lock(&(q->mut))) > 0) {
    fdmi_err ("Query queue lock failed (%s:%s:%d)", __FILE__, __func__, __LINE__);
  }
  return;
}

void fdmi_queue_unlock(struct fdmi_queue* q)
{
  int errno;
  if ((errno =  pthread_mutex_unlock(&(q->mut))) > 0) {
    fdmi_err ("Query queue unlock failed (%s:%s:%d)", __FILE__, __func__, __LINE__);
  }
  return;
}

void fdmi_queue_enq (struct fdmi_queue *q, void *data) 
{
  struct fdmi_queue_data *d;
  d = (struct fdmi_queue_data*)fdmi_malloc(sizeof(struct fdmi_queue_data));
  d->next = NULL;
  d->data = data;

  if (q->head == NULL && q->tail == NULL) {
    q->head = d;
    q->tail = d;
  } else {
    q->tail->next = d;
    q->tail = d;
  }
  q->length++;
  //  if ((data == 0x7fffffffe5c4 || data == NULL) && q == 0x4610150) fdmi_dbgi(1, "aa enq: length:%d q:%p, data:%p", q->length, q, data);
}

void fdmi_queue_lock_enq (struct fdmi_queue *q, void *data) 
{
  fdmi_queue_lock(q);
  fdmi_queue_enq(q, data);
  fdmi_queue_unlock(q);
  return;
}

void* fdmi_queue_deq (struct fdmi_queue *q) {
  struct fdmi_queue_data *deq_d;
  void* data;
  if (q->head == NULL) {
    data = NULL;
  } else {
    data = q->head->data;
    deq_d = q->head;
    q->head = q->head->next;
    fdmi_free(deq_d);
    if (q->head == NULL) {
      q->tail = NULL;
    }
    q->length--;
  }
  //  if (data == 0x7fffffffe5c4 || data == NULL) fdmi_dbgi(1, "aa deq: length:%d, data:%p", q->length, data);
  return data;
}

void* fdmi_queue_lock_deq (struct fdmi_queue *q) 
{
  void* data;
  fdmi_queue_lock(q);
  data = fdmi_queue_deq(q);
  fdmi_queue_unlock(q);
  return data;
}

int fdmi_queue_lock_index(struct fdmi_queue *q, void *data)
{
  void *tmp;
  int tmp_index = 0, index = -1;
  fdmi_queue_lock(q);
  for (tmp = (struct fdmi_query*)fdmi_queue_iterate(q, FDMI_QUEUE_HEAD); tmp != NULL;
       tmp = (struct fdmi_query*)fdmi_queue_iterate(q, FDMI_QUEUE_NEXT)) {
    if (tmp == data) {
      index = tmp_index;
      break;
    }
    tmp_index++;
  }
  fdmi_queue_unlock(q);
  return index;
}

/* void fdmi_queue_init_it (struct fdmi_queue* q) { */
/*   pthread_mutex_lock(&(q->mut)); */
/*   q->cur = q->head; */
/*   return; */
/* } */

/* void fdmi_queue_fin_it (struct fdmi_queue* q) { */
/*   q->cur = NULL; */
/*   pthread_mutex_unlock(&(q->mut)); */
/*   return ; */
/* } */

/* void* fdmi_queue_next(struct fdmi_queue* q) { */
/*   struct fdmi_queue_data *d = q->cur; */
/*   if (d == NULL) { */
/*     return NULL; */
/*   } */
/*   q->cur = q->cur->next; */
/*   return d->data; */
/* } */

void* fdmi_queue_iterate(struct fdmi_queue* q, enum fdmi_queue_iterator_state state)
{
  void* data;
  switch(state) {
  case FDMI_QUEUE_HEAD:
    if (q->head == NULL) {
      data   = NULL;
    } else {
      data   = q->head->data;
      q->ite = q->head->next;
    }
    break;
  case FDMI_QUEUE_NEXT:
    if (q->ite == NULL) {
      /*If remove the data while doing iteration, this branch can happnes*/
      data = NULL;
    } else {
      data = q->ite->data;
      q->ite = q->ite->next;
    }
    break;
  default:
    fdmi_err ("Unknown fdmi_queue_state:%d  (%s:%s:%d)", state, __FILE__, __func__, __LINE__);
  }

  /* if (q->ite != NULL){ */
  /*   fdmi_dbgi(33, "return: %p, next:%p", data, q->ite->data); */
  /* } else { */
  /*   fdmi_dbgi(33, "return: %p", data); */
  /* } */

  return data;
}

void fdmi_queue_lock_remove(struct fdmi_queue* q, void* d)
{
  fdmi_queue_lock(q);
  fdmi_queue_remove(q, d);
  fdmi_queue_unlock(q);
  return;
}


void fdmi_queue_remove(struct fdmi_queue* q, void* d)
{
  struct fdmi_queue_data* cur_d = q->head;
  struct fdmi_queue_data* old_d;
  if (q->head == NULL) {
    /*TODO: Sometime it branches to hered*/
    //    fdmi_err ("Nothing to be removed from Queue (%s:%s:%d)", __FILE__, __func__, __LINE__);
    //    fdmi_dbg ("Nothing to be removed from Queue (%s:%s:%d)", __FILE__, __func__, __LINE__);
    return;
  }
  if (cur_d->data == d){
    q->head = cur_d->next;
    if (q->head == NULL) {
      q->tail = NULL;
      q->ite  = NULL;
    }
    fdmi_free(cur_d);
    q->length--;
    return ;
  }
  old_d = cur_d;
  cur_d = cur_d->next;
  while(cur_d !=NULL){
    if (cur_d->data == d) {
      old_d->next = cur_d->next;
      if (q->tail == cur_d) {
        q->tail = old_d;
      }
      if (q->ite == cur_d) {
	q->ite = cur_d->next;
      }
      fdmi_free(cur_d);
      q->length--;
      // pthread_mutex_unlock(&(q->mut)); /*TODO: ??*/
      return;
    }
    old_d = cur_d;
    cur_d = cur_d->next;
  }
  return;
}


