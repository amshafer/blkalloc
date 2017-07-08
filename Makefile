CC=clang
CFLAGS=-Wall -g

SOURCEDIR=src
BUILDIR=bin
TESTDIR=test

TESTCMD=test/runtests
.PHONY:test

all: setup blkalloc.o memtest.o 
	${CC} ${BUILDIR}/blkalloc.o ${BUILDIR}/memtest.o -o ${BUILDIR}/memtest

setup:
	mkdir -p ${BUILDIR}

blkalloc.o:
	${CC} ${CFLAGS} -c ${SOURCEDIR}/blkalloc.c -o ${BUILDIR}/blkalloc.o

memtest.o:
	${CC} ${CFLAGS} -c ${TESTDIR}/memtest.c -o ${BUILDIR}/memtest.o

test:
	${TESTCMD}

clean:
	rm -rf *~
	rm -rf *.o
