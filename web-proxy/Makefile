CCFLAGS = -std=c++11 -pthread -pedantic -Wall -Werror -ggdb3

myproxy:myproxy.o
	g++ -o myproxy $(CCFLAGS) myproxy.o
myproxy.o:myproxy.cpp
	g++ $(CCFLAGS) -c myproxy.cpp

clean:
	rm -f *~ *.o myproxy
