BINDIR =	/usr/local/sbin
MANDIR =	/usr/local/man/man1
CC =		gcc
CFLAGS =	-O
#CFLAGS =	-g
#SYSVLIBS =	-lnsl -lsocket
LDFLAGS =	-s ${SYSVLIBS}
#LDFLAGS =	-g ${SYSVLIBS}

all:		tmhttpd

tmhttpd:	tmhttpd.o
	${CC} ${CFLAGS} tmhttpd.o ${LDFLAGS} -o tmhttpd

tmhttpd.o:	tmhttpd.c
	${CC} ${CFLAGS} -c tmhttpd.c

install:	all
	rm -f ${BINDIR}/tmhttpd
	cp tmhttpd ${BINDIR}/tmhttpd
	rm -f ${MANDIR}/tmhttpd.1
	cp tmhttpd.1 ${MANDIR}/tmhttpd.1

clean:
	rm -f tmhttpd *.o core core.* *.core
