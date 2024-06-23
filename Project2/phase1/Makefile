CC = gcc
CFLAGS = -Og
LDLIBS = -lpthread

PROGS = myshell

all: $(PROGS)

myshell: myshell.c csapp.c

.PHONY: clean

clean:
	rm -rf *~ $(PROGS)
