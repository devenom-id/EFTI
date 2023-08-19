#include <curses.h>
#include <ncurses.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include "gears.h"
#include "efti_srv.h"
#include "libncread/vector.h"
#include "libncread/ncread.h"

void dialog(WINDOW* stdscr, WINDOW *wr, const char* s) {
	int y,x; getmaxyx(stdscr,y,x);
	int size=strlen(s)+2;
	if (size >= x-25) size=x-25;
	WINDOW *win = newwin(4, size, y/2-2, x/2-size/2);
	wbkgd(win, COLOR_PAIR(1));
	mvwaddstr(win,1,1,s);
	wattron(win, COLOR_PAIR(5));
	mvwaddstr(win,2,x/2-2,"[OK]");
	wattroff(win, COLOR_PAIR(5));
	for (;;) {
		int ch=wgetch(win);
		if (ch == 27 || ch == 10) break;
	}
	delwin(win); touchwin(wr); wrefresh(wr);
}

int get_addr(char **s, char *a) {
	int p=0; int px=0;
	for (int i=0; i<strlen(a); i++) {
		if (!((a[i] >= '0' && a[i] <= '9') || a[i] == ':' || a[i] == '.')) {return 0;}
		if (px==2) {return 0;}
		if (a[i] == ':') {px++;p=0;continue;}
		if ((!px && p==15) || (px && p==5)) {return 0;}
		s[px][p] = a[i];
		p++;
	}
	return 1;
}

char *itodg(int dig) {
	char *darr = calloc(5, 1);
	strcpy(darr, "0000");
	int i=3;
	while (dig) {
		darr[i]=(dig%10)+48;
		dig /= 10;
		i--;
	}
	return darr;
}

int enumdig(int n) {
	int d = 0;
	while (n) {n/=10;d++;}
	return d;
}

void create_dir_if_not_exist(const char* path) {
	struct vector str = string_split((char*)path, '/');
	if (!strlen(str.str[0])) {vector_popat(&str, 0);}
	struct string P; string_init(&P);
	if (path[0] == '/') {string_addch(&P, '/');}
	for (int i=0; i<str.size; i++) {
		string_add(&P, str.str[i]);
		string_addch(&P, '/');
		if (open(P.str, O_RDONLY) == -1) {
			mkdir(P.str, 0777);
		}
	}
}

void server_create() { /*Create server's process*/
	unlink("log");
	create_dir_if_not_exist("/tmp/efti");
	if (open("/tmp/efti/pid", O_RDONLY) != -1) { /*if server exists, don't create another one*/
		return;
	}
	pid_t pid = fork();
	if (!pid) {
		server_main();
		exit(0);
	}
	FILE *F = fopen("/tmp/efti/pid", "w");
	fprintf(F, "%d\n", pid);
	fclose(F);
	return;
}

void server_main() { /*server: listen for connections*/
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(4545);
	addr.sin_addr.s_addr = INADDR_ANY;

	socklen_t addsize = sizeof(addr);
	int serv = socket(AF_INET, SOCK_STREAM, 0);
	int opt = 1;
	setsockopt(serv, SOL_SOCKET, SO_REUSEADDR, &opt, (socklen_t)sizeof(opt));
	(void)bind(serv, (struct sockaddr*)&addr, addsize); /*check output of this.*/
	listen(serv, 1);
	for (;;) {
		int fd = accept(serv, (struct sockaddr*)&addr, &addsize);
		/*check if new connection is a valid client*/
		struct pollfd pfd[1]; pfd[0].events=POLLIN; pfd[0].fd=fd;
		if (!poll(pfd, 1, 2000)) {close(fd);continue;}
		char buff[5] = {0};
		read(fd, buff, 4);
		if (strcmp(buff, "3471")) {close(fd);continue;}
		pthread_t t1;
		pthread_create(&t1, NULL, server_handle, &fd);
	}
}

struct Srvdata get_data(int fd) {
	struct Srvdata sd;
	char *buff = calloc(6,1);
	read(fd, buff, 5);
	sd.order = buff[0]-48;
	char *pbuff = buff; pbuff++;
	int size = atoi(pbuff); /*digits*/
	pbuff = calloc(size+1, 1);
	read(fd, pbuff, size);
	size = atoi(pbuff); /*size*/
	sd.size = size;
	free(pbuff); pbuff = calloc(size+1, 1);
	read(fd, pbuff, size); /*content*/
	sd.content=pbuff;
	return sd;
}

struct Srvdata get_answ(int fd) {
	struct Srvdata sd;
	char *buff = calloc(5,1);
	read(fd, buff, 4);
	sd.order = 0;
	char *pbuff = buff;
	int size = atoi(pbuff); /*digits*/
	pbuff = calloc(size+1, 1);
	read(fd, pbuff, size);
	size = atoi(pbuff); /*size*/
	sd.size = size;
	free(pbuff); pbuff = calloc(size+1, 1);
	read(fd, pbuff, size); /*content*/
	sd.content=pbuff;
	return sd;
}

void *server_handle(void* conn) { /*server's core*/
	int fd = *((int*)conn);
	for (;;) {
		struct pollfd rfd[1]; rfd[0].fd=fd;rfd[0].events=POLLIN;
		poll(rfd, 1, -1);
		struct Srvdata sd = get_data(fd);
		switch (sd.order) {
			case 1: { /*ping (Can be used to disconnect for inactivity)*/
				char tmpbf[1] = {1};
				write(fd, tmpbf, 1);
				break;
			}
			case 2: /*send (I'll send you, you receive)*/
				break;
			case 3: /*download (I'll download this file)*/
				break;
			case 4: { /*list files*/
				DIR *dir = opendir(sd.content);
				struct dirent *dnt;
				struct string files, hstr;
				string_init(&files); string_init(&hstr);
				while ((dnt=readdir(dir))!=NULL) {
					string_add(&files, dnt->d_name);
					string_addch(&files, '/');
				}
				char files_size[11] = {0}; snprintf(files_size, 10, "%d", files.size);
				char *files_size_digit = itodg(enumdig(files.size));
				string_add(&hstr, files_size_digit);
				string_add(&hstr, files_size);
				string_add(&hstr, files.str);
				int wres = write(fd, hstr.str, hstr.size);
				string_free(&hstr); string_free(&files);
				closedir(dir);
				break;
			}
			case 5: { /*get home*/
				char *home = getenv("HOME");
				char *size = calloc(strlen(home)+1,1);
				sprintf(size, "%lu", strlen(home));
				struct string hstr;
				string_init(&hstr);
				string_add(&hstr, itodg(enumdig(strlen(home))));
				string_add(&hstr, size);
				string_add(&hstr, home);
				write(fd, hstr.str, hstr.size);
				break;
			}
			case 0: /*disconnected*/
			case 6: /*disconnect*/
				close(fd);
				return 0;
		}
	}
	return 0;
}

void server_kill() { /*kill server if exists*/
	int fd = open("/tmp/efti/pid", O_RDONLY);
	if (fd == -1) return;
	int pid=0;
	for (;;) {
		char buff[1];
		read(fd, buff, 1);
		if (buff[0] == 10) break;
		pid*=10;pid+=buff[0]-48;
	}
	kill(pid, SIGKILL);
	unlink("/tmp/efti/pid");
}

char* gethome(int fd) {
	write(fd, "50000", 5);
	struct pollfd pfd[1];
	pfd[0].events=POLLIN; pfd[0].fd=fd;
	poll(pfd, 1, -1);
	struct Srvdata sd = get_answ(fd);
	return sd.content;
}

int client_connect(struct TabList *tl, struct Data *data, char* file) {
	// create a window asking for the address
	WINDOW* wr = tl->wobj[0].data->wins[4];
	WINDOW* wfiles = tl->wobj[0].data->wins[5];
	WINDOW* tabwin = tl->wobj[0].data->wins[2];
	WINDOW* stdscr = tl->wobj[0].data->wins[0];
	int y, x; getmaxyx(stdscr, y, x);
	int m = y-2;
	WINDOW* win = newwin(4, 37, y/2-2, x/2-18);
	getmaxyx(win, y, x);
	wbkgd(win, COLOR_PAIR(1));
	keypad(win, 1);
	mvwaddstr(win, 0, x/2-8, "Connect to remote");
	mvwaddstr(win, 2, 1, "Address:port:");
	wrefresh(win);
	char *buff;
	ampsread(win, &buff, 2, 15, 21, 21, 0);
	delwin(win); touchwin(wfiles); wrefresh(wfiles);
	if (!buff) {
		dialog(stdscr, wr, "You have to write the port and the address");
		return 1;
	}
	char a[16]={0}; char b[6]={0}; char *arr[2]={a,b};
	if (!get_addr(arr,buff)) {
		dialog(stdscr, wr, "There was an error on the format of the address");
		return 1;
	}
	// connect to address
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	inet_aton(arr[0], &addr.sin_addr);
	addr.sin_port = htons(atoi(arr[1]));
	socklen_t size = sizeof(addr);
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (connect(fd, (struct sockaddr*)&addr, size) == -1) {
		dialog(stdscr, wr, "An error ocurred on connect(2)");
		return 1;
	}
	write(fd, "3471", 4);
	// create the Wobj and add it to TabList
	add_tab(tabwin, tl);
	tl->wobj[tl->size-1].data = tl->wobj[0].data;
	struct Fopt *fopt = tl->wobj[tl->size-1].data->data;
	fopt->tmp_path = NULL; fopt->pwd = NULL;

	tl->wobj[tl->size-1].local = 0;
	tl->wobj[tl->size-1].pwd = NULL;
	
	WINDOW* wrf = newwin(m, 50, 3, 4);
	keypad(wrf, 1); wbkgd(wrf, COLOR_PAIR(3));
	tl->wobj[tl->size-1].win = wrf;

	tl->wobj[tl->size-1].fd = fd;
	tl->wobj[tl->size-1].ls = NULL;
	tl->wobj[tl->size-1].bind = tl->wobj[0].bind;
	tl->wobj[tl->size-1].pwd = gethome(fd);
	return 1;
}
int client_disconnect();
int server_send(struct TabList *tl, struct Data* data, void* n) {return 1;}
int server_retrieve(struct TabList *tl, struct Data* data, void* n) {return 1;}
