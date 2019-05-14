all: user server

program1: user.c
	gcc -Wall -o user user.c

program2: server.c
	gcc  server.c -o  server  -D_REENTRANT -lpthread -Wall

clean:
	-rm -f *.o
	-rm -f user
	-rm -f server