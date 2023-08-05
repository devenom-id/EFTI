#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "gears.h"
#include "efti_srv.h"
#include "libncread/vector.h"

void create_dir_if_not_exist(const char* path) {
	struct vector str = string_split((char*)path, '/');
	if (!strlen(str.str[0])) {vector_popat(&str, 0);}
	for (int i=0; i<str.size; i++) {
	}
}

void server_create() { /*Create server's process*/
	//check first if it exists.
	// -- 
	pid_t pid = fork();
	if (!pid) {
	}
	return;
}

void server_main() { /*server: listen for connections*/
	struct sockaddr_in addr = {AF_INET, 4545, INADDR_ANY};
	socklen_t addsize = sizeof(addr);
	int serv = socket(AF_INET, SOCK_STREAM, 0);
	int opt = 1;
	setsockopt(serv, SOL_SOCKET, SO_REUSEADDR, &opt, (socklen_t)sizeof(opt));
	(void)bind(serv, (struct sockaddr*)&addr, addsize); /*check output of this.*/
	listen(serv, 1);
	for (;;) {
		int fd = accept(serv, (struct sockaddr*)&addr, &addsize);
		pthread_t t1;
		pthread_create(&t1, NULL, server_handle, NULL);
	}
}

void server_kill() { /*kill server if exists*/
}

void server_connect();
void server_create();
void server_kill();
void server_disconnect();
int server_send(struct TabList *tl, struct Data* data, void* n) {return 1;}
int server_retrieve(struct TabList *tl, struct Data* data, void* n) {return 1;}
