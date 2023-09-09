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
/* connect debe crear una conexión a un servidor remoto.
 * Debe crear un objeto wobj con todos los métodos para poder interactuar
 * con el server. En otras palabras, crear una nueva tab en la tablist.
 * Recordatorio: Cada tab en la tablist es una wobj.*/
void create_dir_if_not_exist(const char* path);
void server_create();
/* create debe crear un proceso nuevo usando fork().
 * Cuando el proceso padre muera, el server seguirá online.
 * create debe crear o sobreescribir un archivo con el PID del nuevo
 * proceso para poder detenerlo a voluntad.*/
void server_kill();
/* Lee el archivo que contiene el pid del server, envía SIGTERM al server
 * y elimina el archivo.*/
int client_disconnect();
/* Cierra la conexión al servidor remoto.
 * Destruye el wobj asociado a esa conexión. (del_tab)*/
void server_main();
struct Srvdata get_answ(int fd);
struct Srvdata get_fdata(int fd);
void* server_handle(void* conn);
int server_send(struct TabList *tl, struct Data* data, void* n);
int server_retrieve(struct TabList *tl, struct Data* data, void* n);
#endif
