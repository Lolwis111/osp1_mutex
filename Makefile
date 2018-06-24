#!/usr/bin/make
.SUFFIXES:
.PHONY: all run pack clean

TAR = tcd
PCK = lab-5.zip
SRC = $(wildcard *.c)
OBJ = $(SRC:%.c=%.o)
CFLAGS = -std=gnu11 -c -g -Os -Wall -Werror
LFLAGS = -pthread

%.o: %.c %.h
	$(CC) $(CFLAGS) $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@

tcd: $(OBJ)
	$(CC) $(LFLAGS) $^ -o $@

all: $(TAR)

run: all
	./$(TAR)

pack: clean
	zip $(PCK) *.c *.h Makefile -x "./.*"

clean:
	$(RM) $(RMFILES) $(OBJ) $(TAR)
