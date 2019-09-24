CC = g++
CFLAGS = -g -std=c++11

all: nameserver

nameserver: main.cpp
	$(CC) $(CFLAGS) -o $@ $^

client: client.cpp
	$(CC) $(CFLAGS) -o $@ $^

clean:
	-rm -f *.o *~ *core* nameserver