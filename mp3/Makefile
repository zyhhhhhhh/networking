Makefile:

CC=gcc
CCOPTS = -Wall -Wextra -g
LINKOPTS = -lpthread

all: reliable_sender reliable_receiver

clean:
	rm -f *.o reliable_sender reliable_receiver

reliable_sender: sender.o
	$(CC) $(CCOPTS) $(LINKOPTS) sender.o -o $@

reliable_receiver: receiver.o
	$(CC) $(CCOPTS) $(LINKOPTS) receiver.o -o $@

sender.o: sender_main.c
	$(CC) $(CCOPTS) -c $< -o $@

receiver.o: receiver_main.c
	$(CC) $(CCOPTS) -c $< -o $@
