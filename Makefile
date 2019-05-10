all: user server

program1: user.c
	gcc -Wall -o user user.c

program2: server.c
	gcc -Wall -o  server server.c

clean:
	-rm -f *.o
	-rm -f user
	-rm -f server