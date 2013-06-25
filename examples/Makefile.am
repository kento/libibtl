bin_PROGRAMS = rdma_server_test \
	       rdma_client_test \
	       rdma_client_latency \
	       rdma_server_latency \
	       rdma_client_silent \
	       rdma_server_silent \
	       rdma_client_offset \
	       rdma_server_offset 

SRC_DIR=../src
INCLUDE_DIR=../include
TARGET=	$(SRC_DIR)/list_queue.c	$(INCLUDE_DIR)/list_queue.h $(SRC_DIR)/rdma_ppool.c $(INCLUDE_DIR)/rdma_ppool.h $(SRC_DIR)/rdma_client.c $(INCLUDE_DIR)/rdma_client.h $(SRC_DIR)/rdma_server.c $(INCLUDE_DIR)/rdma_server.h $(SRC_DIR)/rdma_common.c $(INCLUDE_DIR)/rdma_common.h $(SRC_DIR)/common.c $(INCLUDE_DIR)/common.h $(SRC_DIR)/ibtls.c $(INCLUDE_DIR)/ibtls.h 


rdma_client_test_SOURCES =  rdma_client_test.c $(TARGET)
rdma_client_test_CFLAGS = -I$(INCLUDE_DIR)
rdma_client_test_LDADD = ${LDFLAGS} -lrdmacm -lpthread

rdma_server_test_SOURCES = rdma_server_test.c $(TARGET)
rdma_server_test_CFLAGS = -I$(INCLUDE_DIR)
rdma_server_test_LDADD = ${LDFLAGS} -lrdmacm -lpthread

rdma_client_latency_SOURCES =  rdma_client_latency.c $(TARGET)
rdma_client_latency_CFLAGS = -I$(INCLUDE_DIR)
rdma_client_latency_LDADD = ${LDFLAGS} -lrdmacm -lpthread

rdma_server_latency_SOURCES = rdma_server_latency.c $(TARGET)
rdma_server_latency_CFLAGS = -I$(INCLUDE_DIR)
rdma_server_latency_LDADD = ${LDFLAGS} -lrdmacm -lpthread

rdma_client_silent_SOURCES =  rdma_client_silent.c $(TARGET)
rdma_client_silent_CFLAGS = -I$(INCLUDE_DIR)
rdma_client_silent_LDADD = ${LDFLAGS} -lrdmacm -lpthread

rdma_server_silent_SOURCES = rdma_server_silent.c $(TARGET)
rdma_server_silent_CFLAGS = -I$(INCLUDE_DIR)
rdma_server_silent_LDADD = ${LDFLAGS} -lrdmacm -lpthread

rdma_client_offset_SOURCES =  rdma_client_offset.c $(TARGET)
rdma_client_offset_CFLAGS = -I$(INCLUDE_DIR)
rdma_client_offset_LDADD = ${LDFLAGS} -lrdmacm -lpthread

rdma_server_offset_SOURCES = rdma_server_offset.c $(TARGET)
rdma_server_offset_CFLAGS = -I$(INCLUDE_DIR)
rdma_server_offset_LDADD = ${LDFLAGS} -lrdmacm -lpthread

# ================
# For installation
# ================

examplesdir = $(exec_prefix)/examples

dist_examples_DATA = \
	rdma_client_test.c \
	rdma_server_test.c \
	rdma_client_offset.c \
	rdma_server_offset.c \
	rdma_client_silent.c \
	rdma_server_silent.c \
	rdma_client_latency.c \
	rdma_server_latency.c 

EXTRA_DIST = makefile.example

# Use -hook insted of -local b/c we need to do this AFTER the main install data rule.
install-data-hook:
	$(INSTALL_DATA) makefile.example $(DESTDIR)$(examplesdir)/makefile

uninstall-local:
	-rm -f $(DESTDIR)$(examplesdir)/makefile


