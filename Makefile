# Makefile theft isn't something I care about.

LIBS_CORE = -L/usr/local/lib
#LIBS = -L/usr/local/lib -L. -Wl,-rpath,. -lezjack -ljack
LIBS = -L/usr/local/lib -L. ezjack.o -ljack
CFLAGS_CORE = -g -I/usr/local/include
CFLAGS = -g -I/usr/local/include -I.

all: ezjack.o playez recez libdspez.so

clean:
	rm -f ezjack.o playez recez dspez

ezjack.o: ezjack.c ezjack.h
	$(CC) -fPIC -c -o ezjack.o ezjack.c $(CFLAGS_CORE) $(LIBS_CORE)

playez: ezjack.o playez.c ezjack.h
	$(CC) -o playez playez.c $(CFLAGS) $(LIBS)

recez: ezjack.o recez.c ezjack.h
	$(CC) -o recez recez.c $(CFLAGS) $(LIBS)

libdspez.so: ezjack.o dspez.c ezjack.h
	$(CC) -fPIC -shared -o libdspez.so dspez.c $(CFLAGS) $(LIBS)

