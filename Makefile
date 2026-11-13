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

Commands: LIST - Lists users that are currently online
CHALLENGE user - Challenges a user to a game
ACCEPT user - Accept a user's challenge
REFUSE user - Refuse a user's challenge
MOVE pit(0-5) - Make a move from pit 0-5 (left to right for P1 and right to left P2)
GAMES - Shows a list of live games with their match ID
WATCH id - Watch the match of the id provided
SAY user message - Send the user a private message if their profile is public
BIO text - Set your bio
GETBIO user - Get the bio of a diff user
FRIEND user - Friend the user
PRIVATE on/off - Set your privacy to public or private affecting if random people can join/spectate your games
QUIT - Quit the server
