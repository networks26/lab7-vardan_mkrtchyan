CC = cc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O0 -g

all: server poll_server

server: server.c
	$(CC) $(CFLAGS) -o server server.c

poll_server: poll_server.c
	$(CC) $(CFLAGS) -o poll_server poll_server.c

clean:
	rm -f server poll_server

.PHONY: all clean
