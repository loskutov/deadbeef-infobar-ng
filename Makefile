INCLUDE="-I/usr/include"

GTK=`pkg-config --cflags --libs gtk+-2.0`
LIBXML2 = `pkg-config --cflags --libs libxml-2.0`

CC=gcc
CFLAGS=-Wall -std=c99 -D_GNU_SOURCE

all:
	${CC} ${CFLAGS} ${GTK} ${LIBXML2} ${INCLUDE} -o infobar.so -shared infobar.c support.c
clean:
	rm infobar.so
