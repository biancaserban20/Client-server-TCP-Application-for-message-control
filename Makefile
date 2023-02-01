all: server subscriber

server: server.c
		gcc server.c -lm -o server
subscriber: client.c
		gcc client.c -lm -o subscriber
clean: 
		rm -rf server subscriber