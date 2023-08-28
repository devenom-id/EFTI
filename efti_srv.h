#ifndef EFTI_SRV_H
#define EFTI_SRV_H
#include "gears.h"
struct Srvdata {
	int size;
	char *content;
};
enum FileAttr {FA_DIR=1, FA_EXEC=2};
int client_connect(struct TabList *tl, struct Data *data, char* file);
/* connect debe crear una conexión a un servidor remoto.
 * Debe crear un objeto wobj con todos los métodos para poder interactuar
 * con el server. En otras palabras, crear una nueva tab en la tablist.
 * Recordatorio: Cada tab en la tablist es una wobj.*/
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
void* server_handle(void* conn);
int server_send(struct TabList *tl, struct Data* data, void* n);
int server_retrieve(struct TabList *tl, struct Data* data, void* n);
struct Srvdata get_answ(int fd);
#endif
