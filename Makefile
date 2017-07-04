CC=clang
CFLAGS=-Wall -g

SOURCEDIR=src
BUILDIR=bin
TESTDIR=test

all: blkalloc.o memtest.o
	${CC} ${BUILDIR}/blkalloc.o ${BUILDIR}/memtest.o -o ${BUILDIR}/memtest

blkalloc.o:
	${CC} ${CFLAGS} -c ${SOURCEDIR}/blkalloc.c -o ${BUILDIR}/blkalloc.o

memtest.o:
	${CC} ${CFLAGS} -c ${TESTDIR}/memtest.c -o ${BUILDIR}/memtest.o

clean:
	rm -rf *~
	rm -rf *.o
