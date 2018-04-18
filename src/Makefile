.PHONY: clean

CFLAGS  := -Wall -g -O0 # -Werror
LD      := gcc
LDFLAGS := ${LDFLAGS} -libverbs -lrdmacm

APPS    := rdma_client rdma_server

all: ${APPS}

rdma_client: rdma_client.o
	${LD} -o $@ $^ ${LDFLAGS}

rdma_server: rdma_server.o
	${LD} -o $@ $^ ${LDFLAGS}

clean:
	rm -f *.o ${APPS}

