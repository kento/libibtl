#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "fdmi_err.h"

//#define FDMI_ERR
#define DEBUG_STDOUT stderr

int rank;
char hostname[256];

int fdmi_err_init (void) 
{
  char* value;
  rank = getpid();

  if (gethostname(hostname, sizeof(hostname)) != 0) {
    fdmi_err("gethostname fialed (%s:%s:%d)", __FILE__, __func__, __LINE__);
  }
  return 0;
}

char* fdmi_gethostname() {
  return hostname;
}

int fdmi_err(const char* fmt, ...)
{
  va_list argp;
  fprintf(stderr, "FDMI:ERROR:%s:%d: ", hostname, rank);
  va_start(argp, fmt);
  vfprintf(stderr, fmt, argp);
  va_end(argp);
  fprintf(stderr, "\n");
  exit(1);
}

int fdmi_alert(const char* fmt, ...)
{
  va_list argp;
  fprintf(stderr, "FDMI:ALERT:%s:%d: ", hostname, rank);
  va_start(argp, fmt);
  vfprintf(stderr, fmt, argp);
  va_end(argp);
  fprintf(stderr, " (%s:%s:%d)\n", __FILE__, __func__, __LINE__);
  exit(1);
}

int fdmi_dbg(const char* fmt, ...) {
#ifdef FDMI_ERR
  va_list argp;
  fprintf(DEBUG_STDOUT, "FDMI:DEBUG:%s:%d: ", hostname, rank);
  va_start(argp, fmt);
  vfprintf(DEBUG_STDOUT, fmt, argp);
  va_end(argp);
  fprintf(DEBUG_STDOUT, "\n");
#endif
}

int fdmi_dbgi(int r, const char* fmt, ...) {
  if (rank != r) return;
  va_list argp;
  fprintf(DEBUG_STDOUT, "FDMI:DEBUG:%s:%d: ", hostname, rank);
  va_start(argp, fmt);
  vfprintf(DEBUG_STDOUT, fmt, argp);
  va_end(argp);
  fprintf(DEBUG_STDOUT, "\n");
}

int fdmi_debug(const char* fmt, ...)
{
  va_list argp;
  fprintf(DEBUG_STDOUT, "FDMI:DEBUG:%s:%d: ", hostname, rank);
  va_start(argp, fmt);
  vfprintf(DEBUG_STDOUT, fmt, argp);
  va_end(argp);
  fprintf(DEBUG_STDOUT, "\n");
}


void fdmi_exit(int no) {
  fprintf(stderr, "FDMI:DEBUG:%s:%d: == EXIT == sleep 1sec ...\n", hostname, rank);
  sleep(1);
  exit(no);
  return;
}

void fdmi_test(void* ptr, char* file, char* func, int line)
{
  if (ptr == NULL) {
    fdmi_err("NULL POINTER EXCEPTION (%s:%s:%d)", file, func, line);
  }
}


void fdmi_btrace() 
{
  int j, nptrs;
  void *buffer[100];
  char **strings;

  nptrs = backtrace(buffer, 100);

  /* backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO)*/
  strings = backtrace_symbols(buffer, nptrs);
  if (strings == NULL) {
    perror("backtrace_symbols");
    exit(EXIT_FAILURE);
  }   

  /*
    You can translate the address to function name by
    addr2line -f -e ./a.out <address>
  */
  for (j = 0; j < nptrs; j++)
    fdmi_dbg("%s", strings[j]);
  free(strings);
  return;
}

void fdmi_btracei(int r) 
{
  if (rank != r) return;
  fdmi_btrace();
  return;
}
