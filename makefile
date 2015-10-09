CFLAGS=-Wall -g -std=gnu++0x -pthread

all: amted_server shell

amted_server: amted_server.cc
	g++ $(CFLAGS) -o 550server amted_server.cc

shell: shell.cc
	g++ $(CFLAGS) -o shell shell.cc

clean:
	rm -rf 550server shell *~ *.o *.dSYM
