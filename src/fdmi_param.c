#include <stdlib.h>
#include <stdio.h>

#include "fdmi_param.h"
#include "fdmi_mem.h"

#define DEBUG 1

char* fdmi_param_get(char* param_name)
{
  //TODO: Return address change for some reasones
  char* value;
  value = getenv(param_name);
  if (value != NULL) {
    return value;
  } 
  return NULL;
}

void fdmi_param_set(char* name, char* value)
{
  //  char env[1024];
  char *env;
  env = (char*)fdmi_malloc(1024);
  sprintf(env, "%s=%s", name, value);
  putenv(env);

  return;
}

