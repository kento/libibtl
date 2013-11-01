#ifndef FDMI_HASHTABLE_H
#define FDMI_HASHTABLE_H

#include "fdmi_queue.h"


struct fdmi_hashtable {
  int len;
  struct fdmi_queue **list;
};

struct fdmi_hashtable* fdmi_hashtable_create(int length);
void fdmi_hashtable_destroy(struct fdmi_hashtable* ht);
void fdmi_hashtable_add(struct fdmi_hashtable *ht, void *key, void *data);
void* fdmi_hashtable_update(struct fdmi_hashtable *ht, void *key, void *new_data);
void* fdmi_hashtable_get(struct fdmi_hashtable *ht, void *key);
void* fdmi_hashtable_remove(struct fdmi_hashtable *ht, void *key);

#endif
