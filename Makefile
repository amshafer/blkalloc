CC=clang
CFLAGS=-Wall -g

SOURCEDIR=./src
BUILDIR=./bin

all: blkalloc

blkalloc:
	${CC} ${CFLAGS} ${SOURCEDIR}/blkalloc.c -o ${BUILDIR}/blkalloc

clean:
	rm -rf *~
	rm -rf *.o
