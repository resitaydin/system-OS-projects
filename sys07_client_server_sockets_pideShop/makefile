all: server client

server: server.c
	gcc -pthread -Wall -o server server.c

client: client.c
	gcc -pthread -Wall -o client client.c

clean:
	rm -f server client *.log

