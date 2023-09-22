CC=gcc
CFLAGS=-D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600 -I/usr/include/json-c -fsanitize=address
LIBS=-lncursesw -lpthread -lasan -ljson-c

efti: main.c gears.c
	$(CC) -o efti main.c gears.c efti_srv.c libncread/ncread.c libncread/vector.c logger/logger.c $(CFLAGS) $(LIBS) -g
