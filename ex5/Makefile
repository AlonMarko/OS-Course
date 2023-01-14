INCS =-I.
CFLAGS = -Wall -std=c++11 -g $(INCS)

all: container sockets

container:
		g++ $(CFLAGS) container.cpp -o container

sockets:
		g++ $(CFLAGS) sockets.cpp -o sockets

clean:
		rm  -f container sockets
		
tar:
		tar -cvf ex5.tar sockets.cpp container.cpp Makefile README
