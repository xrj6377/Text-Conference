CC	 	= gcc
FLAGS	= -O2
LDFLAGS = -pthread

all:
	gcc -pthread -o server/server server/server.c
	gcc -pthread -o client/client client/client.c
server:
	$(CC) $(FLAGS) server/server.c -o server/server
deliver:
	$(CC) $(FLAGS) client/client.c -o client/client
clean:
	rm -rf server/server client/client