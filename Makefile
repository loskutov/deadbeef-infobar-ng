OUT?=ddb_infobar.so

GTK_CFLAGS?=`pkg-config --cflags gtk+-2.0`
LIBXML2_CFLAGS?=`pkg-config --cflags libxml-2.0`

GTK_LIBS?=`pkg-config --libs gtk+-2.0`
LIBXML2_LIBS?=`pkg-config --libs libxml-2.0`

CC?=gcc
CFLAGS+=-Wall -fPIC -std=c99 -D_GNU_SOURCE $(GTK_CFLAGS) $(LIBXML2_CFLAGS)
LDFLAGS+=-shared $(GTK_LIBS) $(LIBXML2_LIBS)

SOURCES=infobar.c infobar_ui.c infobar_bio.c infobar_lyr.c utils.c

OBJECTS=$(SOURCES:.c=.o)

all: $(SOURCES) $(OUT)

$(OUT): $(OBJECTS)
	$(CC) $(OBJECTS)  $(LDFLAGS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -c -o $@

clean:
	rm $(OBJECTS) $(OUT)
