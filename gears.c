#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <ncurses.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "gears.h"

void uptime(char* buff) {
	struct sysinfo si;
	sysinfo(&si);
	sprintf(buff, "%d days, %d hours", (int)si.uptime/86400, (int)si.uptime/3600);
}

void dir_up(char *pwd) {
	char *tmp = pwd+strlen(pwd)-2;
	while (*tmp != '/') tmp--;
	*(tmp+1) = 0;
	pwd = realloc(pwd, strlen(pwd)+1);
}

void dir_cd(char *pwd, char *dir) {
	int size = strlen(pwd)+strlen(dir)+2;
	pwd = realloc(pwd, size);
	strcat(pwd, dir);
	pwd[size-2] = '/';
	pwd[size-1] = 0;
}

int list(char *path, char*** ls, int dotfiles) {
	int size = 0;
	struct dirent *dt;
	DIR *dir = opendir(path);
	while ((dt=readdir(dir)) != NULL) {
		if (!strcmp(dt->d_name, ".") || !strcmp(dt->d_name, "..")) continue;
		if (dotfiles && dt->d_name[0] == '.') continue;
		*ls = realloc(*ls, sizeof(char*)*(size+1));
		(*ls)[size] = dt->d_name;
		size++;
	}
	return size;
}

void alph_sort(char** ls, int size) {
	for (int i=0; i<size-1; i++) {
		for (int e=0; e<size-1-i; e++) {
			if (strcmp(ls[e],ls[e+1]) > 0) {
				char* temp = ls[e];
				ls[e] = ls[e+1];
				ls[e+1] = temp;
			}
		}
	}
}

void pr_ls(char **ls, int size) {
	printf("{");
	for (int i=0; i<size-1; i++) {
		printf("%s, ", ls[i]);
	}
	printf("%s}\n", ls[size-1]);
}

void display_opts(WINDOW* win, char **ls, int size, int start, int top, int* ptrs, void* data, int mode) {
	char str[50]; str[49]=0;
	switch (mode) {
		case 0:
			if (size < top) { top = size; }
			int p=0;
			for (int i=start; i<top; i++) {mvwaddstr(win, p, 0, ls[i]);p++;}
			wrefresh(win);
			return;
		case 1:
			memset(str, ' ', 49);strncpy(str, ls[ptrs[1]], strlen(ls[ptrs[1]]));
			mvwaddstr(win, ptrs[0], 0, str);

			wattron(win, A_UNDERLINE);
			memset(str, ' ', 49);strncpy(str, ls[ptrs[1]-1], strlen(ls[ptrs[1]-1]));
			mvwaddstr(win, ptrs[0]-1, 0, str);
			wattroff(win, A_UNDERLINE);
			wrefresh(win);
			return;
		case 2:
			memset(str, ' ', 49);strncpy(str, ls[ptrs[1]], strlen(ls[ptrs[1]]));
			mvwaddstr(win, ptrs[0], 0, str);

			wattron(win, A_UNDERLINE);
			memset(str, ' ', 49);strncpy(str, ls[ptrs[1]+1], strlen(ls[ptrs[1]+1]));
			mvwaddstr(win, ptrs[0]+1, 0, str);
			wattroff(win, A_UNDERLINE);
			wrefresh(win);
			return;
		case 3:
			memset(str, ' ', 49);strncpy(str, ls[ptrs[1]], strlen(ls[ptrs[1]]));
			wattron(win, A_UNDERLINE);
			mvwaddstr(win, ptrs[0], 0, str);
			wattroff(win, A_UNDERLINE);
			return;
	}
}

void display_files(WINDOW *win, char**ls, int size, int start, int top, int *ptrs, void* data, int mode) {
	char str[50]; str[49]=0;
	char *pwd = (char*)data;
	char *path = NULL;
	int len, nsize;
	struct stat st;
	switch (mode) {
		case 0:
			if (size < top) top = size; 
			int p=0;
			for (int i=start; i<top; i++) {
				len = strlen(ls[i]);
				char path[len+strlen(data)+1];strcpy(path,pwd);strcat(path,ls[i]);
				nsize = (len<49) ?  len: 49;
				stat(path, &st);
				mvwaddnstr(win, p, 0, ls[i], nsize);
				if (S_ISDIR(st.st_mode)) mvwaddch(win, p, nsize, '/');
				p++;
			}
			wrefresh(win);
			return;
		case 1:
			len = strlen(ls[ptrs[1]]);
			path = malloc(len+strlen(data)+1);strcpy(path,pwd);strcat(path,ls[ptrs[1]]);
			nsize = (len<49) ?  len: 49;
			memset(str, ' ', 49);strncpy(str, ls[ptrs[1]], nsize);
			mvwaddstr(win, ptrs[0], 0, str);
			stat(path, &st); if (S_ISDIR(st.st_mode)) mvwaddch(win, ptrs[0], nsize, '/');

			wattron(win, A_UNDERLINE);
			len = strlen(ls[ptrs[1]-1]);
			path = realloc(path, len+strlen(data)+1);strcpy(path,pwd);strcat(path,ls[ptrs[1]-1]);
			nsize = (len<49) ?  len: 49;
			memset(str, ' ', 49);strncpy(str, ls[ptrs[1]-1], nsize);
			mvwaddstr(win, ptrs[0]-1, 0, str);
			stat(path, &st); if (S_ISDIR(st.st_mode)) mvwaddch(win, ptrs[0]-1, nsize, '/');
			wattroff(win, A_UNDERLINE);
			wrefresh(win);
			return;
		case 2:
			len = strlen(ls[ptrs[1]]);
			path = malloc(len+strlen(data)+1);strcpy(path,pwd);strcat(path,ls[ptrs[1]]);
			nsize = (len<49) ?  len: 49;
			memset(str, ' ', 49);strncpy(str, ls[ptrs[1]], nsize);
			mvwaddstr(win, ptrs[0], 0, str);
			stat(path, &st); if (S_ISDIR(st.st_mode)) mvwaddch(win, ptrs[0], nsize, '/');

			wattron(win, A_UNDERLINE);
			len = strlen(ls[ptrs[1]+1]);
			path = realloc(path, len+strlen(data)+1);strcpy(path,pwd);strcat(path,ls[ptrs[1]+1]);
			nsize = (len<49) ?  len: 49;
			memset(str, ' ', 49);strncpy(str, ls[ptrs[1]+1], nsize);
			mvwaddstr(win, ptrs[0]+1, 0, str);
			stat(path, &st); if (S_ISDIR(st.st_mode)) mvwaddch(win, ptrs[0]+1, nsize, '/');
			wattroff(win, A_UNDERLINE);
			wrefresh(win);
			return;
		case 3:
			len = strlen(ls[ptrs[1]]);
			path = malloc(len+strlen(data)+1);strcpy(path,pwd);strcat(path,ls[ptrs[1]]);
			nsize = (len<49) ?  len: 49;
			memset(str, ' ', 49);strncpy(str, ls[ptrs[1]], nsize);
			wattron(win, A_UNDERLINE);
			mvwaddstr(win, ptrs[0], 0, str);
			stat(path, &st); if (S_ISDIR(st.st_mode)) mvwaddch(win, ptrs[0], nsize, '/');
			wattroff(win, A_UNDERLINE);
			return;
	}
}

int search_binding(int ch, struct Binding bind) {
	for (int i=0; i<bind.nmemb; i++) {
		if (ch == bind.keys[i]) return i;
	}
	return -1;
}

int menu(WINDOW* win, char** ls, void (*dcb)(WINDOW*,char**,int,int,int,int*,void*,int), struct Callback cb, struct Data *data, struct Binding bind) {
	int y; int x; getmaxyx(win, y, x);
	int p = 0;
	int sp = 0;
	int ptrs[2] = {0};
	int top = y;
	int size = cb.nmemb;
	dcb(win, ls, size, 0, y, ptrs, data->data, 0);
	dcb(win, ls, size, 0, top, ptrs, data->data, 3);
	for (;;) {
		int ch = wgetch(win);
		if (ch == KEY_UP) {
			if (!sp) continue;
			if (!p) {
				wscrl(win, -1);
				top--;sp--;
				wmove(win,0,0);wclrtobot(win);
				dcb(win, ls, size, top-y, top, ptrs, data->data, 0);

				ptrs[0]=p+1;ptrs[1]=sp+1;
				dcb(win,ls,size,top-y,top,ptrs,data->data,1);

			} else {
				ptrs[0]=p;ptrs[1]=sp;
				dcb(win,ls,size,top-y,top,ptrs,data->data,1);
				sp--;p--;
			}
		}
		else if (ch == KEY_DOWN) {
			if (sp == size-1) continue;
			if (sp == top-1) {
				wscrl(win, 1);
				top++;sp++;
				wmove(win,0,0);wclrtobot(win);
				dcb(win, ls, size, top-y, top, ptrs, data->data, 0);
				
				ptrs[0]=p-1;ptrs[1]=sp-1;
				dcb(win,ls,size,top-y,top,ptrs,data->data,2);
			} else {
				ptrs[0]=p;ptrs[1]=sp;
				dcb(win,ls,size,top-y,top,ptrs,data->data,2);
				sp++;p++;
			}
		}
		else if (ch == 27) {return 0;}
		else if (ch == 10) {
			data->ptrs[0] = p; data->ptrs[1] = sp;
			int res = cb.func[sp](data, cb.args[sp]);
			return res;
		} else {
			int index = search_binding(ch, bind);
			if (index != -1) {
				return bind.func[index](data, ls[sp]);
			}
		}
	}
}

int handleFile(struct Data *data, void* f) {
	char* name = (char*)f;
	char* pwd = (char*)data->data;
	char path[strlen(pwd)+strlen(name)+1];strcpy(path,pwd);strcat(path,name);
	struct stat st;
	stat(path, &st);
	if (S_ISDIR(st.st_mode)) dir_cd(pwd, name);
	else {
		endwin();
		int pid = fork();
		if (!pid) {
			char *cpy = malloc(strlen(pwd)+strlen(name)+2);
			strcpy(cpy, pwd);
			cpy[strlen(pwd)] = '/';
			strcat(cpy, name);
			cpy[strlen(pwd)+strlen(name)+1] = 0;
			char *argv[] = {"/usr/bin/nvim", cpy, NULL};
			execvp("nvim", argv);
			exit(1);
		}
		wait(NULL);
		refresh();
	}
	return 1;
}

int execute(struct Data *data, char* file) {
	struct stat st;
	stat(file, &st);
	if (!S_ISDIR(st.st_mode) && st.st_mode & S_IXUSR) {
		endwin();
		int PID = fork();
		if (!PID) {
			execl(file, file, NULL);
			exit(1);
		}
		wait(NULL);
		refresh();
	}
	return 1;
}

void reverse(char* str) {
	int size = strlen(str);
	for (int i=0; i<(size/2); i++) {
		char temp = str[i];
		str[i] = str[size-i-1];
		str[size-i-1] = temp;
	}
}

int isImg(char* file) {
	char *s=NULL; int size = 0;
	for (int i=strlen(file)-1; i>=0; i--) {
		s = realloc(s, size+1);
		s[size] = file[i];
		if (file[i] == '.') break;
		size++;
	}
	reverse(s);
	if (!strcmp(s, ".png") || !strcmp(s, ".jpg")) return 1;
	return 0;
}

int view(struct Data *data, char *file) {
	struct stat st;
	stat(file, &st);
	if (S_ISDIR(st.st_mode) || !isImg(file)) return 1;
	int PID = fork();
	if (!PID) {
		close(0);close(1);close(2);
		execlp("feh", file, NULL);
		exit(1);
	}
	return 1;
}

int updir(struct Data *data, char* file) {dir_up(data->data);return 1;}

int hideDot(struct Data *data, char* file) {
	data->dotfiles = data->dotfiles ? 0: 1; return 1;
}
