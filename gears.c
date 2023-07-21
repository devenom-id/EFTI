#include <curses.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <ncurses.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "gears.h"
#include "libncread/ncread.h"

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
	struct Nopt* nopt = (struct Nopt*)data;
	char *str = malloc(nopt->str_size); str[nopt->str_size-1]=0;
	switch (mode) {
		case 0:
			if (size < top) { top = size; }
			int p=0;
			for (int i=start; i<top; i++) {mvwaddstr(win, p, 0, ls[i]);p++;}
			wrefresh(win);
			return;
		case 1:
			memset(str, ' ', nopt->str_size-1);strncpy(str, ls[ptrs[1]], strlen(ls[ptrs[1]]));
			mvwaddstr(win, ptrs[0], 0, str);

			if (nopt->underline) wattron(win, A_UNDERLINE); else wattron(win, COLOR_PAIR(5));
			memset(str, ' ', nopt->str_size-1);strncpy(str, ls[ptrs[1]-1], strlen(ls[ptrs[1]-1]));
			mvwaddstr(win, ptrs[0]-1, 0, str);
			if (nopt->underline) wattroff(win, A_UNDERLINE); else wattroff(win, COLOR_PAIR(5));
			wrefresh(win);
			return;
		case 2:
			memset(str, ' ', nopt->str_size-1);strncpy(str, ls[ptrs[1]], strlen(ls[ptrs[1]]));
			mvwaddstr(win, ptrs[0], 0, str);

			if (nopt->underline) wattron(win, A_UNDERLINE); else wattron(win, COLOR_PAIR(5));
			memset(str, ' ', nopt->str_size-1);strncpy(str, ls[ptrs[1]+1], strlen(ls[ptrs[1]+1]));
			mvwaddstr(win, ptrs[0]+1, 0, str);
			if (nopt->underline) wattroff(win, A_UNDERLINE); else wattroff(win, COLOR_PAIR(5));
			wrefresh(win);
			return;
		case 3:
			memset(str, ' ', nopt->str_size-1);strncpy(str, ls[ptrs[1]], strlen(ls[ptrs[1]]));
			if (nopt->underline) wattron(win, A_UNDERLINE); else wattron(win, COLOR_PAIR(5));
			mvwaddstr(win, ptrs[0], 0, str);
			if (nopt->underline) wattroff(win, A_UNDERLINE); else wattroff(win, COLOR_PAIR(5));
			return;
	}
}

void reverse(char* str) {
	int size = strlen(str);
	for (int i=0; i<(size/2); i++) {
		char temp = str[i];
		str[i] = str[size-i-1];
		str[size-i-1] = temp;
	}
}

wchar_t *geticon(char* file) {
	struct stat st;
	stat(file, &st);
	if (S_ISDIR(st.st_mode)) return L"";
	char *s=NULL; int size = 0;
	for (int i=strlen(file)-1; i>=0; i--) {
		s = realloc(s, size+1);
		s[size] = file[i];
		if (file[i] == '.') {size++;break;}
		size++;
	}
	s=realloc(s,size+1); s[size]=0;
	reverse(s);
	wchar_t *ico;
	if (!strcmp(s, ".c")) ico=L" ";
	else if (!strcmp(s, ".py")) ico=L"󰌠 ";
	else if (!strcmp(s, ".sh") || !strcmp(s, ".bash")) ico=L" ";
	else if (!strcmp(s, ".java")) ico=L" ";
	else if (!strcmp(s, ".js")) ico=L" ";
	else if (!strcmp(s, ".cpp")) ico=L" ";
	else if (!strcmp(s, ".vim")) ico=L" ";
	else if (!strcmp(s, ".rb")) ico=L" ";
	else ico=L"󰦨 ";
	free(s);
	return ico;
}

void display_files(WINDOW *win, char**ls, int size, int start, int top, int *ptrs, void* data, int mode) {
	char str[50]; str[49]=0;
	char *pwd = ((struct Fopt*)data)->pwd;
	char *path = NULL;
	int len, nsize;
	struct stat st;
	switch (mode) {
		case 0:
			if (size < top) top = size; 
			int p=0;
			for (int i=start; i<top; i++) {
				len = strlen(ls[i]);
				char *path = malloc(len+strlen(pwd)+1);strcpy(path,pwd);strcat(path,ls[i]);
				nsize = (len<49-2) ?  len: 49-2;
				stat(path, &st);
				mvwaddwstr(win, p, 0, geticon(path));
				mvwaddnstr(win, p, 2, ls[i], nsize);
				if (S_ISDIR(st.st_mode)) mvwaddch(win, p, nsize+2, '/');
				wrefresh(win); //remove
				p++;
				free(path);
			}
			wrefresh(win);
			return;
		case 1:
			len = strlen(ls[ptrs[1]]);
			path = malloc(len+strlen(pwd)+1);strcpy(path,pwd);strcat(path,ls[ptrs[1]]);
			nsize = (len<49-2) ?  len: 49-2;
			memset(str, ' ', 49);strncpy(str, ls[ptrs[1]], nsize);
			mvwaddwstr(win, ptrs[0], 0, geticon(path));
			mvwaddnstr(win, ptrs[0], 2, str, nsize);
			stat(path, &st); if (S_ISDIR(st.st_mode)) mvwaddch(win, ptrs[0], nsize+2, '/');

			wattron(win, A_UNDERLINE);
			len = strlen(ls[ptrs[1]-1]);
			path = realloc(path, len+strlen(pwd)+1);strcpy(path,pwd);strcat(path,ls[ptrs[1]-1]);
			nsize = (len<49-2) ?  len: 49-2;
			memset(str, ' ', 49);strncpy(str, ls[ptrs[1]-1], nsize);
			mvwaddwstr(win, ptrs[0]-1, 0, geticon(path));
			mvwaddnstr(win, ptrs[0]-1, 2, str, nsize);
			stat(path, &st); if (S_ISDIR(st.st_mode)) mvwaddch(win, ptrs[0]-1, nsize+2, '/');
			wattroff(win, A_UNDERLINE);
			wrefresh(win);
			return;
		case 2:
			len = strlen(ls[ptrs[1]]);
			path = malloc(len+strlen(pwd)+1);strcpy(path,pwd);strcat(path,ls[ptrs[1]]);
			nsize = (len<49-2) ?  len: 49-2;
			memset(str, ' ', 49);strncpy(str, ls[ptrs[1]], nsize);
			mvwaddwstr(win, ptrs[0], 0, geticon(path));
			mvwaddnstr(win, ptrs[0], 2, str, nsize);
			stat(path, &st); if (S_ISDIR(st.st_mode)) mvwaddch(win, ptrs[0], nsize+2, '/');

			wattron(win, A_UNDERLINE);
			len = strlen(ls[ptrs[1]+1]);
			path = realloc(path, len+strlen(pwd)+1);strcpy(path,pwd);strcat(path,ls[ptrs[1]+1]);
			nsize = (len<49-2) ?  len: 49-2;
			memset(str, ' ', 49);strncpy(str, ls[ptrs[1]+1], nsize);
			mvwaddwstr(win, ptrs[0]+1, 0, geticon(path));
			mvwaddnstr(win, ptrs[0]+1, 2, str, nsize);
			stat(path, &st); if (S_ISDIR(st.st_mode)) mvwaddch(win, ptrs[0]+1, nsize+2, '/');
			wattroff(win, A_UNDERLINE);
			wrefresh(win);
			return;
		case 3:
			if (!size) return;
			len = strlen(ls[ptrs[1]]);
			path = malloc(len+strlen(pwd)+1);strcpy(path,pwd);strcat(path,ls[ptrs[1]]);
			nsize = (len<49-2) ?  len: 49-2;
			memset(str, ' ', 49-2);strncpy(str, ls[ptrs[1]], nsize+2);
			wattron(win, A_UNDERLINE);
			mvwaddwstr(win, ptrs[0], 0, geticon(path));
			mvwaddnstr(win, ptrs[0], 2, str, nsize);
			stat(path, &st); if (S_ISDIR(st.st_mode)) mvwaddch(win, ptrs[0], nsize+2, '/');
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
			if (sp == size-1 || !size) continue;
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
			if(!size)continue;
			data->ptrs[0] = p; data->ptrs[1] = sp;
			int res = cb.func[sp](data, cb.args[sp]);
			return res;
		} else {
			int index = search_binding(ch, bind);
			if (index != -1) {
				char* param = size ? ls[sp] : NULL;
				return bind.func[index](data, param);
			}
		}
	}
}

int handleFile(struct Data *data, void* f) {
	char* name = (char*)f;
	char* pwd = ((struct Fopt*)data->data)->pwd;
	char path[strlen(pwd)+strlen(name)+1];strcpy(path,pwd);strcat(path,name);
	struct stat st;
	stat(path, &st);
	if (S_ISDIR(st.st_mode)) dir_cd(pwd, name);
	else {
		endwin();
		int pid = fork();
		if (!pid) {
			char *cpy = malloc(strlen(pwd)+strlen(name)+1);
			strcpy(cpy, pwd);
			strcat(cpy, name);
			cpy[strlen(pwd)+strlen(name)] = 0;
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
	if (!file) return 1;
	char *pwd = ((struct Fopt*)data->data)->pwd;
	char*path = malloc(strlen(pwd)+strlen(file)+1);
	strcpy(path,pwd);strcat(path,file);
	struct stat st;
	stat(path, &st);
	if (!S_ISDIR(st.st_mode) && st.st_mode & S_IXUSR) {
		endwin();
		int PID = fork();
		if (!PID) {
			execl(path, path, NULL);
			exit(1);
		}
		wait(NULL);
		refresh();
	}
	return 1;
}

int isImg(char* file) {
	if (!file) return 1;
	char *s=NULL; int size = 0;
	for (int i=strlen(file)-1; i>=0; i--) {
		s = realloc(s, size+1);
		s[size] = file[i];
		if (file[i] == '.') {size++;break;}
		size++;
	}
	s=realloc(s,size+1);s[size]=0;
	reverse(s);
	if (!strcmp(s, ".png") || !strcmp(s, ".jpg")) return 1;
	return 0;
}

int view(struct Data *data, char *file) {
	char *pwd=((struct Fopt*)data->data)->pwd;
	if (!file) return 1;
	struct stat st;
	stat(file, &st);
	if (S_ISDIR(st.st_mode) || !isImg(file)) return 1;
	int PID = fork();
	if (!PID) {
		char*param = malloc(strlen(pwd)+strlen(file)+1);
		strcpy(param,pwd);strcat(param,file);
		close(0);close(1);close(2);
		execlp("feh", "feh", param, NULL);
		exit(1);
	}
	return 1;
}

int updir(struct Data *data, char* file) {dir_up(((struct Fopt*)data->data)->pwd);return 1;}

int hideDot(struct Data *data, char* file) {
	struct Fopt* fdata = (struct Fopt*)data->data;
	fdata->dotfiles = fdata->dotfiles ? 0: 1;
	return 1;
}

int menu_close(struct Data *data, void* args) {return 0;}

int fileRename(struct Data *data, char* file) {
	if (!file) return 1;
	char *pwd = ((struct Fopt*)data->data)->pwd;
	WINDOW* stdscr = data->wins[0];
	int y,x; getmaxyx(stdscr, y, x);
	WINDOW* win = newwin(3, 30, y/2-5, x/2-15);
	wbkgd(win, COLOR_PAIR(1));
	keypad(win, 1);
	wrefresh(win);
	getmaxyx(win, y, x);
	mvwaddstr(win, 0, x/2-3, "Rename");
	mvwaddstr(win, 1, 1, "Name:");
	wrefresh(win);
	char *buff;
	ampsread(win, &buff, 1, 7, 20, 20, 0);
	delwin(win); touchwin(data->wins[3]); wrefresh(data->wins[3]);
	if (buff==NULL) return 1;
	char *A = malloc(strlen(pwd)+strlen(file)+1);
	char *B = malloc(strlen(pwd)+strlen(buff)+1);
	strcpy(A, pwd); strcat(A, file);
	strcpy(B, pwd); strcat(B, buff);
	rename(A, B);
	return 1;
}

int fselect(struct Data *data, char* file) {
	if (!file) return 1;
	struct Fopt *fdata = (struct Fopt*)data->data;
	char *pwd = fdata->pwd;
	if (fdata->tmp_path==NULL) {
		char *path = malloc(strlen(pwd)+strlen(file)+1);
		strcpy(path, pwd); strcat(path, file);
		fdata->tmp_path = path;
		mvwaddwstr(data->wins[1],0, 2, L"");
	} else {
		fdata->tmp_path=NULL;
		mvwaddstr(data->wins[1],0, 2, " ");
	}
	wrefresh(data->wins[1]);
	return 1;
}

int fmove(struct Data *data, char* file) {
	struct Fopt *fdata = (struct Fopt*)data->data;
	if (fdata->tmp_path==NULL) return 1;
	char* pwd = fdata->pwd;
	char* spath = fdata->tmp_path;

	WINDOW* stdscr = data->wins[0];
	int y,x; getmaxyx(stdscr, y, x);
	WINDOW* win = newwin(4, 40, y/2-2, x/2-15);
	wbkgd(win, COLOR_PAIR(1));
	keypad(win, 1);
	wrefresh(win);
	getmaxyx(win, y, x);
	mvwaddstr(win, 1, 1, "Press 'y' to MOVE the following file");
	mvwaddstr(win, 2, 1, spath);
	wrefresh(win);
	int ch = wgetch(win);
	if (ch=='y') {
		char *s=NULL; int size = 0;
		for (int i=strlen(spath)-1; i>=0; i--) {
			s = realloc(s, size+1);
			if (spath[i] == '/') {break;}
			s[size] = spath[i];
			size++;
		}
		s=realloc(s,size+1);s[size]=0;
		reverse(s);
		char* path = malloc(strlen(pwd)+strlen(s)+1);
		strcpy(path, pwd);strcat(path, s);
		rename(spath, path);
		fdata->tmp_path=NULL;
		mvwaddstr(data->wins[1],0, 2, " ");
		wrefresh(data->wins[1]);
	}
	delwin(win); touchwin(data->wins[3]); wrefresh(data->wins[3]);
	return 1;
}

int fcopy(struct Data *data, char* file) {
	struct Fopt*fdata = (struct Fopt*)data->data;
	char *pwd = fdata->pwd;
	char *spath = fdata->tmp_path;
	char *s=NULL; int size = 0;
	for (int i=strlen(spath)-1; i>=0; i--) {
		s = realloc(s, size+1);
		if (spath[i] == '/') {break;}
		s[size] = spath[i];
		size++;
	}
	s=realloc(s,size+1);s[size]=0;
	reverse(s);
	char *path = malloc(strlen(pwd)+strlen(s)+1);
	strcpy(path,pwd);strcat(pwd,s);
	/*FIXME CONTINUE HERE FIXME*/
	return 1;
}

int fdelete(struct Data *data, char* file) {
	if (!file) return 1;

	WINDOW* stdscr = data->wins[0];
	int y,x; getmaxyx(stdscr, y, x);
	WINDOW* win = newwin(4, 40, y/2-2, x/2-15);
	wbkgd(win, COLOR_PAIR(1));
	keypad(win, 1);
	wrefresh(win);
	getmaxyx(win, y, x);
	mvwaddstr(win, 1, 1, "Press 'y' to DELETE the following file");
	mvwaddstr(win, 2, x/2-strlen(file)/2, file);
	wrefresh(win);
	int ch = wgetch(win);
	if (ch == 'y') {
		char *pwd = ((struct Fopt*)data->data)->pwd;
		char *path = malloc(strlen(pwd)+strlen(file)+1);
		strcpy(path, pwd);strcat(path, file);
		remove(path);
	}
	delwin(win); touchwin(data->wins[3]); wrefresh(data->wins[3]);
	return 1;
}
/*
󰙯  
*/
