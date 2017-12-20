# CS438 - spring 2016 MP1
#
# NOTE: if you decide to write your solution in C++, you will have to change the compiler 
# in this file. 

CC=gcc
FLAGS=-g3
LIBS=-lpthread

all: http_client http_server

.phony: clean 

http_client: http_client.c
	$(CC) $(FLAGS) http_client.c -o http_client $(LIBS)

http_server: http_server.c
	$(CC) $(FLAGS) http_server.c -o http_server $(LIBS)

clean: 
	rm -rf *.o http_client http_server 