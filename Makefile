server: librcs.a
	gcc -o server tcp-server.c -L. -lrcs -pthread -lstdc++

client: librcs.a
	gcc -o client tcp-client.c -L. -lrcs -lstdc++

librcs.a: rcs.o rcsmap.o
	ar rvs librcs.a rcs.o rcsmap.o

clean:
	rm -rf *.o *.a server client
