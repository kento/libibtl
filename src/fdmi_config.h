#ifndef _FDMI_CONFIG_H
#define _FDMI_CONFIG_H


/* default BTL byte transfer layer type such as verbs, psm */ 
#define FDMI_BTL_TYPE_VERBS "verbs"
#define FDMI_BTL_TYPE_PSM "psm"

#ifndef FDMI_BTL_TYPE
#define FDMI_BTL_TYPE FDMI_BTL_TYPE_PSM
#endif

/* How much FMI accomodate additional process can join
 * after the job lanch. 
 * Default=2:
 *    2 x the starting # of processes
*/
#define FDMI_PSPARE_BUFFSIZE_SCALE (2) 


/**
 * ==================
 * FMI Configuration
 * ================== 
 **/

#define FMI_ANY_SOURCE (-1)
#define FMI_ANY_TAG    (-1)

#define FDMI_COLL_TAG_ALLGATHER       (-10)
#define FDMI_COLL_TAG_ALLGATHERV      (-11)
#define FDMI_COLL_TAG_ALLREDUCE       (-12)
#define FDMI_COLL_TAG_ALLTOALL        (-13)
#define FDMI_COLL_TAG_ALLTOALLV       (-14)
#define FDMI_COLL_TAG_ALLTOALLW       (-15)
#define FDMI_COLL_TAG_BARRIER         (-16)
#define FDMI_COLL_TAG_BCAST           (-17)
#define FDMI_COLL_TAG_EXSCAN          (-18)
#define FDMI_COLL_TAG_GATHER          (-19)
#define FDMI_COLL_TAG_GATHERV         (-20)
#define FDMI_COLL_TAG_REDUCE          (-21)
#define FDMI_COLL_TAG_REDUCE_SCATTER  (-22)
#define FDMI_COLL_TAG_SCAN            (-23)
#define FDMI_COLL_TAG_SCATTER         (-24)
#define FDMI_COLL_TAG_SCATTERV        (-25)

#define FDMI_P2P_TIMEOUT_IN_SEC       (1)
#define FDMI_FT_RECOVERY_MSG_TIMEOUT_IN_SEC (5)

#define FDMI_SIGNUM_FT (30)

#define FDMI_CKPT_MAX_XOR_GROUP_SIZE (16)
#define FDMI_CKPT_MTBF_IN_SEC        (60)
 
/**
 * ==================
 * Overlay Configuration
 * ================== 
 **/

#define VERBS_NO_OVERLAY (0)
#define VERBS_ONDEMAND (1)
#define VERBS_RING (2)
#define VERBS_LOGRING (3)
#define FDMI_PSM_FT_OVERLAY_NETWORK_TYPE  VERBS_ONDEMAND


/**
 * ==================
 * PSM Configuration
 * ================== 
 **/
/**
 * 64-bit tag formed by packing the triple:
 *
 * ( CONTEXT_ID 16bits | RANK 16bits | TAG 32bits )
 **/
#define FDMI_PSM_TAG_BITS                   (64)
#define FDMI_PSM_CONTEXT_ID_TAG_MASK        (0xffffULL)
#define FDMI_PSM_CONTEXT_ID_TAG_BITS_OFFSET (48)
#define FDMI_PSM_RANK_TAG_MASK              (0xffffULL)
#define FDMI_PSM_RANK_TAG_BITS_OFFSET       (32)
#define FDMI_PSM_EPOCH_TAG_MASK             (0xffffULL)
#define FDMI_PSM_EPOCH_TAG_BITS_OFFSET      (16)
#define FDMI_PSM_TAG_TAG_MASK               (0xffffULL)
#define FDMI_PSM_TAG_TAG_BITS_OFFSET        (0)

/*timeout valune for psm_connect; < 1 is waiting forever*/
#define FDMI_PSM_CONNECTION_TIME_OUT_IN_NANO_SEC (200 * 1000) 
/*Cool down time before finalization after a failure*/
#define FDMI_PSM_SLEEP_TIME_BEFORE_FINALIZATION (3)

/*Disconnection detection by ibverbs layer*/
#define FDMI_PSM_VERBS_FT (1)
/*How frequently psm check ibverbs events in usec*/
#define FDMI_PSM_VERBS_HEARTBEAT_INTERVAL_IN_USEC (100 * 1000)

/**
 * ==================
 * ibverbs Configuration
 * ================== 
 **/

/*Scale number to the number of processes*/
#define FDMI_VERBS_PROCESS_MAP_SIZE_SCALE   (4)

#define FDMI_VERBS_LISTEN_BACKLOG (10)
#define FDMI_VERBS_HOSTNAME_LEN   (128)
#define FDMI_VERBS_MAX_EVENTS     (1)

/*Timeout used for rdma_resove_addr(...) */
#define FDMI_VERBS_RESOLVE_ADDR_TIMEOUT_IN_MS (10000)
/*Timeout used for rdma_resove_route(...) */
#define FDMI_VERBS_RESOLVE_ROUTE_TIMEOUT_IN_MS (10)

/*Parameters for connection rdma_connect(...)*/
#define FDMI_VERBS_CONN_RESPONDER_RESOURCES (10)
#define FDMI_VERBS_CONN_INITIATOR_DEPTH     (10)
#define FDMI_VERBS_CONN_RNR_RETRY_COUNT      (7)  /*7 = infinite retry */
#define FDMI_VERBS_CONN_RETRY_COUNT          (7)  /*7 = infinite retry */

/*Parameter for a queie pair: rdma_create_qp(...)*/
#define FDMI_VERBS_QP_MAX_SEND_WR           (10)
#define FDMI_VERBS_QP_MAX_RECV_WR           (10)
#define FDMI_VERBS_QP_MAX_SEND_SGE           (2)
#define FDMI_VERBS_QP_MAX_RECV_SGE           (2)

/*How many times to try connection until deciding a peer is failed*/
#define FDMI_VERBS_CONN_RETRY                (10)

/**
 * ==================
 * Other Configuration
 * ================== 
 **/

#define FDMI_CONFIG_ENVS_SIZE           (256)


/**
 * ==================
 * Variables etc.
 * ==================
 **/



int prank;           /* Physical rank */
int fdmi_size;       /* Total # of physical ranks */
int fdmi_numproc;    /* # of physical ranks per node */
int fdmi_numspare;   /* Total # of stand-by ranks */
int fdmi_id;         /* Uniqe ID for this run */
int fdmi_restart;
int fdmi_epoch;

/*argc of main(argc, argv)*/
int *fdmi_argc;
/*argv of main(argc, argv)*/
char ***fdmi_argv;
/*Enviromental vaiables list*/
extern char** environ;

int fdmi_config_init(int *argc, char ***argv);


#endif

