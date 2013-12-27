SUBDIRS = ../src

bin_PROGRAMS = ibtl_io ibtl_io_server

SRC_DIR=../src
INCLUDE_DIR=../include 
TARGET=	$(SRC_DIR)/list_queue.c	$(INCLUDE_DIR)/list_queue.h $(SRC_DIR)/rdma_ppool.c $(INCLUDE_DIR)/rdma_ppool.h $(SRC_DIR)/rdma_client.c $(INCLUDE_DIR)/rdma_client.h $(SRC_DIR)/rdma_server.c $(INCLUDE_DIR)/rdma_server.h $(SRC_DIR)/rdma_common.c $(INCLUDE_DIR)/rdma_common.h $(SRC_DIR)/common.c $(INCLUDE_DIR)/common.h $(SRC_DIR)/ibtls.c $(INCLUDE_DIR)/ibtls.h 

ibtl_io_SOURCES =  ibtl_io.c ibtl_io_client.c ibvio_common.c
ibtl_io_CFLAGS = -I$(INCLUDE_DIR) -I../src
ibtl_io_LDADD = ${SRC_DIR}/libibtls.a  ${LDFLAGS} -lrdmacm -lpthread

ibtl_io_server_SOURCES =  ibtl_io_server.c ibvio_common.c  yarn.c pgz.c
ibtl_io_server_CFLAGS = -I$(INCLUDE_DIR) -I../src
ibtl_io_server_LDADD = ${SRC_DIR}/libibtls.a  ${LDFLAGS} -lrdmacm -lpthread -lz


# ================
# For installation
# ================

examplesdir = $(exec_prefix)/examples

dist_examples_DATA = \
	ibtl_io.c \
	ibtl_io_server.c \
	ibtl_io_client.c \
	ibtl_io_client.h \
	ibvio_common.c \
	ibvio_common.h \
	pgz.c \
	pgz.h \
	yarn.c \
	yarn.h 

EXTRA_DIST = makefile.example

# Use -hook insted of -local b/c we need to do this AFTER the main install data rule.
install-data-hook:
	$(INSTALL_DATA) makefile.example $(DESTDIR)$(examplesdir)/makefile

uninstall-local:
	-rm -f $(DESTDIR)$(examplesdir)/makefile

clean-local:
	rm -f *.core

