compile: a1jobs a1mon

a1jobs: a1jobs.cpp
	g++ a1jobs.cpp -o a1jobs.o -g -std=c++14

a1mon: a1mon.cpp
	g++ a1mon.cpp -o a1mon.o -g -std=c++14

tar: clean
	cd .. && tar -czf submit.tar.gz submit
clean:
	rm -rf *.o
