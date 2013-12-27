
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
* Run server code  
    sierra0$ ./example_server
* Run client cond    
    sierra1$ ./example_client sierra0

### Run example 2: RDMA I/O  ###
* Run server code    
    sierra0$ ./ibio_server
* Run client code  
    sierra1$ ./ibio_test sierra0:/path/to/file 0 # write
    sierra1$ ./ibio_test sierra0:/path/to/file 1 # read
  

RDMA Communication APIs
-----------------------

IBIO Server side:
source: ./src/ibtl_io_server.c
binary: ./src/ibtl_io_server

IBIO Client APIs:
source: ./src/ibtl_io_client.c

IBIO Test code:
source: ./example/ibtl_io.c
