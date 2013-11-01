#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "fdmi_pmtv_common.h"
#include "fdmi.h"
#include "fdmi_err.h"
#include "fdmi_mem.h"
#include "fdmi_hashtable.h"
#include "fdmi_proc_man.h"
//#include "fdmi_loop.h"
#include "fdmi_datatype.h"
//#include "fdmi_allgather.h" //removed comm_split

/*TODO: I multipled size of rank_map_vtop by 2(extra) for dynamic scaling, but the magic number should be removed */
#define EXTRA (2)

enum prank_state {
  FDMI_PFAILURE,
  FDMI_PCOMPUTE,
  FDMI_PSTANDBY,
  FDMI_PNOTHING
};

int *rmap_vtop;
int *rmap_ptov;
int *prank_state_list;
struct fdmi_hashtable *hstb_itop;
struct fdmi_queue *starting_prank_q;

int comm_id_counter = 0;

int fdmi_psize;
int fdmi_prank;
int fdmi_numspare;
int fdmi_numproc;
int fdmi_next_prank;

static int fdmi_get_next_prank(void);
static int fdmi_proc_man_comm_init(struct fdmi_communicator **comm_master, int psize, int numspare, int numproc, int prank);

/*TODO: change input valuables order*/
void fdmi_proc_man_init(int psize, int numspare, int numproc, int prank)
{
  int	fdmi_vsize;
  int	i;
  int	errno;

  fdmi_psize	  = psize;
  fdmi_prank	  = prank;
  fdmi_numspare	  = numspare;
  fdmi_numproc	  = numproc;
  fdmi_vsize	  = fdmi_psize - (fdmi_numspare * fdmi_numproc);
  fdmi_next_prank = fdmi_vsize;
  
  prank_state_list = (int*)fdmi_malloc(sizeof(int) * fdmi_psize * EXTRA);
  for (i = 0; i < fdmi_psize * EXTRA; i++) {
    if (i < fdmi_vsize) {
      prank_state_list[i] = FDMI_PCOMPUTE;
    } else if (i < fdmi_psize) {
      prank_state_list[i] = FDMI_PSTANDBY;
    } else {
      prank_state_list[i] = FDMI_PNOTHING;
    }
  }
  hstb_itop = fdmi_hashtable_create(sizeof(int) * fdmi_psize * EXTRA);
  starting_prank_q = fdmi_queue_create();



  //  fdmi_proc_man_comm_init(&fdmi_comm_master_global,  psize, 0, numproc, prank);
  fdmi_proc_man_comm_init(&fdmi_comm_master_ignore,  psize, 0, numproc, prank);
  fdmi_proc_man_comm_init(&fdmi_comm_master_exclude, psize, numspare, numproc, prank);
  fdmi_proc_man_comm_init(&fdmi_comm_master_elect,   psize, numspare, numproc, prank);
  //  fdmi_err("size: %d", fdmi_comm_master_elect->comm_size);
  //  fdmi_err("size: %d, %d %d", psize, numspare, numproc);

  /* Initialize a default communicator as elect communicator*/
  fdmi_init_comm_world_elect(&fmi_comm_world);
  
  //  fdmi_err("size: %d", fmi_comm_world->comm_size);
  return;
}


static int fdmi_proc_man_comm_init(struct fdmi_communicator **comm_master, int psize, int numspare, int numproc, int prank)
{
  int i;
  struct fdmi_communicator *commm = (struct fdmi_communicator *)fdmi_malloc(sizeof(struct fdmi_communicator));

  commm->comm_master    = NULL;
  commm->id             = comm_id_counter++;	/*TODOB: this is not thread safe*/
  //  commm->comm_size      = psize - (numspare * numproc);
  commm->comm_size      = psize;
  commm->vrank          = (prank < commm->comm_size)? prank:-1;
  commm->psize		= psize;
  commm->rmap_ptov_size = psize *	EXTRA;
  commm->rmap_ptov      = (int*)fdmi_malloc(sizeof(int) * commm->rmap_ptov_size);
  commm->rmap_vtop_size = psize *	EXTRA;
  commm->rmap_vtop      = (int*)fdmi_malloc(sizeof(int) * commm->rmap_vtop_size);

  for (i = 0; i < psize * EXTRA; i++) {
    commm->rmap_ptov[i] = (i < commm->comm_size)? i:-1;
    commm->rmap_vtop[i] = (i < commm->comm_size)? i:-1;
  }

  *comm_master = commm;

  //  fdmi_dbg("====> ignore:8 => %d, comm_size:%d", comm_master->rmap_vtop[8], comm_master->comm_size);

  return 1;
}

static int fdmi_comm_set(struct fdmi_communicator **ncomm,
			 struct fdmi_communicator *ocomm,
			 int comm_size,
			 int vrank,
			 int psize,
			 int *rmap_ptov,
			 int *rmap_vtop)
{

  struct fdmi_communicator *newcomm;
  
  newcomm = (struct fdmi_communicator*)malloc(sizeof(struct fdmi_communicator));
  newcomm->comm_master    = ocomm;
  newcomm->id             = comm_id_counter++;  /*TODOB: this is not thread safe*/
  newcomm->comm_size      = comm_size;
  newcomm->vrank          = vrank;
  newcomm->psize          = psize;
  newcomm->rmap_ptov_size = ocomm->rmap_ptov_size;
  newcomm->rmap_ptov      = rmap_ptov;
  newcomm->rmap_vtop_size = ocomm->rmap_vtop_size;
  newcomm->rmap_vtop      = rmap_vtop;

  *ncomm = newcomm;

  return 1;  
}

int fdmi_init_comm_world(struct fdmi_communicator *comm_master, struct fdmi_communicator **newcomm)
{
  int i;
  struct fdmi_communicator *ncomm;

  ncomm = (struct fdmi_communicator*)fdmi_malloc(sizeof(struct fdmi_communicator));
  
  ncomm->comm_master    = comm_master;
  ncomm->id             = comm_id_counter++;  /*TODOB: this is not thread safe*/
  ncomm->vrank	       = comm_master->vrank;
  ncomm->psize	       = comm_master->psize;
  ncomm->comm_size      = comm_master->comm_size;
  ncomm->rmap_ptov_size = comm_master->rmap_ptov_size;
  ncomm->rmap_ptov      = (int*)fdmi_malloc(sizeof(int) * ncomm->rmap_ptov_size);
  ncomm->rmap_vtop_size = comm_master->rmap_vtop_size;
  ncomm->rmap_vtop      = (int*)fdmi_malloc(sizeof(int) * ncomm->rmap_vtop_size);


  for (i = 0; i < ncomm->rmap_ptov_size; i++) {
    ncomm->rmap_ptov[i] = (i < ncomm->comm_size)? i:-1;
  }

  for (i = 0; i < ncomm->rmap_vtop_size; i++) {
    ncomm->rmap_vtop[i] = (i < ncomm->comm_size)? i:-1;
  }

  *newcomm = ncomm;
  //  fdmi_dbg("vrank %d, psize %d, size: %d", ncomm->vrank, ncomm->psize, comm_master->comm_size);

  return 1;
}

int fdmi_verbs_comm_dup(struct fdmi_communicator *comm, struct fdmi_communicator **newcomm)
{
  int i;
  struct fdmi_communicator *ncomm;

  ncomm = (struct fdmi_communicator*)fdmi_malloc(sizeof(struct fdmi_communicator));
  
  ncomm->comm_master    = comm->comm_master;
  ncomm->id             = comm_id_counter++;  /*TODOB: this is not thread safe*/
  ncomm->vrank          = comm->vrank;
  ncomm->psize          = comm->psize;
  ncomm->comm_size      = comm->comm_size;
  ncomm->rmap_ptov_size = comm->rmap_ptov_size;
  ncomm->rmap_ptov      = (int*)fdmi_malloc(sizeof(int) * ncomm->rmap_ptov_size);
  ncomm->rmap_vtop_size = comm->rmap_vtop_size;
  ncomm->rmap_vtop      = (int*)fdmi_malloc(sizeof(int) * ncomm->rmap_vtop_size);

  memcpy(ncomm->rmap_ptov, comm->rmap_ptov, sizeof(int) * ncomm->rmap_ptov_size);
  memcpy(ncomm->rmap_vtop, comm->rmap_vtop, sizeof(int) * ncomm->rmap_vtop_size);

  *newcomm = ncomm;
  //  fdmi_dbg("vrank %d, psize %d", newcomm->vrank, newcomm->psize);
  return 1;
}

/* int fdmi_comm_split(struct fdmi_communicator *comm, int color, int key, */
/* 		    struct fdmi_communicator **newcomm) */
/* { */
/*   int size, my_size; */
/*   int vrank; */
/*   int info[2]; */
/*   int *results = NULL, *sorted = NULL; */
/*   int ret; */
/*   int i, loc; */
/*   int *rmap_ptov, *rmap_vtop; */

/*   fdmi_comm_size(comm, &size); */
/*   fdmi_comm_vrank(comm, &vrank); */

/*   results = (int*) malloc(2 * size * sizeof(int)); */

/*   info[0] = color; */
/*   info[1] = key; */

/*   ret = fdmi_allgather(info,    2, FMI_INT,  */
/* 		       results, 2, FMI_INT, */
/* 		       comm); */
/*   if (ret != FMI_SUCCESS) {return ret;} */

/*   /\* how many have the same color like me *\/ */
/*   for (my_size = 0, i=0; i < size; i++) { */
/*     if (results[2*i] == color) { */
/*       my_size++; */
/*     } */
/*   } */

/*   sorted = (int *) malloc ( sizeof( int ) * my_size * 2); */
/*   if ( NULL == sorted) { */
/*     fdmi_err ("malloc failed  (%s:%s:%d)", __FILE__, __func__, __LINE__); */
/*   } */

/*   rmap_ptov = (int*)malloc(comm->psize * EXTRA * sizeof(int)); */
/*   rmap_vtop = (int*)malloc(comm->psize * EXTRA * sizeof(int)); */

/*   for(loc = 0, i = 0; i < size; i++) { */
/*     if (results[(2*i)+0] == color) { */
/*       rmap_vtop[loc] = i; */
/*       rmap_ptov[i] = loc; */
/*       sorted[(2*loc)+0] = i;                 /\* copy org rank *\/ */
/*       sorted[(2*loc)+1] = results[(2*i)+1];  /\* copy key *\/ */
/*       loc++; */
/*     } */
/*   } */

  
/*   fdmi_comm_set(newcomm, */
/*   		comm, */
/*   		my_size, */
/* 		rmap_ptov[vrank], */
/* 		comm->psize, */
/*   		rmap_ptov, */
/* 		rmap_vtop); */

  
/*   /\* if (vrank == 27) { *\/ */
/*   /\*   for(i = 0; i < comm->psize; i++) { *\/ */
/*   /\*     fdmi_dbg("p:%d -> v:%d", i, rmap_ptov[i]); *\/ */
/*   /\*   } *\/ */
/*   /\*   int  count = 2; *\/ */
/*   /\*   fdmi_dbg("color %d: key: %d,rank:%d, master:%d, vrank:%d", color, key, rmap_ptov[vrank], comm->id, vrank); *\/ */
/*   /\*   for(i = 0; i < my_size; i++) { *\/ */
/*   /\*     fdmi_dbg("Color %d: rank: %d key: %d", color, sorted[i*count], sorted[i*count + 1]); *\/ */
/*   /\*   } *\/ */
/*   /\* } *\/ */
    

/*   return FMI_SUCCESS; */
/* } */

/* int fdmi_comm_dsplit(struct fdmi_communicator *comm, int *colors, int color, */
/* 		    struct fdmi_communicator **newcomm) */
/* { */
/*   int size, my_size; */
/*   int vrank; */
/*   int info[2]; */
/*   int *sorted = NULL; */
/*   int ret; */
/*   int i, loc; */
/*   int *rmap_ptov, *rmap_vtop; */

/*   fdmi_comm_size(comm, &size); */
/*   fdmi_comm_vrank(comm, &vrank); */

/*   /\* how many have the same color like me *\/ */
/*   for (my_size = 0, i=0; i < size; i++) { */
/*     if (colors[2*i] == color) { */
/*       my_size++; */
/*     } */
/*   } */

/*   sorted = (int *) malloc ( sizeof( int ) * my_size * 2); */
/*   if ( NULL == sorted) { */
/*     fdmi_err ("malloc failed  (%s:%s:%d)", __FILE__, __func__, __LINE__); */
/*   } */

/*   rmap_ptov = (int*)malloc(comm->psize * EXTRA * sizeof(int)); */
/*   rmap_vtop = (int*)malloc(comm->psize * EXTRA * sizeof(int)); */

/*   for(loc = 0, i = 0; i < size; i++) { */
/*     if (colors[(2*i)+0] == color) { */
/*       rmap_vtop[loc] = i; */
/*       rmap_ptov[i] = loc; */
/*       sorted[(2*loc)+0] = i;                 /\* copy org rank *\/ */
/*       sorted[(2*loc)+1] = colors[(2*i)+1];  /\* copy key *\/ */
/*       loc++; */
/*     } */
/*   } */

  
/*   fdmi_comm_set(newcomm, */
/*   		comm, */
/*   		my_size, */
/* 		rmap_ptov[vrank], */
/* 		comm->psize, */
/*   		rmap_ptov, */
/* 		rmap_vtop); */

  
/*   /\* if (vrank == 27) { *\/ */
/*   /\*   for(i = 0; i < comm->psize; i++) { *\/ */
/*   /\*     fdmi_dbg("p:%d -> v:%d", i, rmap_ptov[i]); *\/ */
/*   /\*   } *\/ */
/*   /\*   int  count = 2; *\/ */
/*   /\*   fdmi_dbg("color %d: key: %d,rank:%d, master:%d, vrank:%d", color, key, rmap_ptov[vrank], comm->id, vrank); *\/ */
/*   /\*   for(i = 0; i < my_size; i++) { *\/ */
/*   /\*     fdmi_dbg("Color %d: rank: %d key: %d", color, sorted[i*count], sorted[i*count + 1]); *\/ */
/*   /\*   } *\/ */
/*   /\* } *\/ */
    

/*   return FMI_SUCCESS; */
/* } */

/* int fdmi_tmp_create_fte_ckpt_comm(struct fdmi_communicator *comm, struct fdmi_communicator **new_comm)  */
/* { */
/*   int i; */
/*   int size, vrank; */
/*   int *colors; */



/*   fdmi_comm_size(comm, &size); */
/*   fdmi_comm_vrank(comm, &vrank); */


/*   colors = (int*)fdmi_malloc(sizeof(int) * size * 2); */

/*   for (i = 0; i < size; i++) { */
/*     int xor_max_gsize = FDMI_CKPT_MAX_XOR_GROUP_SIZE; */
/*     int num_node, xor_node_group_num; */
/*     int rank_node, rank_across_nodes; */
/*     int xor_node_group_color,  xor_group_color; */

/*     rank_across_nodes = i / fdmi_numproc; */
/*     num_node  = fdmi_size /fdmi_numproc; */
/*     xor_node_group_num = num_node / xor_max_gsize; */
/*     if (0 != num_node % xor_max_gsize) xor_node_group_num++; */
/*     xor_node_group_color = rank_across_nodes % xor_node_group_num; */
/*     rank_node = i % fdmi_numproc; */
/*     xor_group_color = xor_node_group_color * fdmi_numproc + rank_node; */
/*     colors[i*2] = xor_group_color; */
/*     colors[i*2 + 1] = vrank; */
/*   } */
/*   fdmi_comm_dsplit(comm, colors, colors[vrank*2], new_comm); */


/*   /\* int new_rank; *\/ */
/*   /\* fdmi_comm_vrank(*new_comm, &new_rank); *\/ */
/*   /\* fdmi_comm_size(*new_comm, &size); *\/ */
/*   /\* usleep(100000 * prank); *\/ */

/*   /\* fdmi_dbg(" new_rank: %d, size; %d", new_rank, size); *\/ */
/*   /\* sleep(1111); *\/ */

/*   fdmi_free(colors); */
/* } */



      
int fdmi_comm_get_vrank(struct fdmi_communicator *comm, int prank)
{
  struct fdmi_communicator *comms[128];
  struct fdmi_communicator *cur_comm;
  int i, index, rank;
  index = 0;

  comms[index] = comm;
  while ((comms[index + 1] = comms[index]->comm_master) != NULL) {
    index++;
    //    fdmi_dbgi(0, "index: %d id: %d", index, comms[index]->id);
  }

  rank = fdmi_prank;
  for (i = index; i >= 0; i--) {
    rank = comms[i]->rmap_ptov[rank];
  }
  return rank;
}

int fdmi_comm_get_prank(struct fdmi_communicator* comm, int vrank)
{
  struct fdmi_communicator *cur_comm;
  int rank;

  if (vrank == -1) return vrank; /*TODOB: We want to use FMI_ANY_SOURCE(= -1) instead*/

  cur_comm = comm;
  rank     = vrank;
  while (cur_comm != NULL) {
    rank = cur_comm->rmap_vtop[rank];
    //    fdmi_dbgi(3, "==== rank: %d", rank);
    cur_comm = cur_comm->comm_master;
  }
  return rank;
}

int fdmi_init_comm_world_ignore(struct fdmi_communicator **comm)
{
  return fdmi_init_comm_world(fdmi_comm_master_ignore, comm);
}

int fdmi_init_comm_world_exclude(struct fdmi_communicator **comm)
{
  return fdmi_init_comm_world(fdmi_comm_master_exclude, comm);
}

int fdmi_init_comm_world_elect(struct fdmi_communicator **comm)
{
  return fdmi_init_comm_world(fdmi_comm_master_elect, comm);
}

void fdmi_update_comm_world(int *failed_offset, int *failed_range,
			    int *elected_offset, int *elected_range, int length)
{
  /*Only support comm_world_elect*/
  struct fdmi_communicator *comm;
  int i,f,e;

  comm = fdmi_comm_master_elect;
  for (i = 0; i < length; i++) {
    for (f = failed_offset[i], e = elected_offset[i]; 
	 f < failed_offset[i] + failed_range[i] &&
	 e < elected_offset[i] + elected_range[i]; 
	 f++, e++) {
      int vrank;
      if (comm->rmap_ptov[f] == -2) continue;
      vrank = comm->rmap_ptov[f];
      comm->rmap_vtop[vrank] = e;
      comm->rmap_ptov[e] = vrank;
      comm->rmap_ptov[f] = -2;
    }
  }
  comm->vrank = comm->rmap_ptov[fdmi_prank];
}

/*Old failed_offset ..., can also included in the array*/
void fdmi_reallocate_state_listd(int *failed_offset, int *failed_range, int failed_length,
				 int *elected_offset, int *elected_range, int elected_length)
{
  int i, j, sid;
  for (i = 0; i < failed_length; i++) {
    for (j = failed_offset[i]; j < failed_offset[i] + failed_range[i]; j++) {
	prank_state_list[j] = FDMI_PFAILURE;
    }
  }
  
  sid = -1;
  while (prank_state_list[++sid] != FDMI_PSTANDBY);

  /*TODO: handle when standby nodes are failed*/
  for (i = elected_length; i < failed_length; i++) {
    elected_offset[i] = sid;
    elected_range[i]  = failed_range[i];
    sid += failed_range[i];
  }

  for (i = elected_length; i < failed_length; i++) {
    for (j = elected_offset[i]; j < elected_offset[i] + elected_range[i]; j++) {
      if (prank_state_list[j] == FDMI_PCOMPUTE) continue;
      if (prank_state_list[j] == FDMI_PSTANDBY) {
	prank_state_list[j] = FDMI_PSTANDBY;
      }
      if (prank_state_list[j] == FDMI_PFAILURE) {
	fdmi_err("Cannot reallocate failed node rank:%d", j);
	
      }
      if (prank_state_list[j] == FDMI_PFAILURE) {
	fdmi_err("No enough standby nodes(index:%d)", j);
      }
    }
  }

}





int fdmi_proc_man_comm_size(struct fdmi_communicator *comm_master, int size)
{
  
}

/*TDOD: use enum !!, for now, use -1 for standby, 0 for compute*/
int fdmi_prank_state() {
  if (fdmi_rmap_is_standby(fdmi_prank)) {
    return -1;
  }
  return 0;
}

void fdmi_rmap_btw_vandp_add(int vrank, int prank) {
  if (vrank >= fdmi_psize * EXTRA || prank >= fdmi_psize * EXTRA) {
    fdmi_err ("Size of vrank or prank exceeded mapping table size (%s:%s:%d)", __FILE__, __func__, __LINE__);
  }
  rmap_vtop[vrank] = prank;
  rmap_ptov[prank] = vrank;
  return;
}

int fdmi_rmap_ptov_get(int prank) {
  return rmap_ptov[prank];
}

int fdmi_rmap_vtop_get(int vrank) {
  return rmap_vtop[vrank];
}

int fdmi_rmap_is_standby(int prank) {
  return prank_state_list[prank] == FDMI_PSTANDBY;
}

void fdmi_rmap_itop_add(void* id, int prank)
{
  /*TODOB: warning*/
  void *pr = (void*)prank;
  fdmi_hashtable_add(hstb_itop, id, pr);
  return;
}

int fdmi_rmap_itop_get(void* id)
{
  return (int)fdmi_hashtable_get(hstb_itop, id);
}

/* int fdmi_rank_man_report_failure(int prank) */
/* { */
/*   int vrank; */
/*   int new_prank, *new_prank_p; */
/*   int new_loop; */

/*   prank_state_list[prank] = FDMI_PFAILURE; */
/*   vrank = fdmi_rmap_ptov_get(prank); */
/*   new_prank = fdmi_get_next_prank(); */
/*   fdmi_rmap_btw_vandp_add(vrank, new_prank); */

/*   //  fdmi_dbg("Process map: (v=p)=(%d=%d) -> (%d=%d)",vrank,prank,vrank,new_prank); */

/*   new_prank_p = (int*)fdmi_malloc(sizeof(int)); */
/*   *new_prank_p = new_prank; */
/*   fdmi_queue_lock_enq(starting_prank_q, (void*)new_prank_p); */
/* } */

int fdmi_proc_update_map(int old_prank, int new_prank, int new_vrank)
{
  prank_state_list[old_prank] = FDMI_PFAILURE;
  fdmi_rmap_btw_vandp_add(new_vrank, new_prank);
}

int fdmi_proc_get_new_vp_pair(int old_prank, int* new_prank, int* new_vrank)
{
  prank_state_list[old_prank] = FDMI_PFAILURE;
  *new_vrank = fdmi_rmap_ptov_get(old_prank);
  *new_prank = fdmi_get_next_prank();
  fdmi_rmap_btw_vandp_add(*new_vrank, *new_prank);
}

int fdmi_rank_man_get_starting_prank()
{
  int prank, *prank_p;
  prank_p =  (int*)fdmi_queue_lock_deq(starting_prank_q);
  if (prank_p == NULL) {
    prank = -1;
  } else {
    prank = *prank_p;
    fdmi_free(prank_p);
  }
  return prank;
}

int fdmi_get_next_prank(void)
{
  int i;
  int next_prank;
  for (i = 0; i < fdmi_psize; i++) {
    next_prank = (fdmi_next_prank + i) % fdmi_psize;
    if (prank_state_list[next_prank] == FDMI_PSTANDBY) {
      prank_state_list[next_prank] = FDMI_PCOMPUTE;
      fdmi_next_prank = next_prank + 1;
      return next_prank;
    }
  }
}
