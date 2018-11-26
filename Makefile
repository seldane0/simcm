CFLAGS=-Wall

all: server client

server: server.o 
	gcc -o server server.o -lrdmacm -libverbs

server.o: server.c 
	gcc $(CFLAGS) -c server.c 

client: client.o 
	gcc -o client client.o -lrdmacm

client.o: client.c 
	gcc $(CFLAGS) -c client.c 

clean:
	rm -f server client *.o
