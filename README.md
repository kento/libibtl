Infiniband-based RDMA communication runtime
==========================================


Build libibtl
---------------
./configure --prefix=/path/to/install/dir
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