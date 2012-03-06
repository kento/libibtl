#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
//#include <unistd.h>
//#include <sys/file.h>
//#include <sys/types.h>
//#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
//#include <unistd.h>

#include "common.h"
#include "rdma_common.h"
#include "rdma_ppool.h"
#include "list_queue.h"

#define AHOST_FNAME "/ahosts"
#define LIFE_FNAME "/life"

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
static void make_port_file(char *addr);
static void make_active_lock_file(char *src_ip);

static void get_lock_file_path(char *lock_file_path, char* src_ip);
static void get_port_file_path(char* port_file_path, char* addr);
static void get_active_lock_file_path(char *lock_file_path);

static void lock_iface(char* src_ip);
static void update_port_file(char *addr, int port);

static int count_connected_active(char *source_ip_dir);


static DIR* open_dir(char *dir);
static int is_dir(char * dir_path);
static int filter_dir(struct dirent *dp);
static int unlock_ndpool(int fd);
static int lock_ndpool(void);
static void recursive_dir_search(lq *dir_q, char *dir, int depth);
static int is_running(char* source_ip_dir);
static int test_lock(char * path);
static int is_same_host_connecting (char * source_ip_dir) ;
static void make_connect_lock_file(char* passive_host);

static char* get_dir_name (char* path);
static int get_port_number(char* source_ip_dir);

static int lock_file(char* path, int operation);
static int unlock_file(int fd);

void find_passive_host(struct psockaddr *psa, int mode)
{
  int lock ;
  int status = -1;

  while (find_passive_host_rd(psa) == -1) {
    usleep(1000);
  }

  /*
  lock = lock_ndpool();
  while (status == -1) {
    status = find_passive_host_rd(psa);
    unlock_ndpool(lock);
  }
  */
  return;
}


static void recursive_dir_search(lq* dir_q, char *cur_dir_path, int depth)
{
  char next_dir_path[512];
  DIR* cur_dir;
  struct dirent* dp;
  struct stat statbuf;
  int i;

  //  fprintf(stderr, "Start function: %d\n", depth);
  if (depth == 0) {
    char *leaf_dir_path;
    leaf_dir_path = (char *)malloc(strlen(cur_dir_path));
    //    leaf_dir_path = (char *)malloc();
    //    fprintf(stderr, "ENQ:%s\n", cur_dir_path);
    strcpy(leaf_dir_path, cur_dir_path);
    lq_enq(dir_q, leaf_dir_path);
    //    fprintf(stderr, "End of function: %d\n", depth);
    return;
  }

  //  fprintf(stderr, "= |%s|\n", cur_dir_path);
  cur_dir = open_dir(cur_dir_path); 
  //  fprintf(stderr, "= %s\n", cur_dir_path);
  //  fprintf(stderr, "= %p\n", cur_dir);
  //  for(i = 0; NULL != (dp = readdir(cur_dir)); i++){
  while (dp = readdir(cur_dir)) {
    //fprintf(stderr, "Start of for\n");
    //    fprintf(stderr, "Start of for: \n");
    //    stat(dp->d_name, &statbuf);
    sprintf(next_dir_path,"%s/%s" , cur_dir_path, dp->d_name);
    //    fprintf(stderr, "== %s\n", next_dir_path);
    //fprintf(stderr, "%s:%d\n", cur_dir_path, statbuf.st_mode);
    if (  (strcmp(dp->d_name, ".")  == 0) 
	  || (strcmp(dp->d_name, "..")  == 0) ){
      continue;
    }
    if(
       dp->d_type == DT_DIR
       //is_dir(next_dir_path)
       //       S_ISDIR(statbuf.st_mode)
       // && S_ISREG(statbuf.st_mode)
       //    if (statbuf.st_mode & S_IFMT == S_IFDIR
       )
      {
	//fprintf(stderr, "=== %s:  depth=%d\n", next_dir_path, depth);
	//fprintf(stderr, "%s\n", next_dir_path);
	recursive_dir_search(dir_q, next_dir_path, depth - 1);
	//	fprintf(stderr, "=== %s:  depth=%d\n", next_dir_path, depth);
    }
  }
  //  fprintf(stderr, "End of function: %d\n", depth);
  //  fprintf(stderr, "= %p\n", cur_dir);
  closedir(cur_dir) ;
  return;
}

 /*
static void recursive_dir_search(lq* dir_q, char *cur_dir_path, int depth)
{
  char next_dir_path[512];
  DIR* cur_dir;
  struct dirent* dp;
  struct dirent ** namelist;
  struct stat statbuf;
  int i;

  fprintf(stderr, "Start function: %d\n", depth);
  if (depth == 0) {
    char *leaf_dir_path;
    leaf_dir_path = (char *)malloc(sizeof(strlen(cur_dir_path)));
    fprintf(stderr, "ENQ:%s\n", cur_dir_path);
    strcpy(leaf_dir_path, cur_dir_path);
    lq_enq(dir_q, leaf_dir_path);
    fprintf(stderr, "End of function: %d\n", depth);
    return;
  }

  int r;
  //  fprintf(stderr, "= |%s|\n", cur_dir_path);
  //  cur_dir = open_dir(cur_dir_path); 
  r = scandir(cur_dir_path, &namelist, &filter_dir, NULL);
  if(r == -1) {
    err(EXIT_FAILURE, "%s\n", cur_dir_path);
  }
  for (i = 0; i < r; ++i) {
    sprintf(next_dir_path,"%s/%s" , cur_dir_path, namelist[i]);
    //    fprintf(stderr, "== %s\n", next_dir_path);
    //    fprintf(stderr, "=== %s:  depth=%d\n", next_dir_path, depth);
    //fprintf(stderr, "%s\n", next_dir_path);
    recursive_dir_search(dir_q, next_dir_path, depth - 1);
    //    fprintf(stderr, "=== %s:  depth=%d\n", next_dir_path, depth);
    free(namelist[i]);
  }
  free(namelist);
  return;
}
 */

static int filter_dir(struct dirent *dp) 
{
  if (  (strcmp(dp->d_name, ".")  == 0) ) return 0;
  if (  (strcmp(dp->d_name, "..")  == 0) ) return 0;
  //  if(is_dir(next_dir_path)) return 1;
  return 1;
}

static int is_dir(char * dir_path)
{
  DIR *dir = NULL;
  fprintf(stderr, "= %p\n", dir);
  fprintf(stderr, "%s=>\n", dir_path);
  dir = opendir(dir_path);
  fprintf(stderr, "%s=>\n", dir_path);
  fprintf(stderr, "= %p\n", dir);
  if (dir == NULL) {
    fprintf(stderr, "Not dir= %p\n", dir);
    return 0;
  } else {
    fprintf(stderr, "is  dir= %p\n", dir);
    closedir(dir);
    return 1;
  }
}

int find_passive_host_rd(struct psockaddr *psa)
{
  char top_dir_path[512];
  lq ndp_q;
  int lock;
  char *source_ip_dir;
  int min_count = -1;
  int temp_count = 0;
  char min_host[32];
  char min_source_ip_dir[512];
  int status = 0;
  
  lock = lock_ndpool();
  get_top_dir(top_dir_path);
  lq_init(&ndp_q);
  recursive_dir_search(&ndp_q, top_dir_path, 2);

  lq_init_it(&ndp_q);
  while ((source_ip_dir = (char *)lq_next(&ndp_q)) != NULL) {
    if (is_running(source_ip_dir) == 0)  continue;

    if (is_same_host_connecting(source_ip_dir)) {
      min_count = 0;
      sprintf(min_source_ip_dir, "%s", source_ip_dir);
      sprintf(min_host, "%s", get_dir_name(source_ip_dir));      
      status = 1;
    }
  }
  lq_fin_it(&ndp_q);


  if (status == 0) {
    lq_init_it(&ndp_q);
    while ((source_ip_dir = (char *)lq_next(&ndp_q)) != NULL) {
      if (is_running(source_ip_dir) == 0)  continue;
      temp_count = count_connected_active(source_ip_dir);
      if (status == 0 && (min_count == -1 || min_count > temp_count)) {
	min_count = temp_count;
	sprintf(min_source_ip_dir, "%s", source_ip_dir);
	sprintf(min_host, "%s", get_dir_name(source_ip_dir));      
      }
      //      free(source_ip_dir);
    }
    lq_fin_it(&ndp_q);
  }

  lq_init_it(&ndp_q);
  while ((source_ip_dir = (char *)lq_next(&ndp_q)) != NULL) {
      free(source_ip_dir);
  }
  lq_fin_it(&ndp_q);
  
  if (!(min_count == -1)) {
    sprintf(psa->addr, "%s", min_host);
    //    fprintf(stderr, "%s\n", min_source);
    psa->port = get_port_number(min_source_ip_dir);
    make_connect_lock_file(min_source_ip_dir); 
    //    fprintf(stderr, "%d\n", min_count);
    unlock_ndpool(lock);
    return min_count;
  }
  unlock_ndpool(lock);
  return min_count;
}

static int is_same_host_connecting (char * source_ip_dir) 
{
  char *name;
  char host[16];
  int status = 0;
  char dir[516];
  char active_lock_file_path[516];
  int scanf_num;
  struct dirent ** namelist;
  int i;

  sprintf(dir, "%s%s", source_ip_dir, AHOST_FNAME);
  if((scanf_num = scandir(dir, &namelist, filter_dir, NULL)) == -1) {
    fprintf(stderr, "Failed scan under a directory: %s\n", dir);
    exit(1);
  }
  //  fprintf(stderr, "%s:%d\n", ahost_path, scanf_num);
  for (i = 0; i < scanf_num; ++i) {
    sprintf(active_lock_file_path, "%s/%s", dir, namelist[i]->d_name);
    if (!test_lock(active_lock_file_path)) {
      free(namelist[i]);
      continue;
    }
    name = strtok(namelist[i]->d_name, ".");
    gethostname(host, sizeof(host));
    if (strcmp(name, host) == 0) {
      status = 1;
    }
    free(namelist[i]);
  }
  free(namelist);
  return status;
}

static void make_connect_lock_file(char* passive_host_ip)
{
  char connect_lock_file_path[512];
  char source_ip_dir_path[512];
  char hostname[16];
  int pid;
  gethostname(hostname, sizeof(hostname));
  pid = get_pid();
  sprintf(connect_lock_file_path, "%s/%s/%s.%d", passive_host_ip, AHOST_FNAME, hostname, pid);
  //  fprintf(stderr, "%s\n", connect_lock_file_path);
  make_file(connect_lock_file_path);
  lock_file(connect_lock_file_path, LOCK_EX);
  return;
}

static int get_port_number(char* source_ip_dir)
{
  char port_file_path[512];
  char top_dir[512];
  char port[16];
  int port_number = 0;
  int fd;
  
  memset(port, 0, sizeof(port));
  get_top_dir(top_dir);
  sprintf(port_file_path, "%s/port", source_ip_dir);
  //  get_port_file_path(port_file_path, source_ip);
  //  fprintf(stderr, "port_file_path = %s\n", port_file_path);
  if ((fd = open(port_file_path, O_RDONLY)) != -1) {
    read(fd, port, sizeof(port));
    port_number = atoi(port);
    while (port_number == 0) {
      usleep(100);
      read(fd, port, sizeof(port));
      port_number = atoi(port);
    }
    return port_number;

  } else {
    fprintf(stderr, "failed to open port file %s \n", port_file_path);
    exit(1);
  }
  return 0;
}

static char* get_dir_name (char* path)
{
  char *name, *tmp = NULL;
  tmp = strtok(path, "/");
  while ((name = strtok(NULL, "/"))) {
    tmp = name;
  }
  return tmp;
}

static int is_running(char* source_ip_dir)
{
  int fd = -1;
  char life_path[512];
  sprintf(life_path, "%s/%s", source_ip_dir, LIFE_FNAME);
  // fprintf(stderr, "%s\n", life_path);
  return test_lock(life_path);
}

static int test_lock(char * path)
{
  int fd = -1;
  fd = lock_file(path, LOCK_EX | LOCK_NB);
  //  printf("==== fd= %d\n", fd);
  if (fd == -1) {
    return 1;
  } else {
    unlock_file(fd);
    close(fd);
    return 0;
  }
}

static int count_connected_active(char *source_ip_dir)
{
  int scanf_num;
  int count = 0;
  char ahost_path[512];
  char active_lock_file_path[512];
  struct dirent ** namelist;
  int i;

  sprintf(ahost_path, "%s%s", source_ip_dir, AHOST_FNAME);

  if((scanf_num = scandir(ahost_path, &namelist, filter_dir, NULL)) == -1) {
    fprintf(stderr, "Failed scan under a directory: %s\n", ahost_path);
    exit(1);
  }
  //  fprintf(stderr, "%s:%d\n", ahost_path, scanf_num);
  for (i = 0; i < scanf_num; ++i) {
    sprintf(active_lock_file_path, "%s/%s", ahost_path, namelist[i]->d_name);
    if (test_lock(active_lock_file_path)) {
      count++;
    }
    free(namelist[i]);
  }
  free(namelist);
  /*Count the actual number of active hosts*/
  return count;
}


static int lock_ndpool() 
{
  int fd;
  char active_lock_file_path[512];
  get_active_lock_file_path(active_lock_file_path);
  fd = lock_file(active_lock_file_path, LOCK_EX);
  return fd;
}

static int lock_file(char* path, int operation)
{
  int fd = -1;
  int r = -1;

  if ((fd = open(path, O_WRONLY | O_APPEND)) != -1) {
    r = flock(fd, operation);
    //    fprintf(stderr, "Lock r=%d\n", r);
    if (r == 0) {
      return fd;
    } else {
      close(fd);
      return -1;
    }
  } else {
    fprintf(stderr, "open file error\n");
    exit(1);
  }
}

static int unlock_file(int fd) {
  flock(fd, LOCK_UN);
  return close(fd);
}

static int unlock_ndpool(int fd)
{
  flock(fd, LOCK_UN);
  close(fd);
}

static DIR* open_dir(char *dir_path)
{
  DIR *dir = NULL;
  if (NULL == (dir = opendir(dir_path))){
    //    closedir(dir);
    fprintf(stderr, "failed to open directory: %s\n", dir_path);
    exit(1);
  }
  return dir;
}


void join_passive_pool(char *addr, int port)
{ 
  char *ndpool_dir;
  int lock;

  make_top_dir();
  make_node_dir();
  make_source_ip_dir(addr);
  make_active_host_dir(addr);

  make_lock_file(addr);
  make_active_lock_file(addr);

  lock_iface(addr);

  make_port_file(addr);
  //  lock = lock_ndpool();
  update_port_file(addr, port);
  //  unlock_ndpool(lock);
  fprintf(stderr, "%s:%d Joined !\n", addr, port);
}

static void update_port_file(char *addr, int port)
{
  char port_file_path[512];
  char port_str[16];
  int fd;

  get_port_file_path(port_file_path, addr);

  if ((fd = open(port_file_path, O_WRONLY)) != -1) {
    sprintf(port_str,"%d", port);
    write(fd, port_str, strlen(port_str));
    fsync(fd);
    sleep(1);
  } else {
    fprintf(stderr, "failed to open %s \n", port_file_path);
  }
}


static void make_port_file(char *addr)
{
  char port_file_path[512];
  get_port_file_path(port_file_path, addr);
  make_file(port_file_path);
}

static void get_port_file_path(char* port_file_path, char* addr)
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
  get_active_lock_file_path(lock_file_path);
  make_file(lock_file_path);
}

static void get_active_lock_file_path(char *lock_file_path)
{
  char path[512];
  //  get_source_ip_dir(path, src_ip);
  get_top_dir(path);
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
    //    fprintf(stderr, "Created %s\n", path);
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
  strcat(dir, AHOST_FNAME);
  strcpy(active_host_dir, dir);
}


static void make_source_ip_dir(char *src_ip)
{
  char src_ip_dir[512];
  char command[512];
  get_source_ip_dir(src_ip_dir, src_ip);
  sprintf(command, "rm -rf %s\n", src_ip_dir);
  system(command);
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

