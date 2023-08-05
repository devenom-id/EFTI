CC=gcc
CFLAGS=-D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600
LIBS=-lncursesw -lpthread


efti: main.c gears.c
	$(CC) -o efti main.c gears.c libncread/ncread.c libncread/vector.c $(CFLAGS) $(LIBS) -g
