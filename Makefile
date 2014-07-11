librcs.a:
	g++ -c rcs.cpp rcsmap.cpp

	ar rvs librcs.a rcs.o rcsmap.cpp

clean:
	rm -rf *.o *.a