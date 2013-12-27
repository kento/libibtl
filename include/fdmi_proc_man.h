#ifndef _FDMI_PROC_MAN
#define _FDMI_PROC_MAN


struct fdmi_communicator {
  struct fdmi_communicator *comm_master;
  int    id;             /*id to destingish messages by different communicators*/
  int    vrank;
  int    psize;          /*# of physical ranks; total process number*/
  int	 comm_size;      /*# of virtual ranks; total virtaul rank number; communicator size*/
  int	 rmap_ptov_size;
  int	*rmap_ptov;
  int	 rmap_vtop_size;
  int	*rmap_vtop;
};


enum fdmi_state {
  FDMI_COMPUTE,
  FDMI_FAILURE,
  FDMI_RECOVERY,
  FDMI_JOINING
};

//struct fdmi_communicator fdmi_comm_master_global;   
struct fdmi_communicator *fdmi_comm_master_ignore;   
struct fdmi_communicator *fdmi_comm_master_exclude;  
struct fdmi_communicator *fdmi_comm_master_elect;


struct fdmi_communicator *fmi_comm_world;
#define FMI_COMM_WORLD fmi_comm_world

struct fdmi_communicator *fmi_comm_world_ft;
typedef struct fdmi_communicator* FMI_Comm;

int fdmi_size;
int fdmi_numspare;
int fdmi_numproc;
int prank;
int fdmi_state;

void fdmi_proc_man_init(int psize, int numspare, int numproc, int prank);
void fdmi_standby(int prank);
void fdmi_proc_man_join(void);
void fdmi_rmap_btw_vandp_add(int vrank, int prank);
int fdmi_rmap_ptov_get(int prank);
int fdmi_rmap_vtop_get(int vrank);
int fdmi_rmap_is_standby(int prank);
void fdmi_rmap_itop_add(void* id, int prank);
int fdmi_rmap_itop_get(void* id);
int fdmi_proc_update_map(int old_prank, int new_prank, int new_vrank);

int fdmi_init_comm_world_ignore(struct fdmi_communicator **comm);
int fdmi_init_comm_world_exclude(struct fdmi_communicator **comm);
int fdmi_init_comm_world_elect(struct fdmi_communicator **comm);

int fdmi_verbs_comm_dup(struct fdmi_communicator *comm, struct fdmi_communicator **newcomm);
int fdmi_comm_split(struct fdmi_communicator *comm, int color, int key, struct fdmi_communicator **newcomm);

int fdmi_comm_get_vrank(struct fdmi_communicator *comm, int prank);

void fdmi_reallocate_state_listd(int *failed_offset, int *failed_range, int failed_length,
                                 int *elected_offset, int *elected_range, int elected_length);
void fdmi_update_comm_world(int *failed_offset, int *failed_range,
                            int *elected_offset, int *elected_range, int length);
#endif
