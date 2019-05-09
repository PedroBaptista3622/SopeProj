all: user server

program1: user.c
	gcc -wall -o user user.c

program2: server.c
	gcc -wall -o server server.c

clean:
	-rm -f *.o
	-rm -f user
	-rm -f server