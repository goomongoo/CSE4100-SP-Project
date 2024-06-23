CC = gcc
CFLAGS = -Og
LDLIBS = -lpthread

PROGS = myshell

all: $(PROGS)

myshell: myshell.c csapp.c list.c

.PHONY: clean

clean:
	rm -rf *~ $(PROGS)
