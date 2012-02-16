bin_PROGRAMS = rdma_server_test rdma_client_test

SRC_DIR=../src
INCLUDE_DIR=../include

rdma_client_test_SOURCES = \
	rdma_client_test.c \
	$(SRC_DIR)/list_queue.c \
	$(INCLUDE_DIR)/list_queue.h \
	$(SRC_DIR)/rdma_client.c \
	$(INCLUDE_DIR)/rdma_client.h \
	$(SRC_DIR)/rdma_server.c \
	$(INCLUDE_DIR)/rdma_server.h \
	$(SRC_DIR)/rdma_common.c \
	$(INCLUDE_DIR)/rdma_common.h \
	$(SRC_DIR)/common.c \
	$(INCLUDE_DIR)/common.h \
	$(SRC_DIR)/ibtls.c \
	$(INCLUDE_DIR)/ibtls.h 



rdma_client_test_CFLAGS = -I$(INCLUDE_DIR)
rdma_client_test_LDADD = ${LDFLAGS} -lrdmacm -lpthread

rdma_server_test_SOURCES = \
	rdma_server_test.c \
	$(SRC_DIR)/list_queue.c \
	$(INCLUDE_DIR)/list_queue.h \
	$(SRC_DIR)/rdma_server.c \
	$(INCLUDE_DIR)/rdma_server.h \
	$(SRC_DIR)/rdma_client.c \
	$(INCLUDE_DIR)/rdma_client.h \
	$(SRC_DIR)/rdma_common.c \
	$(INCLUDE_DIR)/rdma_common.h \
	$(SRC_DIR)/common.c \
	$(INCLUDE_DIR)/common.h \
	$(SRC_DIR)/ibtls.c \
	$(INCLUDE_DIR)/ibtls.h

rdma_server_test_CFLAGS = -I$(INCLUDE_DIR)
rdma_server_test_LDADD = ${LDFLAGS} -lrdmacm -lpthread