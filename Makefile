xpto: user server

user:
	gcc -Wall -o user user.c log.c user_functions.c aux_functions.c

server:
	gcc server.c log.c aux_functions.c server_functions.c -o server -D_REENTRANT -lpthread -Wall

clean:
	-rm -f *.o
	-rm -f user
	-rm -f server