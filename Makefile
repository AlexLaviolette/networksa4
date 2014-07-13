server: librcs.a tcp-server.o
	gcc -o server tcp-server.o -L. -lrcs -pthread -lstdc++

client: librcs.a tcp-client.o
	gcc -o client tcp-client.o -L. -lrcs -lstdc++

librcs.a: rcs.o rcsmap.o
	ar rvs librcs.a rcs.o rcsmap.o

clean:
	rm -rf *.o *.a server client
