RCS_OBJS = rcs.o rcsmap.o rcsconn.o outqueue.o packet.o
UCP_OBJS = ucp.o mybind.o
LINK_FLAGS = -L. -lrcs -lucp -pthread -lstdc++

all: rcsapp

rcsapp: rcsapp-server rcsapp-client

rcsapp-server: librcs.a rcsapp-server.o libucp.a
	gcc -o rcsapp-server rcsapp-server.o $(LINK_FLAGS)

rcsapp-client: librcs.a rcsapp-client.o libucp.a
	gcc -o rcsapp-client rcsapp-client.o $(LINK_FLAGS)

librcs.a: $(RCS_OBJS)
	ar rvs librcs.a $(RCS_OBJS)

libucp.a: $(UCP_OBJS)
	ar rvs libucp.a $(UCP_OBJS)

clean:
	rm -rf *.o *.a [0-9]* rcsapp-client rcsapp-server
