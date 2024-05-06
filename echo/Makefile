# Makefile for echoservice and echoclient
# Compiler and flags
CC = gcc
CFLAGS = -O2 -Wall -I .
LIB = -lpthread
# Targets
all: echoserver echoclient

echoserver: echoserver.o csapp.o
	$(CC) $(CFLAGS) -o echoserver echoserver.c csapp.o $(LIB)

echoclient: echoclient.o csapp.o
	$(CC) $(CFLAGS) -o echoclient echoclient.c csapp.o $(LIB)

csapp.o: csapp.c
	$(CC) $(CFLAGS) -c csapp.c
	
clean:
	rm -f *.o echoservice echoclient