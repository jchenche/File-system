CC=gcc
CFLAGS=-c -g -std=c11 -pedantic-errors -Wall -Werror

all: kapish

kapish: kapish.o File.o
	$(CC) kapish.o File.o -o kapish

kapish.o: kapish.c ../io/File.h
	$(CC) $(CFLAGS) kapish.c

File.o: ../io/File.c ../io/File.h
	$(CC) $(CFLAGS) ../io/File.c

clean:
	rm *.o kapish
