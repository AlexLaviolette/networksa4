all: server client

rcsapp: rcsapp-server rcsapp-client

rcsapp-server: librcs.a rcsapp-server.o libucp.a
	gcc -o rcsapp-server rcsapp-server.o -L. -lrcs -lucp -pthread -lstdc++

rcsapp-client: librcs.a rcsapp-client.o libucp.a
	gcc -o rcsapp-client rcsapp-client.o -L. -lrcs -lucp -lstdc++

server: librcs.a tcp-server.o libucp.a
	gcc -o server tcp-server.o -L. -lrcs -lucp -pthread -lstdc++

client: librcs.a tcp-client.o libucp.a
	gcc -o client tcp-client.o -L. -lrcs -lucp -lstdc++

librcs.a: rcs.o rcsmap.o rcsconn.o outqueue.o packet.o
	ar rvs librcs.a rcs.o rcsmap.o rcsconn.o outqueue.o packet.o

libucp.a: ucp.o mybind.o
	ar rvs libucp.a ucp.o mybind.o

clean:
	rm -rf *.o *.a [0-9]* server client rcsapp-client rcsapp-server
