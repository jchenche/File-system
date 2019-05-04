CC=gcc
CFLAGS=-c -g -std=c11 -pedantic-errors -Wall -Werror

all: kapish

kapish: kapish.o File.o diskIO.o
	$(CC) kapish.o File.o diskIO.o -o kapish

kapish.o: kapish.c ../io/File.h ../disk/diskIO.h
	$(CC) $(CFLAGS) kapish.c

File.o: ../io/File.c ../io/File.h ../disk/diskIO.h
	$(CC) $(CFLAGS) ../io/File.c

diskIO.o: ../disk/diskIO.c ../disk/diskIO.h
	$(CC) $(CFLAGS) ../disk/diskIO.c

.PHONY: clean

clean:
	rm *.o kapish
