CC=gcc
CFLAGS=
LIBS=-lncurses


efti: main.c gears.c
	$(CC) -o efti main.c gears.c $(CFLAGS) $(LIBS)
