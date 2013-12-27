
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

RDMA Communication APIs
-----------------------
    int fdmi_verbs_init(int *argc, char ***argv)  
This function must be called before any communication functions  
- `argc`: Pointer to the number of arguments
- `argv`: Argument vector  
    
     
kmc:    
    int fdmi_verbs_finalize()  
This function finalize the communications

    void fdmi_connection* fdmi_verbs_connect(int rank, char *hostname);

    int fdmi_verbs_isend (const void* buf, int count, struct fdmi_datatype dataype, int dest, int tag, struct fdmi_communicator *comm, struct fdmi_request* request);
    int fdmi_verbs_irecv(const void* buf, int count, struct fdmi_datatype dataype,  int source, int tag, struct fdmi_communicator *comm, struct fdmi_request* request);
    int fdmi_verbs_test(struct fdmi_request *request, struct fdmi_status *staus);
    int fdmi_verbs_wait(struct fdmi_request* request, struct fdmi_status *status);
    int fdmi_verbs_iprobe(int source, int tag, struct fdmi_communicator* comm, int *flag, struct fdmi_status *status);

