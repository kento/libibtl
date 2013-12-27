# ================
# Building for debuging
# ================

SUBDIRS = ../src
bin_PROGRAMS = example_client example_server ibio_test ibio_server 
SRC_DIR=../src
INCLUDE_DIR=../include 

example_client_SOURCES =  example_client.c
example_client_CFLAGS = -I$(INCLUDE_DIR) -I../src
example_client_LDADD = ${SRC_DIR}/libibtls.a  ${LDFLAGS} -lrdmacm -lpthread

example_server_SOURCES =  example_server.c
example_server_CFLAGS = -I$(INCLUDE_DIR) -I../src
example_server_LDADD = ${SRC_DIR}/libibtls.a  ${LDFLAGS} -lrdmacm -lpthread

ibio_test_SOURCES =  ibio_test.c ibio_client.c ibio_common.c
ibio_test_CFLAGS = -I$(INCLUDE_DIR) -I../src
ibio_test_LDADD = ${SRC_DIR}/libibtls.a  ${LDFLAGS} -lrdmacm -lpthread

ibio_server_SOURCES =  ibio_server.c ibio_common.c  yarn.c pgz.c
ibio_server_CFLAGS = -I$(INCLUDE_DIR) -I../src
ibio_server_LDADD = ${SRC_DIR}/libibtls.a  ${LDFLAGS} -lrdmacm -lpthread -lz


# ================
# For installation
# ================

examplesdir = $(exec_prefix)/examples

dist_examples_DATA = \
	ibio_test.c \
	ibio_server.c \
	ibio_client.c \
	ibio_client.h \
	ibio_common.c \
	ibio_common.h \
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

