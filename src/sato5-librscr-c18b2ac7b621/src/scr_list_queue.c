#include <scr_list_queue.h>
#include <stdlib.h>
#include <stdio.h>


void scr_lq_init (scr_lq *q) 
{
  q->head = NULL;
  q->tail = NULL;
  pthread_mutex_init(&(q->mut), NULL);
}

void scr_lq_enq (scr_lq *q, void *data) 
{
  pthread_mutex_lock(&(q->mut));
  scr_lq_d *d;
  d = (scr_lq_d*) malloc(sizeof(scr_lq_d));
  d->next = NULL;
  d->data = data;
  //  fprintf(stderr, "queeu: %p\n", d);
  if (q->head == NULL && q->tail == NULL) {
    //    fprintf(stderr, "create: %p\n", d);
    q->head = d;
    q->tail = d;
  } else {
    //    fprintf(stderr, "connect: %p after %p\n", d, q->tail);
    q->tail->next = d;
    q->tail = d;
  }
  //  debug(fprintf(stderr, "RDMA lib: COMM: Qued: %p\n", data),2);
  /*
  scr_lq_d *dd;
  dd = q->head;
  fprintf(stderr, "Q: ");
  while (dd != NULL) {
    fprintf(stderr, "%p ", dd);
    dd = dd->next;
  }
  fprintf(stderr, "\n");
  */
  pthread_mutex_unlock(&(q->mut));
  return;
}

void* scr_lq_deq (scr_lq *q) {
  pthread_mutex_lock(&(q->mut));
  scr_lq_d *deq_d;
  void* data;
  if (q->head == NULL) {
    return NULL;
  } else {
    data = q->head->data;
    deq_d = q->head;
    q->head = q->head->next;
    free(deq_d);
    if (q->head == NULL) {
      q->tail = NULL;
    }
  }
  pthread_mutex_unlock(&(q->mut));
  return data;
}

void scr_lq_init_it (scr_lq* q) {
  pthread_mutex_lock(&(q->mut));
  q->cur = q->head;
  return;
}

void scr_lq_fin_it (scr_lq* q) {
  q->cur = NULL;
  pthread_mutex_unlock(&(q->mut));
  return ;
}


void* scr_lq_next(scr_lq* q) {
  scr_lq_d *d = q->cur;
  if (d == NULL) {
    return NULL;
  }
  q->cur = q->cur->next;
  return d->data;
}



void scr_lq_remove(scr_lq* q, void* d) {
  scr_lq_d* cur_d = q->head;
  scr_lq_d* old_d;
  pthread_mutex_lock(&(q->mut));
  if (cur_d->data == d){
    //    printf("%p = %p\n", cur_d, d);
    q->head = cur_d->next;
    if (q->head == NULL) {
      q->tail = NULL;
    }
    //    debug(fprintf(stderr, "RDMA lib: COMM: Rmvd: %p\n", d), 2);
    pthread_mutex_unlock(&(q->mut));
    return ;
  }

  old_d = cur_d;
  cur_d = cur_d->next;
  while(cur_d !=NULL){
    if (cur_d->data == d) {
      //      printf("%p = %p\n", cur_d, d);
      old_d->next = cur_d->next;
      if (q->tail == cur_d) {
	q->tail = old_d;
      }
      free(cur_d);
      pthread_mutex_unlock(&(q->mut));
      return;
    }
    //    printf("%p != %p\n", cur_d, d);
    old_d = cur_d;
    cur_d = cur_d->next;
  }
  pthread_mutex_unlock(&(q->mut));
  return;
}


/*
int main() {
  lq q;
  int a, b, c;
  int *v;
  a = 1;
  b = 2;
  c = 3;
  
  lq_init(&q);
  lq_enq(&q, &a);
  printf("enq: %d\n", a);
  lq_enq(&q, &b);
  printf("enq: %d\n", b);

  v = lq_deq(&q);
  printf("deq: %d\n", *v);
  v = lq_deq(&q);
  printf("deq: %d\n", *v);


  lq_enq(&q, &c);
  printf("enq: %d\n", c);
  lq_enq(&q, &c);
  printf("enq: %d\n", c);
  v = lq_deq(&q);
  printf("deq: %d\n", *v);
  v = lq_deq(&q);
  printf("deq: %d\n", *v);

  lq_enq(&q, &a);
  printf("enq: %d\n", a);
  lq_enq(&q, &b);
  printf("enq: %d\n", b);
  lq_enq(&q, &a);
  printf("enq: %d\n", a);
  lq_enq(&q, &b);
  printf("enq: %d\n", b);
  lq_enq(&q, &a);
  printf("enq: %d\n", a);
  lq_enq(&q, &b);
  printf("enq: %d\n", b);
  
  lq_d *d, *d_;
  d = q.head;
  while (d != NULL) {
    int *x = d->data;
    printf("%d\n", *x);
    if (*x == 1) {
      d_ = lq_next(d);
      lq_remove(&q, d);
      d = d_;
      continue;
    }
      d = lq_next(d);
  }

  v = lq_deq(&q);
  printf("deq: %d\n", *v);
  v = lq_deq(&q);
  printf("deq: %d\n", *v);
  v = lq_deq(&q);
  printf("deq: %d\n", *v);
  v = lq_deq(&q);
  printf("deq: %d\n", *v);
}
*/
