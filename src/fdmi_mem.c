#include <stdlib.h>
#include <stdio.h>

#include "fdmi_err.h"

unsigned long total_alloc_size = 0;
unsigned long total_alloc_count = 0;

void* fdmi_malloc(size_t size) 
{
  void* addr;
  //  fdmi_dbg("malloc: %d", total_alloc_size);
  if ((addr = malloc(size)) == NULL) {
    fdmi_err("Memory allocation returned (%s:%s:%d)",  __FILE__, __func__, __LINE__);
  }
  total_alloc_count++;
  //fdmi_dbg("mem: %lu", total_alloc_count);

  //TODO: Manage memory consumption
  //  total_alloc_size += size;
  //  fdmi_dbg("malloc: done %d", total_alloc_size);
  return addr;
}

void fdmi_free(void* addr) 
{
  free(addr);
  total_alloc_count--;
  //  fdmi_dbg("mem: %lu", total_alloc_count);
  //
  //TODO: Manage memory consumption
  //  total_alloc_size -= size;
  return;
}



