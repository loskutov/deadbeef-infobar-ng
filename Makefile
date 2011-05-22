GTK?=`pkg-config --cflags --libs gtk+-2.0`
LIBXML2?=`pkg-config --cflags --libs libxml-2.0`

CC?=gcc
CFLAGS+=-Wall -fPIC -std=c99 -D_GNU_SOURCE
OUT?=ddb_infobar.so

all:
	${CC} ${CFLAGS} ${GTK} ${LIBXML2} ${INCLUDE} -o $(OUT) -shared infobar.c support.c
clean:
	rm $(OUT)
