#include <list_queue.h>
#include <stdlib.h>
#include <stdio.h>


void lq_init (lq *q) 
{
  q->head = NULL;
  q->tail = NULL;
  pthread_mutex_init(&(q->mut), NULL);
}

void lq_enq (lq *q, void *data) 
{
  lq_d	*d;

  pthread_mutex_lock(&(q->mut));
  d	  = (lq_d*) malloc(sizeof(lq_d));
  d->next = NULL;
  d->data = data;

  if (q->head == NULL && q->tail == NULL) {
    q->head	  = d;
    q->tail	  = d;
  } else {
    q->tail->next = d;
    q->tail	  = d;
  }
  pthread_mutex_unlock(&(q->mut));
  return;
}

void* lq_deq (lq *q) {
  pthread_mutex_lock(&(q->mut));
  lq_d	*deq_d;
  void*	 data;
  if (q->head == NULL) {
    return NULL;
  } else {
    data      = q->head->data;
    deq_d     = q->head;
    q->head   = q->head->next;
    free(deq_d);
    if (q->head == NULL) {
      q->tail = NULL;
    }
  }
  pthread_mutex_unlock(&(q->mut));
  return data;
}

void lq_init_it (lq* q) {
  pthread_mutex_lock(&(q->mut));
  q->cur = q->head;
  return;
}

void lq_fin_it (lq* q) {
  q->cur = NULL;
  pthread_mutex_unlock(&(q->mut));
  return;
}

void* lq_next(lq* q) {
  lq_d	*d = q->cur;
  if (d == NULL) {
    return NULL;
  }
  q->cur = q->cur->next;
  return d->data;
}


/*!! This function is note thread safe !!*/
void lq_remove(lq* q, void* d) {
  lq_d* cur_d = q->head;
  lq_d* old_d;
  if (cur_d->data == d){
    q->head   = cur_d->next;
    if (q->head == NULL) {
      q->tail = NULL;
    }
    return;
  }

  old_d		  = cur_d;
  cur_d		  = cur_d->next;
  while(cur_d !=NULL){
    if (cur_d->data == d) {
      old_d->next = cur_d->next;
      if (q->tail == cur_d) {
        q->tail	  = old_d;
      }
      free(cur_d);
      pthread_mutex_unlock(&(q->mut));
      return;
    }
    old_d = cur_d;
    cur_d = cur_d->next;
  }
  pthread_mutex_unlock(&(q->mut));
  return;
}

