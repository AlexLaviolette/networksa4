all: server client

server: librcs.a tcp-server.o
	gcc -o server tcp-server.o -L. -lrcs -lucp -pthread -lstdc++

client: librcs.a tcp-client.o
	gcc -o client tcp-client.o -L. -lrcs -lucp -lstdc++

librcs.a: rcs.o rcsmap.o
	ar rvs librcs.a rcs.o rcsmap.o

libucp.a: ucp.o mybind.o
	ar rvs libucp.a ucp.o mybind.o

clean:
	rm -rf *.o *.a server client
