CC=gcc
CFLAGS=-D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600 -fsanitize=address
LIBS=-lncursesw -lpthread -lasan

efti: main.c gears.c
	$(CC) -o efti main.c gears.c efti_srv.c libncread/ncread.c libncread/vector.c $(CFLAGS) $(LIBS) -g
