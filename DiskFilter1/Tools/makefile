CC = gcc
CFLAGS = -Wall

all: VerifyCache FileVerify

VerifyCache.o: VerifyCache.c
md5.o: md5.c
FileVerify.o: FileVerify.c

VerifyCache: VerifyCache.o ../Cache.c
	$(CC) $+ -o $@

FileVerify: FileVerify.o md5.o
	$(CC) $+ -o $@

clean:
	rm -rf VerifyCache.o Cache.o FileVerify.o md5.o
