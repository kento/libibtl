SRC_DIR=../src
INCLUDE_DIR=../include

lib_LIBRARIES = libibtls.a

libibtls_a_SOURCES = \
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

libibtls_a_LIBADD = @LIBOBJS@
libibtls_a_CFLAGS = -I$(INCLUDE_DIR)
#libibtls_a_LDADD = ${LDFLAGS} -lrdmacm -lpthread

