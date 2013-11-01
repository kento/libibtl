#include "fdmi_hashtable.h"
#include "fdmi_queue.h"
#include "fdmi_mem.h"

struct fdmi_hashtable_node
{
  void* key;
  void* data;
};

struct fdmi_hashtable* fdmi_hashtable_create(int length)
{
  struct fdmi_hashtable *ht;
  int i;

  ht	   = (struct fdmi_hashtable*)fdmi_malloc(sizeof(struct fdmi_hashtable));
  ht->len  = length;
  ht->list = (struct fdmi_queue**)fdmi_malloc(sizeof(struct fdmi_queue*) * ht->len);
  for (i = 0; i < ht->len; i++) {
    ht->list[i] = fdmi_queue_create();
  }
  return ht;
}

void fdmi_hashtable_destroy(struct fdmi_hashtable* ht) 
{
  struct fdmi_hashtable_node *nd;
  int i;
  for (i = 0; i < ht->len; i++) {
    /*TODO: fdmi_queue_destroy seems not to free all of resources*/
    while ((nd = fdmi_queue_deq(ht->list[i])) != NULL) {
      fdmi_free(nd);
    }
    fdmi_queue_destroy(ht->list[i]);
  }
  fdmi_free(ht->list);
  fdmi_free(ht);
  return;
}

/* void *data(event->id) is multiple of 16, so we divide *data by 16 first, 
   then mod by ht->len to distribute the hash*/
static unsigned long fdmi_hash(struct fdmi_hashtable *ht, void *data)
{
  unsigned long hash = (unsigned long) data;
  hash = (hash / 16) % ht->len;
  return hash;
}


void fdmi_hashtable_add(struct fdmi_hashtable *ht, void *key, void *data)
{
  struct fdmi_queue *q;
  struct fdmi_hashtable_node *nd;
  unsigned long hash;

  hash = fdmi_hash(ht, key);
  q = ht->list[hash];

  nd = (struct fdmi_hashtable_node*)fdmi_malloc(sizeof(struct fdmi_hashtable_node));
  nd->key = key;
  nd->data = data;
  fdmi_queue_enq(q, nd);
  return;
}

void* fdmi_hashtable_update(struct fdmi_hashtable *ht, void *key, void *new_data)
{
  struct fdmi_queue *q;
  struct fdmi_hashtable_node *nd;
  void* old_data;;
  unsigned long hash;

  hash = fdmi_hash(ht, key);
  q = ht->list[hash];

  for (nd = (struct fdmi_hashtable_node*)fdmi_queue_iterate(q, FDMI_QUEUE_HEAD); nd != NULL;
       nd = (struct fdmi_hashtable_node*)fdmi_queue_iterate(q, FDMI_QUEUE_NEXT)) {  
    if (nd->key == key) {
      old_data = nd->data;
      nd->data = new_data;
      return old_data;
    }
  }
  fdmi_hashtable_add(ht, key, new_data);
  return NULL;
}

void* fdmi_hashtable_get(struct fdmi_hashtable *ht, void *key)
{
  struct fdmi_queue *q;
  struct fdmi_hashtable_node *nd;
  unsigned long  hash;

  hash = fdmi_hash(ht, key);
  q = ht->list[hash];

  for (nd = (struct fdmi_hashtable_node*)fdmi_queue_iterate(q, FDMI_QUEUE_HEAD); nd != NULL;
       nd = (struct fdmi_hashtable_node*)fdmi_queue_iterate(q, FDMI_QUEUE_NEXT)) {  
    if (nd->key == key) {
      return nd->data;
    }
  }
  return NULL;
}

void* fdmi_hashtable_remove(struct fdmi_hashtable *ht, void *key)
{
  struct fdmi_queue *q;
  struct fdmi_hashtable_node *nd;
  unsigned long  hash;
  void* data;

  hash = fdmi_hash(ht, key);
  q = ht->list[hash];

  for (nd = (struct fdmi_hashtable_node*)fdmi_queue_iterate(q, FDMI_QUEUE_HEAD); nd != NULL;
       nd = (struct fdmi_hashtable_node*)fdmi_queue_iterate(q, FDMI_QUEUE_NEXT)) {  
    if (nd->key == key) {
      data = nd->data;
      fdmi_queue_remove(q, nd);
      fdmi_free(nd);
      return nd->data;
    }
  }
  return NULL;
}
