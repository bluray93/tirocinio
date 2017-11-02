
.PHONY:clean

all: server

server:server.c
	gcc -g -Wall server.c -o server `pkg-config libwebsockets --libs --cflags`

clean:
	-rm server
