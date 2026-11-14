CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pedantic -O2
LDFLAGS =

all: awale_server awale_client

awale_server: server.o awale.o
	$(CC) $(CFLAGS) -o $@ server.o awale.o $(LDFLAGS)

awale_client: client.o
	$(CC) $(CFLAGS) -o $@ client.o $(LDFLAGS)

server.o: server.c awale.h
	$(CC) $(CFLAGS) -c server.c

awale.o: awale.c awale.h
	$(CC) $(CFLAGS) -c awale.c

client.o: client.c
	$(CC) $(CFLAGS) -c client.c

clean:
	rm -f *.o awale_server awale_client

.PHONY: all clean
