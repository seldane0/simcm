CFLAGS=-Wall

server: server.o 
	gcc -o server server.o -lrdmacm

server.o: server.c 
	gcc $(CFLAGS) -c server.c 

clean:
	rm -f server *.o
