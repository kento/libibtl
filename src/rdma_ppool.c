#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>

#include "rdma_common.h"
#include "rdma_ppool.h"
#include "list_queue.h"

static int make_dir(char * dir);
static void make_top_dir(); 
static void make_node_dir(); 
static void make_source_ip_dir(char *src_ip);
static void make_active_host_dir(char *src_ip);

static void get_top_dir(char *top_dir);
static void get_node_dir(char *node_dir);
static void get_source_ip_dir(char *src_ip_dir, char *src_ip);
static void get_active_host_dir(char *active_host_dir, char *src_ip);

static void make_file(char *path);
static void make_lock_file(char *src_ip);
static void make_port_file(char *addr, int port);
static void make_active_lock_file(char *src_ip);

static void get_lock_file_path(char *lock_file_path, char* src_ip);
static void get_port_file_path(char* port_file_path, char* addr, int port);
static void get_active_lock_file_path(char *lock_file_path, char* src_ip);

static void lock_iface(char* src_ip);
static void update_port_file(char *addr, int port);

static DIR* open_dir(char *dir);
static void recursive_dir_search(lq *dir_q, char *dir);

void find_passive_host(struct psockaddr *psa, int mode)
{
  find_passive_host_rd(psa);

  return;
}

static void recursive_dir_search(lq* dir_q, char *cur_dir_path)
{
  char next_dir_path[512];
  DIR* cur_dir;
  struct dirent* dp;
  struct stat statbuf;
  int last = 1;
  int i;



  cur_dir = open_dir(cur_dir_path); 
  for(i = 0; NULL != (dp = readdir(cur_dir)); i++){
    stat(dp->d_name, &statbuf);
    //    sprintf(next_dir_path,"%s/%s" , cur_dir_path, dp->d_name);
    //   fprintf(stderr, "%s:%d\n", cur_dir_path, statbuf.st_mode);
    if(S_ISDIR(statbuf.st_mode)// && S_ISREG(statbuf.st_mode)
    //    if (statbuf.st_mode & S_IFMT == S_IFDIR
       && !(strcmp(dp->d_name, ".") == 0) 
       && !(strcmp(dp->d_name, "..") == 0) )
      {
	last = 0;
	//	fprintf(stderr, "%s\n", cur_dir_path);
	sprintf(next_dir_path,"%s/%s" , cur_dir_path, dp->d_name);
	fprintf(stderr, "%s\n", next_dir_path);
	recursive_dir_search(dir_q, next_dir_path);

    }
  }

  if (last == 1) {
    char *leaf_dir;
    leaf_dir = (char *)malloc(sizeof(strlen(cur_dir_path)));
    lq_enq(dir_q, leaf_dir);
  }
  return;
}

void is_dir(char * dir)
{
  return opendir(dir);
}

void find_passive_host_rd (struct psockaddr *psa)
{
  char top_dir_path[512];
  lq top_dir_q;

  get_top_dir(top_dir_path);

  recursive_dir_search(&top_dir_q, top_dir_path);
  return;


}

static DIR* open_dir(char *dir)
{
  DIR *odir;
  if (NULL == (odir = opendir(dir))){
    fprintf(stderr, "failed to open directory: %s\n", dir);
    exit(1);
  }
  return odir;
}


void join_passive_pool(char *addr, int port)
{ 
  char *ndpool_dir;

  make_top_dir();
  make_node_dir();
  make_source_ip_dir(addr);
  make_active_host_dir(addr);

  make_lock_file(addr);
  make_active_lock_file(addr);
  lock_iface(addr);

  make_port_file(addr, port);
  update_port_file(addr, port);
  fprintf(stderr, "%s:%d Joined !\n", addr, port);
}

static void update_port_file(char *addr, int port)
{
  char port_file_path[512];
  char port_str[16];
  int fd;

  get_port_file_path(port_file_path, addr, port);

  if ((fd = open(port_file_path, O_WRONLY)) != -1) {
    sprintf(port_str,"%d", port);
    write(fd, port_str, strlen(port_str));
  } else {
    fprintf(stderr, "failed to open %s \n", port_file_path);
  }
}


static void make_port_file(char *addr, int port)
{
  char port_file_path[512];
  get_port_file_path(port_file_path, addr, port);
  make_file(port_file_path);
}

static void get_port_file_path(char* port_file_path, char* addr, int port)
{
  char path[512];
  get_source_ip_dir(path, addr);
  strcat(path, "/port");
  strcpy(port_file_path, path);
}

static void lock_iface(char* src_ip)
{
  char lock_file_path[512];
  int fd;
  get_lock_file_path(lock_file_path, src_ip);
  if ((fd = open(lock_file_path, O_WRONLY | O_APPEND)) != -1) {
    if (flock(fd, LOCK_EX) != 0) {
      fprintf(stderr,"failed to get lock %s\n", lock_file_path);
    }
  } else {
    fprintf(stderr,"failed to open lock file %s\n", lock_file_path);
  }
}

static void make_active_lock_file(char *src_ip)
{
  char lock_file_path[512];
  get_active_lock_file_path(lock_file_path, src_ip);
  make_file(lock_file_path);
}

static void get_active_lock_file_path(char *lock_file_path, char* src_ip) 
{
  char path[512];
  get_source_ip_dir(path, src_ip);
  strcat(path, "/active");
  strcpy(lock_file_path, path);
}

static void make_lock_file(char *src_ip)
{
  char lock_file_path[512];
  get_lock_file_path(lock_file_path, src_ip);
  make_file(lock_file_path);
}

static void get_lock_file_path(char *lock_file_path, char* src_ip) 
{
  char path[512];
  get_source_ip_dir(path, src_ip);
  strcat(path, "/life");
  strcpy(lock_file_path, path);
}

static void make_file(char *path)
{
  int fd;
  if (!(fd = open(path, O_CREAT, S_IREAD | S_IWRITE))) {
    fprintf(stderr, "%s already exists\n", path);
  } else {
    fprintf(stderr, "Created %s\n", path);
  }
  close(fd);
}


static void make_active_host_dir(char *src_ip)
{
  char active_host_dir[512];
  get_active_host_dir(active_host_dir, src_ip);
  make_dir(active_host_dir);
}

static void get_active_host_dir(char *active_host_dir, char *src_ip)
{
  char dir[512];

  get_source_ip_dir(dir, src_ip);
  strcat(dir, "/ahosts");
  strcpy(active_host_dir, dir);
}


static void make_source_ip_dir(char *src_ip)
{
  char src_ip_dir[512];
  get_source_ip_dir(src_ip_dir, src_ip);
  make_dir(src_ip_dir); 
}

static void get_source_ip_dir(char *src_ip_dir, char *src_ip) 
{
  char dir[512];

  get_node_dir(dir);
  strcat(dir, "/");
  strcat(dir, src_ip);
  strcpy(src_ip_dir, dir);
}

static int exist_node_dir()
{
  
}

static int exist_dir(char * dir)
{
  
}

static int make_dir(char * dir) 
{
  if (mkdir(dir,
	    S_IRUSR | S_IWUSR | S_IXUSR |         /* rwx */
	    S_IRGRP | S_IWGRP | S_IXGRP |         /* rwx */
	    S_IROTH | S_IWOTH | S_IXOTH) == 0) {  /* rwx */
    fprintf(stderr,"Created %s\n", dir);
  } else {
    fprintf(stderr,"Node directory: %s alread exist. skipped.\n", dir);
  }
}

static void make_top_dir() 
{
  char host[64];
  char top_dir[512];

  get_top_dir(top_dir);
  make_dir(top_dir);
}

static void get_top_dir(char *top_dir)
{
  strcpy(top_dir, RDMA_NDPOOL_DIR);
}

static void make_node_dir() 
{
  char host[64];
  char host_dir[512];
  
  get_node_dir(host_dir);
  make_dir(host_dir);
}

static void get_node_dir(char *node_dir)
{
  char host[64];
  char host_dir[512];

  get_top_dir(host_dir);
  strcat(host_dir, "/");
  gethostname(host, sizeof(host));
  strcat(host_dir, host);
  strcpy(node_dir, host_dir);
}

