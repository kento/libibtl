
RDMA Communication Runtime
==========================

This runtime is a client/server model communication runtime over infiniband interconntects.
The runtime is develeped by using ibverbs.



Directly Structure
------------------
* libibtl  
    * src    : Directory for source codes  
    * include: Directory for header files  
    * example: Directory for example codes  



HOW to Build & Install
------------
* Run commands below
    $ ./configure --prefix=/path/to/install/dir
    $ make 
    $ make install



Quick Start
-------------
### Build examples ###

* Run commands below  
    $ cd /path/to/install/dir
    $ cd examples

* Edit makefile (examples/makefile)  
    INSTALL_DIR = /path/to/install/dir

* Make  
    $ make

### Run example 1: Simple communication ###
The example codes simply exchanges messages (Ping-Pong) initiated by a client side

* Run server code  
    sierra0$ ./example_server
* Run client cond    
    sierra1$ ./example_client sierra0

### Run example 2: RDMA I/O  ###
The client example code wirte/read a spedified file on the remote server.

* Run server code    
    sierra0$ ./ibio_server
* Run client code  
    sierra1$ ./ibio_test sierra0:/path/to/file 0 # write
    sierra1$ ./ibio_test sierra0:/path/to/file 1 # read

RDMA Communication APIs & Variables
-----------------------
# APIs

#### Initialization
    int fdmi_verbs_init(int *argc, char ***argv)  
This function must be called before any communication functions  
* `argc` [input]: Pointer to the number of arguments  
* `argv` [input]: Argument vector  
    
#### Finalization
    int fdmi_verbs_finalize()  
This function finalize the communications

#### Connection (Used by only clients )
    void fdmi_connection* fdmi_verbs_connect(int rank, char *hostname);
This function make connection to a specified server, create mapping from `rank` to `hostname`.
`rank` is used by the rest of communication function calls (send/redv).
* `rank` [input]: integer to assinge the host
* `hostname` [input]: a server to connect
    
#### Send
    int fdmi_verbs_isend (const void* buf, int count, struct fdmi_datatype dataype, int dest, int tag, struct fdmi_communicator *comm, struct fdmi_request* request);
This function provides non-blocking send.
* `buf` [input]:
* `count` [input]:
* `datatype` [input]: 


    int fdmi_verbs_irecv(const void* buf, int count, struct fdmi_datatype dataype,  int source, int tag, struct fdmi_communicator *comm, struct fdmi_request* request);
    int fdmi_verbs_test(struct fdmi_request *request, struct fdmi_status *staus);
    int fdmi_verbs_wait(struct fdmi_request* request, struct fdmi_status *status);
    int fdmi_verbs_iprobe(int source, int tag, struct fdmi_communicator* comm, int *flag, struct fdmi_status *status);

# Variables