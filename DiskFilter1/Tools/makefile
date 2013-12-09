CC = gcc

all: VerifyCache

VerifyCache.o: VerifyCache.c

VerifyCache: VerifyCache.o ../Cache.c
	$(CC) $+ -o $@

clean:
	rm -rf VerifyCache.o Cache.o
