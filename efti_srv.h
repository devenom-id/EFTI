#ifndef EFTI_SRV_H
#define EFTI_SRV_H
#include "gears.h"
struct Srvdata {
	int size;
	char *content;
};
enum FileAttr {FA_DIR=1, FA_EXEC=2};
enum ServerOrder {OP_PING=1, OP_DOWNLOAD, OP_UPLOAD, OP_LIST_FILES, OP_GET_HOME,
	OP_MOVE, OP_COPY, OP_DELETE, OP_NEW_FILE, OP_NEW_DIR, OP_DISCONNECT};
typedef int (*bindFunc)(struct TabList*, struct Data*, char *);
int client_connect(struct TabList *tl, struct Data *data, char* file);
void server_create(struct TabList* tl);
void server_kill();
int client_disconnect(struct TabList* tl, struct Data* data, char* f);
void server_main(struct TabList* tl, int* fd);
int get_err_code(int fd);
struct Srvdata get_answ(int fd);
struct Srvdata get_fdata(int fd);
void* server_handle(void* conn);
#endif
