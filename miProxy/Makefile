CC = g++
CFLAGS = -g -std=c++11

all: miProxy

miProxy: miProxy.cpp
	$(CC) $(CFLAGS) -o $@ $^

clean:
	-rm -f *.o *~ *core* miProxy