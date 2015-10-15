CFLAGS=-Wall -g -std=gnu++0x -pthread

all: shell

shell: shell.cc
	g++ $(CFLAGS) -o shell shell.cc

clean:
	rm -rf shell *~ *.o *.dSYM
