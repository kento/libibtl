====================================================
RDMA Communication Runtime
====================================================

This runtime is a client/server model communication runtime over infiniband interconntects.
The runtime is develeped by using ibverbs.

----------------------------------------------------
Directly Structure
----------------------------------------------------
libibtl  
 |---- src    : Directory for source codes  
 |---- include: Directory for header files  
 |---- example: Directory for example codes  


----------------------------------------------------
HOW to Build
----------------------------------------------------
    ./configure --prefix=/path/to/install/dir
    make 
    make install

----------------------------------------------------
Quickto Start
----------------------------------------------------
    cd /path/to/install/dir
    cd examples
    make

Quick Start
------------

IBIO Server side:
source: ./src/ibtl_io_server.c
binary: ./src/ibtl_io_server

IBIO Client APIs:
source: ./src/ibtl_io_client.c

IBIO Test code:
source: ./example/ibtl_io.c
