# Makefile theft isn't something I care about.

LIBS_CORE = -L/usr/local/lib
LIBS = -L/usr/local/lib -L. -Wl,-rpath,. -lezjack -ljack
CFLAGS_CORE = -g -I/usr/local/include
CFLAGS = -g -I/usr/local/include -I.

all: libezjack.so playez

clean:
	rm -f libezjack.so playez

libezjack.so: ezjack.c ezjack.h
	$(CC) -fPIC -shared -o libezjack.so ezjack.c $(CFLAGS_CORE) $(LIBS_CORE)

playez: libezjack.so playez.c ezjack.h
	$(CC) -o playez playez.c $(CFLAGS) $(LIBS)

