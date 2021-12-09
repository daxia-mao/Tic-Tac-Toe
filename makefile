SHELL = /bin/bash
CC = gcc
CFLAGS = -g
#CFLAGS = -g -pthread
SRC = $(wildcard *.c)
EXE = $(patsubst %.c, %, $(SRC))

all: ${EXE}

%:	%.c
	${CC} ${CFLAGS} $@.c -o $@ -pthread

clean:
	rm ${EXE}
