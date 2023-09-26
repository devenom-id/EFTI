#include <curses.h>
#include <json-c/json_object.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <ncurses.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <pthread.h>
#include <signal.h>
#include <libgen.h>
#include <fcntl.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <json.h>
#include "efti_srv.h"
#include "gears.h"
#include "libncread/ncread.h"
#include "libncread/vector.h"
#include "logger/logger.h"

int _;
void handleError(int c, int n, const char* str) {
	if (c!=n)return;
	(void) endwin();
	(void) puts("\t\033[1;31mEFTI has encountered an error.\033[0m");
	(void) perror(str);
	exit(1);
}
void handleMemError(void* m, const char* str) {
	if (m!=NULL)return;
	(void) endwin();
	(void) puts("\t\033[1;31mEFTI has encountered an error.\033[0m");
	(void) perror(str);
	exit(1);
}
void handleDisconnection(struct TabList* tl) {
	WINDOW* stdscr = tl->wobj[0].data->wins[0];
	WINDOW* main = tl->wobj[0].data->wins[4];
	WINDOW* wfiles = tl->wobj[0].data->wins[5];
	WINDOW* wins[] = {stdscr, main, wfiles};
	dialog(wins, "El cliente ha perdido conexión con el servidor.");
	char option=0;
	client_disconnect(tl, NULL, &option);
}

void dialog(WINDOW** wins, const char* s) {
	int y,x; getmaxyx(wins[0],y,x);
	int size=strlen(s)+2;
	if (size >= x-25) size=x-25;
	WINDOW *win = newwin(4, size, y/2-2, x/2-size/2);
	box(win, ACS_VLINE, ACS_HLINE);
	getmaxyx(win,y,x);
	wbkgd(win, COLOR_PAIR(1));
	mvwaddstr(win,1,1,s);
	wattron(win, COLOR_PAIR(5));
	mvwaddstr(win,2,x/2-2,"[OK]");
	wattroff(win, COLOR_PAIR(5));
	wrefresh(win);
	for (;;) {
	int ch=wgetch(win);
	if (ch == 27 || ch == 10) break;
	}
	delwin(win);
	touchwin(wins[1]);
	touchwin(wins[2]);
	wrefresh(wins[1]);
	wrefresh(wins[2]);
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
	string_free(&P);
	vector_free(&str);
}

char *itodg(int dig) {
	char *darr = calloc(5, 1); handleMemError(darr, "calloc(2) on itodg");
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
	if (!n) return 1;
	int d = 0;
	while (n) {n/=10;d++;}
	return d;
}

void itoa(int n, int d, char*b) {
	for (int i=d-1; i>=0; i--) {
	b[i] = (n%10)+48;
	n /= 10;
	}
}

void tcbreak(struct termios *old, struct termios *tty) {
	tcgetattr(0, old);
	tty = old;
	tty->c_lflag &= ~(ECHO | ICANON);
	tty->c_cc[VTIME] = 0; tty->c_cc[VMIN] = 1;
	tcsetattr(0, TCSADRAIN, tty);
}

void tnocbreak(struct termios *old) {tcsetattr(0, TCSADRAIN, old);}

void uptime(char* buff) {
	struct sysinfo si;
	sysinfo(&si);
	sprintf(buff, "%d days, %d hours", (int)si.uptime/86400, (int)si.uptime/3600);
}

void dir_up(char **pwd) {
	if (!strcmp(*pwd, "/")) return;
	char *tmp = *pwd+strlen(*pwd)-2;
	while (*tmp != '/') tmp--;
	*(tmp+1) = 0;
	*pwd = realloc(*pwd, strlen(*pwd)+1);handleMemError(*pwd, "realloc(2) on dir_up");
}

void dir_cd(char **pwd, char *dir) {
	int size = strlen(*pwd)+strlen(dir)+2;
	*pwd = realloc(*pwd, size);handleMemError(*pwd, "realloc(2) on dir_cd");
	strcat(*pwd, dir);
	(*pwd)[size-2] = '/';
	(*pwd)[size-1] = 0;
}

int list(struct TabList *tl, int dotfiles) {
	struct Wobj *wobj = get_current_tab(tl);
	char ***ls = &(wobj->ls);
	int local = wobj->local;
	if (!local) {
		int fd = wobj->fd;
		char* pwd = wobj->pwd;
		int size = strlen(pwd);
		int digits = enumdig(size);
		char *strsz = calloc(digits+1,1); snprintf(strsz, digits+1, "%d", size);
		struct string str; string_init(&str);
		string_addch(&str, '4');
		string_add(&str, itodg(digits));
		string_add(&str, strsz);
		string_add(&str, pwd);
		string_add(&str, "00011");
		string_addch(&str, dotfiles+48);
		int r = write(fd, str.str, str.size);
		struct Srvdata answ = get_answ(tl, fd);
		if (answ.size==-1) {
			return -1;
		}
		struct vector vec = string_split(answ.content, '/');
		vector_pop(&vec);
		wobj->ls = vec.str;
		answ = get_answ(tl, fd);
		if (answ.size==-1) {
			return -1;
		}
		wobj->attrls = answ.content;
		alph_sort(wobj, vec.size);
		return vec.size;
	}
	int size = 0;
	struct dirent *dt;
	DIR *dir = opendir(wobj->pwd);
	while ((dt=readdir(dir)) != NULL) {
		if (!strcmp(dt->d_name, ".") || !strcmp(dt->d_name, "..")) continue;
		if (dotfiles && dt->d_name[0] == '.') continue;
		*ls = realloc(*ls, sizeof(char*)*(size+1));handleMemError(*ls, "realloc(2) on list");
		(*ls)[size] = strdup(dt->d_name);
		size++;
	}
	alph_sort(wobj, size);
	return size;
}

void alph_sort(struct Wobj* wobj, int size) {
	char** ls = wobj->ls;
	char* attrls = wobj->attrls;
	int local = wobj->local;
	for (int i=0; i<size-1; i++) {
		for (int e=0; e<size-1-i; e++) {
			if (strcmp(ls[e],ls[e+1]) > 0) {
				char* temp = ls[e];
				ls[e] = ls[e+1];
				ls[e+1] = temp;
				if (!local) {
					char c = attrls[e];
					attrls[e] = attrls[e+1];
					attrls[e+1] = c;
				}
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

void display_opts(struct TabList *tl, int start, int top, int* ptrs, int mode) {
	struct Wobj* wobj = get_current_tab(tl);
	WINDOW *win = wobj->win;
	char **ls = wobj->ls;
	int size = wobj->cb.nmemb;
	void* data = wobj->data->data;

	struct Nopt* nopt = (struct Nopt*)data;
	char *str = malloc(nopt->str_size); handleMemError(str, "malloc(2) on display_opts"); str[nopt->str_size-1]=0;
	switch (mode) {
		case 0:
			if (size < top) { top = size; }
			int p=0;
			for (int i=start; i<top; i++) {mvwaddstr(win, p, 0, ls[i]);p++;}
			wrefresh(win);
			free(str);
			return;
		case 1:
			memset(str, ' ', nopt->str_size-1);strncpy(str, ls[ptrs[1]], strlen(ls[ptrs[1]]));
			mvwaddstr(win, ptrs[0], 0, str);

			if (nopt->underline) wattron(win, A_UNDERLINE); else wattron(win, COLOR_PAIR(5));
			memset(str, ' ', nopt->str_size-1);strncpy(str, ls[ptrs[1]-1], strlen(ls[ptrs[1]-1]));
			mvwaddstr(win, ptrs[0]-1, 0, str);
			if (nopt->underline) wattroff(win, A_UNDERLINE); else wattroff(win, COLOR_PAIR(5));
			wrefresh(win);
			free(str);
			return;
		case 2:
			memset(str, ' ', nopt->str_size-1);strncpy(str, ls[ptrs[1]], strlen(ls[ptrs[1]]));
			mvwaddstr(win, ptrs[0], 0, str);

			if (nopt->underline) wattron(win, A_UNDERLINE); else wattron(win, COLOR_PAIR(5));
			memset(str, ' ', nopt->str_size-1);strncpy(str, ls[ptrs[1]+1], strlen(ls[ptrs[1]+1]));
			mvwaddstr(win, ptrs[0]+1, 0, str);
			if (nopt->underline) wattroff(win, A_UNDERLINE); else wattroff(win, COLOR_PAIR(5));
			wrefresh(win);
			free(str);
			return;
		case 3:
			memset(str, ' ', nopt->str_size-1);strncpy(str, ls[ptrs[1]], strlen(ls[ptrs[1]]));
			if (nopt->underline) wattron(win, A_UNDERLINE); else wattron(win, COLOR_PAIR(5));
			mvwaddstr(win, ptrs[0], 0, str);
			if (nopt->underline) wattroff(win, A_UNDERLINE); else wattroff(win, COLOR_PAIR(5));
			free(str);
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

wchar_t *geticon(struct Wobj* wobj, int index, char* file) {
	if (W_ISDIR(wobj, index, file)) return L"";
	char *s=getExtension(file);
	if (!s) return L"󰦨 ";
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

int W_ISDIR(struct Wobj *wobj, int index, const char* f) {
	if (wobj->local) {
		struct stat st;
		stat(f, &st);
		if (S_ISDIR(st.st_mode)) return 1;
		return 0;
	}
	if (wobj->attrls[index]-48 == FA_DIR) return 1;
	return 0;
}

int W_ISEXEC(struct Wobj *wobj, int index, const char* f) {
	if (wobj->local) {
		struct stat st;
		stat(f, &st);
		if (!S_ISDIR(st.st_mode) && st.st_mode & S_IXUSR) return 1;
		return 0;
	}
	if (wobj->attrls[index]-48 == FA_EXEC) return 1;
	return 0;
}

void display_files(struct TabList *tl, int start, int top, int *ptrs, int mode) {
	struct Wobj* wobj = get_current_tab(tl);
	WINDOW *win = wobj->win;
	char **ls = wobj->ls;
	int size = wobj->cb.nmemb;
	void* data = wobj->data->data;
	int local = wobj->local;

	char str[50]={0};
	char *pwd = wobj->pwd;
	char *path = NULL;
	int len, nsize;
	switch (mode) {
		case 0:
			if (size < top) top = size; 
			int p=0;
			for (int i=start; i<top; i++) {
				len = strlen(ls[i]);
				char *path = malloc(len+strlen(pwd)+1);
				handleMemError(path, "malloc(2) on display_files");
				strcpy(path,pwd);
				strcat(path,ls[i]);
				nsize = (len<49-2) ?  len: 49-2;
				mvwaddwstr(win, p, 0, geticon(wobj, i, path));
				mvwaddnstr(win, p, 2, ls[i], nsize);
				if (W_ISDIR(wobj, i, path)) mvwaddch(win, p, nsize+2, '/');
				p++;
				free(path);
			}
			wrefresh(win);
			return;
		case 1:
			len = strlen(ls[ptrs[1]]);
			path = malloc(len+strlen(pwd)+1);handleMemError(path, "malloc(2) on display_files");strcpy(path,pwd);strcat(path,ls[ptrs[1]]);
			nsize = (len<49-2) ?  len: 49-2;
			memset(str, ' ', 49);strncpy(str, ls[ptrs[1]], nsize);
			mvwaddwstr(win, ptrs[0], 0, geticon(wobj, ptrs[1], path));
			mvwaddnstr(win, ptrs[0], 2, str, nsize);
			if (W_ISDIR(wobj, ptrs[1], path)) mvwaddch(win, ptrs[0], nsize+2, '/');

			wattron(win, A_UNDERLINE);
			len = strlen(ls[ptrs[1]-1]);
			path = realloc(path, len+strlen(pwd)+1);handleMemError(path, "realloc(2) on display_files");strcpy(path,pwd);strcat(path,ls[ptrs[1]-1]);
			nsize = (len<49-2) ?  len: 49-2;
			memset(str, ' ', 49);strncpy(str, ls[ptrs[1]-1], nsize);
			mvwaddwstr(win, ptrs[0]-1, 0, geticon(wobj, ptrs[1]-1, path));
			mvwaddnstr(win, ptrs[0]-1, 2, str, nsize);
			if (W_ISDIR(wobj, ptrs[1]-1, path)) mvwaddch(win, ptrs[0]-1, nsize+2, '/');
			free(path);
			wattroff(win, A_UNDERLINE);
			wrefresh(win);
			return;
		case 2:
			len = strlen(ls[ptrs[1]]);
			path = malloc(len+strlen(pwd)+1);handleMemError(path, "malloc(2) on display_files");strcpy(path,pwd);strcat(path,ls[ptrs[1]]);
			nsize = (len<49-2) ?  len: 49-2;
			memset(str, ' ', 49);strncpy(str, ls[ptrs[1]], nsize);
			mvwaddwstr(win, ptrs[0], 0, geticon(wobj, ptrs[1], path));
			mvwaddnstr(win, ptrs[0], 2, str, nsize);
			if (W_ISDIR(wobj, ptrs[1], path)) mvwaddch(win, ptrs[0], nsize+2, '/');

			wattron(win, A_UNDERLINE);
			len = strlen(ls[ptrs[1]+1]);
			path = realloc(path, len+strlen(pwd)+1);handleMemError(path, "realloc(2) on display_files");strcpy(path,pwd);strcat(path,ls[ptrs[1]+1]);
			nsize = (len<49-2) ?  len: 49-2;
			memset(str, ' ', 49);strncpy(str, ls[ptrs[1]+1], nsize);
			mvwaddwstr(win, ptrs[0]+1, 0, geticon(wobj, ptrs[1]+1, path));
			mvwaddnstr(win, ptrs[0]+1, 2, str, nsize);
			if (W_ISDIR(wobj, ptrs[1]+1, path)) mvwaddch(win, ptrs[0]+1, nsize+2, '/');
			free(path);
			wattroff(win, A_UNDERLINE);
			wrefresh(win);
			return;
		case 3:
			if (!size) return;
			len = strlen(ls[ptrs[1]]);
			path = malloc(len+strlen(pwd)+1);handleMemError(path, "malloc(2) on display_files");strcpy(path,pwd);strcat(path,ls[ptrs[1]]);
			nsize = (len<49-2) ?  len: 49-2;
			memset(str, ' ', 49-2);strncpy(str, ls[ptrs[1]], nsize+2);
			wattron(win, A_UNDERLINE);
			mvwaddwstr(win, ptrs[0], 0, geticon(wobj, ptrs[1], path));
			mvwaddnstr(win, ptrs[0], 2, str, nsize);
			if (W_ISDIR(wobj, ptrs[1], path)) mvwaddch(win, ptrs[0], nsize+2, '/');
			free(path);
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

int menu(struct TabList *tl, void (*dcb)(struct TabList*,int,int,int*,int)) {
	struct Wobj *wobj = get_current_tab(tl);
	struct TCallback cb = wobj->cb; struct Data *data = wobj->data;
	struct Binding bind = wobj->bind;
	char **ls = wobj->ls; char *pwd = wobj->pwd; WINDOW* win = wobj->win;
	int y; int x; getmaxyx(win, y, x);
	int p = 0;
	int sp = 0;
	int ptrs[2] = {0};
	wobj->data->ptrs = ptrs;
	int top = y;
	int size = cb.nmemb;
	dcb(tl, 0, y, ptrs,  0);
	dcb(tl, 0, top, ptrs, 3);
	for (;;) {
		int ch = wgetch(win);
		if (ch == KEY_UP) {
			if (!sp) continue;
			if (!p) {
				wscrl(win, -1);
				top--;sp--;
				wmove(win,0,0);wclrtobot(win);
				dcb(tl, top-y, top, ptrs, 0);

				ptrs[0]=p+1;ptrs[1]=sp+1;
				dcb(tl,top-y,top,ptrs,1);

			} else {
				ptrs[0]=p;ptrs[1]=sp;
				dcb(tl,top-y,top,ptrs,1);
				sp--;p--;
			}
		}
		else if (ch == KEY_DOWN) {
			if (sp == size-1 || !size) continue;
			if (sp == top-1) {
				wscrl(win, 1);
				top++;sp++;
				wmove(win,0,0);wclrtobot(win);
				dcb(tl, top-y, top, ptrs,  0);
				
				ptrs[0]=p-1;ptrs[1]=sp-1;
				dcb(tl,top-y,top,ptrs,2);
			} else {
				ptrs[0]=p;ptrs[1]=sp;
				dcb(tl,top-y,top,ptrs,2);
				sp++;p++;
			}
		}
		else if (ch == 27) {return 0;}
		else if (ch == 10) {
			if(!size)continue;
			data->ptrs[0] = p; data->ptrs[1] = sp;
			if (cb.func[sp] == NULL) return 1;
			int res = cb.func[sp](tl, data, cb.args[sp]);
			return res;
		} else {
			int index = search_binding(ch, bind);
			data->ptrs[0] = p; data->ptrs[1] = sp;
			if (index != -1) {
				char* param = size ? ls[sp] : NULL;
				return bind.func[index](tl, data, param);
			}
		}
	}
}

int handleFile(struct TabList *tl, struct Data *data, void* f) {
	struct Wobj* wobj = get_current_tab(tl);
	char* name = (char*)f;
	char* pwd = wobj->pwd;
	int local = wobj->local;
	char path[strlen(pwd)+strlen(name)+1];strcpy(path,pwd);strcat(path,name);
	if (W_ISDIR(wobj, wobj->data->ptrs[1], path)) {dir_cd(&pwd, name);wobj->pwd=pwd;}
	else {
		endwin();
		int pid = fork();
		if (!pid) {
			char *cpy = malloc(strlen(pwd)+strlen(name)+1);handleMemError(cpy, "malloc(2) on handleFile");
			strcpy(cpy, pwd);
			strcat(cpy, name);
			if (!local) {
				struct Srvdata svd = high_GetFileData(tl, wobj->fd, cpy);
				free(cpy);
				cpy = high_GetTempFile(name);
				FILE *Tmp = fopen(cpy, "wb");
				if (svd.size) fwrite(svd.content, 1, svd.size, Tmp);
				fclose(Tmp);
				free(svd.content);
			}
			char *argv[] = {(char*)tl->settings.defed, cpy, NULL, NULL};
			if (!local) {argv[2] = "-R";}
			execvp(tl->settings.defed, argv);
			exit(1);
		}
		signal(SIGWINCH, SIG_IGN);
		wait(NULL);
		signal(SIGWINCH, SIG_DFL);
		refresh();
	}
	return 1;
}

int execute(struct TabList *tl, struct Data *data, char* file) {
	if (!file) return 1;
	struct Wobj *wobj = get_current_tab(tl);
	char *pwd = wobj->pwd;
	char*path = malloc(strlen(pwd)+strlen(file)+1);handleMemError(path, "malloc(2) on execute");
	strcpy(path,pwd);strcat(path,file);
	if (!W_ISDIR(wobj, wobj->data->ptrs[1], path) && W_ISEXEC(wobj, wobj->data->ptrs[1], path)) {
		endwin();
		int PID = fork();
		if (!PID) {
			execl(path, path, NULL);
			exit(1);
		}
		signal(SIGWINCH, SIG_IGN);
		wait(NULL);
		signal(SIGWINCH, SIG_DFL);
		struct termios tty, old; tcbreak(&tty, &old);
		printf("\n\033[2K[Presiona una tecla para continuar]\n");
		getchar();
		tnocbreak(&old);
		refresh();
	}
	free(path);
	return 1;
}

int execwargs_argsInput(WINDOW* win, struct Data* data, void* n) {
	char** buff = (char**)n;
	ampsread(win, buff, 3, 7, 20, 100, 0, 1);
	return 1;
}
int execwargs_ok(WINDOW* win, struct Data* data, void* n) {
	int* x = (int*)n;
	*x = !(*x);
	return 1;
}

int execwargs(struct TabList *tl, struct Data *data, char* file) {
	if (!file) return 1;
	struct Wobj *wobj = get_current_tab(tl);
	char *pwd = wobj->pwd;
	char *path = malloc(strlen(pwd)+strlen(file)+1);handleMemError(path, "malloc(2) on execwargs");
	strcpy(path,pwd);strcat(path,file);

	WINDOW* stdscr = data->wins[0];
	int y,x; getmaxyx(stdscr, y, x);
	WINDOW* win = newwin(6, 30, y/2-2, x/2-15);
	wbkgd(win, COLOR_PAIR(1));
	keypad(win, 1);
	wrefresh(win);
	getmaxyx(win, y, x);

	mvwaddstr(win, 0, x/2-8, "Execute with args");
	mvwaddstr(win, 3, 1, "Args: ");
	wrefresh(win);
	struct Mobj a = NewCheck(2,1,0,"As root");
	struct Mobj b = NewField(3,7,20);
	struct Mobj c = NewText(4,x/2-2,"[OK]");
	struct Mobj mobj[3] = {a,b,c};
	int emph_color[2] = {2,4};

	char *buff = NULL;
	int ok = 0;
	int sudo = 0;
	void *args[3] = {&sudo, &buff, &ok};
	int (*func[3])(WINDOW*, struct Data*, void*) = {execwargs_ok, execwargs_argsInput, execwargs_ok};
	struct Callback cb; cb.args=args; cb.func=func; cb.nmemb=3;

	for (;;) {
		if (navigate(win, emph_color, mobj, cb)) {
			if (ok) break;
		} else {
			delwin(win); touchwin(data->wins[4]); wrefresh(data->wins[4]);
			return 1;
		}
	}
	delwin(win); touchwin(data->wins[4]); wrefresh(data->wins[4]);

	if (!W_ISDIR(wobj, wobj->data->ptrs[1], path) && W_ISEXEC(wobj, wobj->data->ptrs[1], path)) {
		struct vector str;
		if (buff!=NULL && strlen(buff)) str = string_split(buff, ' ');
		else {vector_init(&str);};
		str.str=realloc(str.str,sizeof(struct vector)*(str.size+1));handleMemError(str.str, "realloc(2) on execwargs");
		str.str[str.size]=NULL;str.size++;
		char **tmpstr = malloc(sizeof(char**)*(str.size+1));handleMemError(tmpstr, "malloc(2) on execwargs");
		tmpstr[0] = path;
		for (int i=1; i<str.size+1; i++) {
			tmpstr[i] = str.str[i-1];
		}
		free(str.str);
		str.str = tmpstr;
		str.size++;

		endwin();
		int PID = fork();
		if (!PID) {
			chdir(pwd);
			execv(path, str.str);
			exit(1);
		}
		signal(SIGWINCH, SIG_IGN);
		wait(NULL);
		signal(SIGWINCH, SIG_DFL);

		struct termios tty, old; tcbreak(&tty, &old);
		free(tmpstr);
		printf("\n\033[2K[Presiona una tecla para continuar]\n");
		getchar();
		tnocbreak(&old);
		refresh();
	}
	free(path);
	return 1;
}

char* getExtension(char* file) {
	char *s=NULL; int size = 0;
	int f=0;
	for (int i=strlen(file)-1; i>=0; i--) {
		s = realloc(s, size+1);handleMemError(s, "realloc(2) on isImg");
		s[size] = file[i];
		if (file[i] == '.') {f=1;size++;break;}
		size++;
	}
	if (!f) {if (s) free(s); return NULL;}
	s=realloc(s,size+1);handleMemError(s, "realloc(2) on isImg");
	s[size]=0;
	reverse(s);
	return s;
}

int isImg(char* file) {
	if (!file) return 1;
	char* s = getExtension(file);
	if (!s) return 0;
	if (!strcmp(s, ".png") || !strcmp(s, ".jpg")) {free(s);return 1;}
	free(s);
	return 0;
}

void high_SendOrder(int fd, int order, int dig, size_t size, char* param) {
	struct string hstr; string_init(&hstr);
	char *sz = calloc(dig+1, 1); snprintf(sz, dig+1, "%lu", size);
	if (order != -1) string_addch(&hstr, order+48);
	string_add(&hstr, itodg(dig));
	string_add(&hstr, sz);
	string_nadd(&hstr, size, param);
	int p=0;
	while (p != hstr.size) {
		int r = write(fd, hstr.str, hstr.size);
		p += r;
	}
	string_free(&hstr);
	free(sz);
}

struct Srvdata high_GetFileData(struct TabList* tl, int fd, char* path) {
	int size = strlen(path);
	high_SendOrder(fd, OP_DOWNLOAD, enumdig(size), size, path);
	if (!get_err_code(fd)) {
		WINDOW** w = tl->wobj[0].data->wins;
		WINDOW* wins[] = {w[0], w[4], w[5]};
		dialog(wins, "The selected file exceeds the limit of 400MB. Aborted");
		struct Srvdata sd = {0, NULL};
		return sd;
	}
	return get_fdata(tl, fd);
}

void increase_max_tmp(int tmp) {
	char *tmp_path = TMP_PATH;
	char *tp1 = malloc(strlen(tmp_path)+11+1);
	strcpy(tp1, tmp_path); strcat(tp1, "/efti/maxfn");
	FILE* F = fopen(tp1, "w");
	tmp++;
	char *buff = calloc(enumdig(tmp)+1, 1);
	sprintf(buff, "%d", tmp);
	fwrite(buff, 1, enumdig(tmp), F);
	fclose(F);
	free(tp1);
}

char* high_GetTempFile(char *file) {
	char *tmp_path = TMP_PATH;
	int tmp_size = strlen(tmp_path);
	char *tp1 = malloc(tmp_size+6+1);
	char *tp2 = malloc(tmp_size+11+1);
	char *tp3 = malloc(tmp_size+7+1);
	strcpy(tp1, tmp_path); strcat(tp1, "/efti/");
	strcpy(tp2, tmp_path); strcat(tp2, "/efti/maxfn");
	strcpy(tp3, tmp_path); strcat(tp3, "/efti/0");
	create_dir_if_not_exist(tp1);
	FILE* F = fopen(tp2, "r");
	if (!F) {
		increase_max_tmp(0);
		return tp3;
	}
	struct stat st; stat(tp2, &st);
	char *buff = calloc(st.st_size+1, 1);
	fread(buff, 1, st.st_size, F);
	int tmp = atoi(buff);
	fclose(F);
	increase_max_tmp(tmp);
	free(buff);
	char *extension = getExtension(file);
	int extension_size = (extension) ? strlen(extension) : 0;
	buff = calloc(strlen(tp1)+enumdig(tmp)+extension_size+1, 1);
	char* size = calloc(enumdig(tmp)+1, 1);
	snprintf(size, enumdig(tmp)+1, "%d", tmp);
	strcpy(buff, tp1);
	strcat(buff, size);
	if (extension) strcat(buff, extension);
	free(tp1);free(tp2);free(tp3);
	return buff;
}

int view(struct TabList *tl, struct Data *data, char *file) {
	struct Wobj* wobj = get_current_tab(tl);
	char* pwd=wobj->pwd;
	int local = wobj->local;
	if (!file) return 1;
	if (W_ISDIR(wobj, wobj->data->ptrs[1],file) || !isImg(file)) return 1;
	pid_t PID = fork();
	if (!PID) {
		char* param = malloc(strlen(pwd)+strlen(file)+1);handleMemError(param, "malloc(2) on view");
		strcpy(param,pwd);strcat(param,file);
		if (!local) {
			struct Srvdata svd = high_GetFileData(tl, wobj->fd, param);
			if (!svd.size) {
				exit(1);
			}
			free(param);
			param = high_GetTempFile(file);
			FILE *Tmp = fopen(param, "wb");
			fwrite(svd.content, 1, svd.size, Tmp);
			fclose(Tmp);
			free(svd.content);
		}
		close(0);close(1);close(2);
		execlp(tl->settings.defimg, tl->settings.defimg, param, NULL);
		if (!local) remove(param);
		exit(1);
	}
	int status;
	signal(SIGWINCH, SIG_IGN);
	waitpid(PID, &status, 0);
	signal(SIGWINCH, SIG_DFL);
	if (!local && WEXITSTATUS(status)) { 
		WINDOW** w = tl->wobj[0].data->wins;
		WINDOW* wins[] = {w[0], w[4], w[5]};
		dialog(wins, "The selected file exceeds the limit of 400MB. Aborted");
	}
	return 1;
}

int updir(struct TabList *tl, struct Data *data, char* file) {struct Wobj *wobj = get_current_tab(tl);dir_up(&(wobj->pwd));return 1;}

int hideDot(struct TabList *tl, struct Data *data, char* file) {
	struct Fopt* fdata = (struct Fopt*)data->data;
	fdata->dotfiles = fdata->dotfiles ? 0: 1;
	return 1;
}

int menu_close(struct TabList *tl, struct Data *data, void* args) {return 0;}

int fileRename(struct TabList *tl, struct Data *data, char* file) {
	if (!file) return 1;
	struct Wobj *wobj = get_current_tab(tl);
	int fd = wobj->fd;
	char *pwd = wobj->pwd;
	int local = wobj->local;
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
	ampsread(win, &buff, 1, 7, 20, 20, 0, 1);
	delwin(win); touchwin(data->wins[4]); wrefresh(data->wins[4]);
	if (buff==NULL) {free(buff);return 1;}
	char *A = malloc(strlen(pwd)+strlen(file)+1);handleMemError(A, "malloc(2) on fileRename");
	char *B = malloc(strlen(pwd)+strlen(buff)+1);handleMemError(B, "malloc(2) on fileRename");
	strcpy(A, pwd); strcat(A, file);
	strcpy(B, pwd); strcat(B, buff);
	if (local) {rename(A, B);}
	else {
		int A_sz = strlen(A); int B_sz = strlen(B);
		high_SendOrder(fd, OP_MOVE, enumdig(A_sz), A_sz, A);
		high_SendOrder(fd, -1, enumdig(B_sz), B_sz, B);
	}
	free(buff); free(A); free(B);
	return 1;
}

int fselect(struct TabList *tl, struct Data *data, char* file) {
	if (!file) return 1;
	struct Wobj *wobj = get_current_tab(tl);
	char *pwd=wobj->pwd;
	if (tl->tmp_path.path==NULL) {
		char *path = malloc(strlen(pwd)+strlen(file)+1);handleMemError(path, "malloc(2) on fselect");
		strcpy(path, pwd); strcat(path, file);
		tl->tmp_path.path = path;
		tl->tmp_path.id = tl->point;
		mvwaddwstr(data->wins[1],0, 2, L"");
	} else {
		free(tl->tmp_path.path);
		tl->tmp_path.path=NULL;
		tl->tmp_path.id = 0;
		mvwaddstr(data->wins[1],0, 2, " ");
	}
	wrefresh(data->wins[1]);
	return 1;
}

void transfer(struct TabList* tl, struct Wobj* wobj, char* path, char* spath, int rm_after_transf) {
	int spath_sz = strlen(spath); int path_sz = strlen(path);
	WINDOW** w = tl->wobj[0].data->wins;
	WINDOW* wins[] = {w[0], w[4], w[5]};
	if (tl->tmp_path.id && wobj->local) { // remote to local
		int fd = tl->wobj[tl->tmp_path.id].fd;
		high_SendOrder(fd, OP_DOWNLOAD, enumdig(spath_sz), spath_sz, spath);
		if (!get_err_code(fd)) {
			dialog(wins, "The selected file exceeds the limit of 400MB. Aborted");
			return;
		}
		FILE *FN = fopen(path, "wb");
		struct Srvdata sd = get_fdata(tl, fd);
		if (sd.size==-1) {
			return;
		}
		fwrite(sd.content, 1, sd.size, FN);
		fclose(FN);
		free(sd.content);sd.content=NULL;sd.size=0;
		if (rm_after_transf) high_SendOrder(fd, OP_DELETE, enumdig(spath_sz), spath_sz, spath);
	}
	else if (!tl->tmp_path.id && !wobj->local) { // local to remote
		int fd = wobj->fd;
		struct stat st; stat(spath, &st);
		if (st.st_size > TRANSF_LIMIT) {
			dialog(wins, "The selected file exceeds the limit of 400MB. Aborted");
			return;
		}
		FILE *FN = fopen(spath, "rb");
		char *buffer = malloc(st.st_size); 
		fread(buffer, 1, st.st_size, FN);
		fclose(FN);
		high_SendOrder(fd, OP_UPLOAD, enumdig(path_sz), path_sz, path);
		high_SendOrder(fd, -1, enumdig(st.st_size), st.st_size, buffer);
		free(buffer);
		if (rm_after_transf) remove(spath);
	}
	else { // TODO remote to remote
	}
}

int fmove(struct TabList *tl, struct Data *data, char* file) {
	struct Wobj *wobj = get_current_tab(tl);
	if (tl->tmp_path.path==NULL) return 1;
	char* pwd = wobj->pwd;
	char* spath = tl->tmp_path.path;
	int local = wobj->local;
	int fd = wobj->fd;

	WINDOW* stdscr = data->wins[0];
	int y,x; getmaxyx(stdscr, y, x);
	WINDOW* win = newwin(4, 40, y/2-2, x/2-15);
	wbkgd(win, COLOR_PAIR(1));
	keypad(win, 1);
	wrefresh(win);
	getmaxyx(win, y, x);
	mvwaddstr(win, 1, 1, "Press 'y' to MOVE the following file");
	mvwaddstr(win, 2, 1, basename(spath));
	wrefresh(win);
	int ch = wgetch(win);
	if (ch=='y') {
		char *s=basename(spath); int size = 0;
		char* path = malloc(strlen(pwd)+strlen(s)+1);handleMemError(path, "malloc(2) on fmove");
		strcpy(path, pwd);strcat(path, s);

		if (tl->point != tl->tmp_path.id) {  // if trying to move to a different device
			transfer(tl, wobj, path, spath, 1);
		}
		else if (local) {int r = rename(spath, path); handleError(r, -1, "rename");}
		else {
			int spath_sz = strlen(spath); int path_sz = strlen(path);
			high_SendOrder(fd, OP_MOVE, enumdig(spath_sz), spath_sz, spath);
			high_SendOrder(fd, -1, enumdig(path_sz), path_sz, path);
		}
		free(path);

		free(tl->tmp_path.path);
		tl->tmp_path.path=NULL;

		mvwaddstr(data->wins[1],0, 2, " ");
		wrefresh(data->wins[1]);
	}
	delwin(win); touchwin(data->wins[4]); wrefresh(data->wins[4]);
	return 1;
}

void copy(struct Wobj* wobj, char *A, char *B) {
	struct stat st;
	stat(A, &st);
	if (S_ISDIR(st.st_mode)) return;
	FILE *FA = fopen(A, "rb");
	char *buff = malloc(st.st_size);handleMemError(buff, "malloc(2) on copy");
	fread(buff, 1, st.st_size, FA);

	FILE *FB = fopen(B, "wb");
	fwrite(buff, 1, st.st_size, FB);

	fclose(FA); fclose(FB);
	free(buff);
}

int fcopy(struct TabList *tl, struct Data *data, char* file) {
	struct Wobj *wobj = get_current_tab(tl);
	if (tl->tmp_path.path==NULL) return 1;
	char *pwd = wobj->pwd;
	char *spath = tl->tmp_path.path;
	char *s=basename(spath);
	int local = wobj->local;
	int fd = wobj->fd;
	char *path = malloc(strlen(pwd)+strlen(s)+1);handleMemError(path, "malloc(2) on fcopy");
	strcpy(path,pwd); strcat(path,s);
	if (tl->point != tl->tmp_path.id) {  // if trying to move to a different device
		transfer(tl, wobj, path, spath, 0);
	}
	else if (local) {copy(wobj,spath,path);}
	else {
		int spath_sz = strlen(spath); int path_sz = strlen(path);
		high_SendOrder(fd, OP_COPY, enumdig(spath_sz), spath_sz, spath);
		high_SendOrder(fd, -1, enumdig(path_sz), path_sz, path);
	}
	
	free(tl->tmp_path.path);
	tl->tmp_path.path = NULL;

	mvwaddstr(data->wins[1],0, 2, " ");
	wrefresh(data->wins[1]);
	free(path);
	return 1;
}

int fdelete(struct TabList *tl, struct Data *data, char* file) {
	if (!file) return 1;

	struct Wobj *wobj = get_current_tab(tl);
	WINDOW* stdscr = data->wins[0];
	int local = wobj->local;
	int fd = wobj->fd;
	int y,x; (void)getmaxyx(stdscr, y, x);
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
		char *pwd = wobj->pwd;
		char *path = malloc(strlen(pwd)+strlen(file)+1);handleMemError(path, "malloc(2) on fdelete");
		(void) strcpy(path, pwd); (void) strcat(path, file);
		if (local) {_= remove(path); handleError(_, -1, "fdelete: remove");}
		else {
			int path_sz = strlen(path);
			high_SendOrder(fd, OP_DELETE, enumdig(path_sz), path_sz, path);
		}
		(void) free(path);
	}
	delwin(win); touchwin(data->wins[4]); wrefresh(data->wins[4]);
	return 1;
}

int fnew(struct TabList *tl, struct Data *data, char* file) {
	struct Wobj *wobj = get_current_tab(tl);
	char *pwd = wobj->pwd;
	WINDOW* stdscr = data->wins[0];
	int local = wobj->local;
	int fd = wobj->fd;
	int y,x; getmaxyx(stdscr, y, x);
	WINDOW* win = newwin(3, 30, y/2-5, x/2-15);
	wbkgd(win, COLOR_PAIR(1));
	keypad(win, 1);
	wrefresh(win);
	getmaxyx(win, y, x);
	mvwaddstr(win, 0, x/2-4, "New file");
	mvwaddstr(win, 1, 1, "Name:");
	wrefresh(win);
	char *buff;
	(void) ampsread(win, &buff, 1, 7, 20, 20, 0, 1);
	delwin(win); touchwin(data->wins[4]); wrefresh(data->wins[4]);
	if (buff==NULL) return 1;
	char *path = malloc(strlen(pwd)+strlen(buff)+1);handleMemError(path, "malloc(2) on fnew");
	(void) strcpy(path,pwd); (void)strcat(path, buff);
	if (local) {
		struct stat st;
		if (stat(path, &st) != -1) return 1;
		FILE *F = fopen(path, "w"); (void)fclose(F);
	}
	else {
		int path_sz = strlen(path);
		high_SendOrder(fd, OP_NEW_FILE, enumdig(path_sz), path_sz, path);
	}
	(void) free(path);
	return 1;
}

int dnew(struct TabList *tl, struct Data *data, char* file) {
	struct Wobj *wobj = get_current_tab(tl);
	char *pwd = wobj->pwd;
	int local = wobj->local;
	int fd = wobj->fd;
	WINDOW* stdscr = data->wins[0];
	int y,x; (void) getmaxyx(stdscr, y, x);
	WINDOW* win = newwin(3, 30, y/2-5, x/2-15);
	wbkgd(win, COLOR_PAIR(1));
	keypad(win, 1);
	wrefresh(win);
	getmaxyx(win, y, x);
	mvwaddstr(win, 0, x/2-6, "New directory");
	mvwaddstr(win, 1, 1, "Name:");
	wrefresh(win);
	char *buff;
	(void) ampsread(win, &buff, 1, 7, 20, 20, 0, 1);
	delwin(win); touchwin(data->wins[4]); wrefresh(data->wins[4]);
	if (buff==NULL) return 1;
	char *path = malloc(strlen(pwd)+strlen(buff)+1);handleMemError(path, "malloc(2) on dnew");
	(void) strcpy(path, pwd); (void)strcat(path, buff);
	if (local) {
		struct stat st;
		if (stat(path, &st) != -1) return 1;
		_= mkdir(path, 0700); handleError(_,-1,"dnew: mkdir");
	}
	else {
		int path_sz = strlen(path);
		high_SendOrder(fd, OP_NEW_DIR, enumdig(path_sz), path_sz, path);
	}
	(void) free(path);
	return 1;
}

void tab_init(struct TabList *tl) {tl->list=NULL;tl->point=0; tl->size=0;tl->wobj=NULL;}

void add_tab(WINDOW* tabwin, struct TabList *tl) {
	if (tl->list != NULL) {
		char str[4] = {' ', tl->list[tl->point]+48, ' ', 0};
		wattron(tabwin, COLOR_PAIR(6));
		mvwaddstr(tabwin, 0, tl->point*3, str);
		wattroff(tabwin, COLOR_PAIR(6));
		wrefresh(tabwin);
	}
	tl->wobj = realloc(tl->wobj, sizeof(struct Wobj)*(tl->size+1));handleMemError(tl->wobj, "realloc(2) on add_tab");
	tl->list = realloc(tl->list, tl->size+1);handleMemError(tl->wobj, "realloc(2) on add_tab");
	tl->list[tl->size] = tl->size+1;
	char str[4] = {' ', tl->list[tl->size]+48, ' ', 0};
	wattron(tabwin, COLOR_PAIR(7));
	mvwaddstr(tabwin, 0, tl->size*3, str);
	wattroff(tabwin, COLOR_PAIR(7));
	wrefresh(tabwin);
	tl->point = tl->size;
	tl->size++;
}

void del_tab(WINDOW* tabwin, struct TabList *tl) {
	struct Wobj* wobj = get_current_tab(tl);
	if (wobj->local) return;
	tl->wobj = realloc(tl->wobj, sizeof(struct Wobj)*(tl->size-1));handleMemError(tl->wobj, "realloc(2) on del_tab");
	tl->list = realloc(tl->list, tl->size-1);handleMemError(tl->list, "realloc(2) on del_tab");
	tl->size--;
	tl->point = tl->size-1;
	wmove(tabwin,0,0);wclrtoeol(tabwin);
	for (int i=0; i<tl->size; i++) {
		char str[4] = {' ', tl->list[i]+48, ' ', 0};
		wattron(tabwin, COLOR_PAIR(6));
		mvwaddstr(tabwin, 0, i*3, str);
		wattroff(tabwin, COLOR_PAIR(6));
	}
	if (tl->size) {
		char str[4] = {' ', tl->list[tl->point]+48, ' ', 0};
		wattron(tabwin, COLOR_PAIR(7));
		mvwaddstr(tabwin, 0, tl->point*3, str);
		wattroff(tabwin, COLOR_PAIR(7));
	}
	wrefresh(tabwin);
}

void tab_switch(WINDOW* tabwin, struct TabList *tl) {
	char str[4] = {' ', tl->list[tl->point]+48, ' ', 0};
	wattron(tabwin, COLOR_PAIR(6));
	mvwaddstr(tabwin, 0, tl->point*3, str);
	wattroff(tabwin, COLOR_PAIR(6));
	wrefresh(tabwin);
	if (tl->point == tl->size-1) {tl->point = 0;}
	else {tl->point++;}
	str[1] = tl->list[tl->point]+48;
	wattron(tabwin, COLOR_PAIR(7));
	mvwaddstr(tabwin, 0, tl->point*3, str);
	wattroff(tabwin, COLOR_PAIR(7));
	wrefresh(tabwin);
}

int b_tab_switch(struct TabList *tl, struct Data* data, char* file) {
	WINDOW* tabwin = tl->wobj[0].data->wins[2];
	tab_switch(tabwin, tl);
	return 1;
}

struct Wobj* get_current_tab(struct TabList *tl) {return &(tl->wobj[tl->point]);}

struct Mobj NewText(int y, int x, const char* text) {
	struct ObjText a;
	a.text = text;
	a.yx[0]=y; a.yx[1]=x;
	struct Mobj m; m.id=T_TEXT; m.object.text=a;
	return m;
}
struct Mobj NewField(int y, int x, int size) {
	struct ObjField a;
	a.size = size;
	a.yx[0]=y; a.yx[1]=x;
	struct Mobj m; m.id=T_FIELD; m.object.field=a;
	return m;
}
struct Mobj NewCheck(int y, int x, int checked, const char* text) {
	struct ObjCheck a;
	a.checked=checked;
	a.text=text;
	a.yx[0]=y; a.yx[1]=x;
	struct Mobj m; m.id=T_CHECKBOX; m.object.checkbox=a;
	return m;
}
struct Mobj NewRect(int y1, int x1, int y2, int x2) {
	struct ObjRect a;
	a.coords[0]=y1; a.coords[1]=x1;
	a.coords[2]=y2; a.coords[3]=x2;
	struct Mobj m; m.id=T_RECTANGLE; m.object.rectangle=a;
	return m;
}

void print_mobj(WINDOW* win, int color, struct Mobj mobj) {
	switch (mobj.id) {
		case T_TEXT: {
			int * yx = mobj.object.text.yx;
			wattron(win, COLOR_PAIR(color));
			mvwaddstr(win, yx[0], yx[1], mobj.object.text.text);
			wattroff(win, COLOR_PAIR(color));
			break;
		}
		case T_FIELD: {
			int *yx = mobj.object.field.yx;
			for (int i=0; i<mobj.object.field.size; i++) {
				mvwaddch(win, yx[0], yx[1]+i, (mvwinch(win, yx[0], yx[1]+i)&A_CHARTEXT)|COLOR_PAIR(color));
				wrefresh(win);
			}
			break;
		}
		case T_CHECKBOX: {
			int *yx = mobj.object.checkbox.yx;
			char str[5] = {'(', ' ', ')', ' ', 0};
			if (mobj.object.checkbox.checked) str[1] = 'x';
			mvwaddstr(win, yx[0], yx[1], str);
			wattron(win, COLOR_PAIR(color));
			waddstr(win, mobj.object.checkbox.text);
			wattroff(win, COLOR_PAIR(color));
			break;
		}
		case T_RECTANGLE: {
			wattron(win, COLOR_PAIR(color));
			int *coords = mobj.object.rectangle.coords;
			mvwaddch(win, coords[0], coords[1], ACS_ULCORNER);
			for (int i=coords[1]+1; i<coords[3]; i++) {mvwaddch(win,coords[0],i,ACS_HLINE);}
			waddch(win, ACS_URCORNER);
			for (int i=coords[0]+1; i<coords[2]; i++) {
				mvwaddch(win, i, coords[1], ACS_VLINE);
				mvwaddch(win, i, coords[3], ACS_VLINE);
			}
			mvwaddch(win, coords[2], coords[1], ACS_LLCORNER);
			for (int i=coords[1]+1; i<coords[3]; i++) {mvwaddch(win, coords[2], i, ACS_HLINE);}
			waddch(win, ACS_LRCORNER);
			wattroff(win, COLOR_PAIR(color));
			break;
		}
	}
	wrefresh(win);
}

int navigate(WINDOW* win, int emph_color[2], struct Mobj *mobj, struct Callback cb) {
	int size = cb.nmemb;
	for (int i=0; i<size; i++) { print_mobj(win, 0, mobj[i]); }
	int attr = (mobj[0].id==3) ? emph_color[0] : emph_color[1];
	print_mobj(win, attr, mobj[0]);
	int p = 0;
	for (;;) {
		int ch = wgetch(win);
		switch (ch) {
			case 27:
				return 0;
			case 10:
				if (mobj[p].id == 2) {
					mobj[p].object.checkbox.checked = !mobj[p].object.checkbox.checked;
					print_mobj(win, emph_color[1], mobj[p]);
				}
				if (cb.func[p] == NULL) return 1;
				return cb.func[p](win, NULL, cb.args[p]);
				break;
			case KEY_UP: {
				if (!p) continue;
				print_mobj(win, 0, mobj[p]);
				int attr = (mobj[p-1].id==3) ? emph_color[0] : emph_color[1];
				print_mobj(win, attr, mobj[p-1]);
				p--;
				break;
			}
			case KEY_DOWN: {
				if (p == size-1) continue;
				print_mobj(win, 0, mobj[p]);
				int attr = (mobj[p+1].id==3) ? emph_color[0] : emph_color[1];
				print_mobj(win, attr, mobj[p+1]);
				p++;
				break;
			}
		}
	}
	return 1;
}

void load_settings(struct TabList* tl) {
	char* home = getenv("HOME");
	int home_len = strlen(home);
	char* path = malloc(home_len+19+1);
	strcpy(path, home);
	strcat(path, "/.local/share/efti/");
	char* fpath = strdup(path);
	fpath = realloc(fpath, home_len+19+9+1);
	strcat(fpath, "conf.json");
	create_dir_if_not_exist(path);
	struct stat st;
	if (stat(fpath, &st) == -1 && errno == ENOENT) {
		tl->settings.srv_local = 0;
		tl->settings.port = "4545";
		struct stat st;
		if (!TERMUX) {
			if (stat("/usr/bin/nvim", &st)==-1 && errno==ENOENT) {
				tl->settings.defed = "/usr/bin/vim";
			} else tl->settings.defed = "/usr/bin/nvim";
		} else {
			if (stat("/data/data/com.termux/files/usr/bin/nvim", &st)==-1 && errno==ENOENT) {
				tl->settings.defed = "/data/data/com.termux/files/usr/bin/vim";
			} else tl->settings.defed = "/data/data/com.termux/files/usr/bin/nvim";
		}
		tl->settings.defimg = TERMUX ? "/data/data/com.termux/files/usr/bin/termux-open" : "/usr/bin/feh";
		struct json_object* jobj = json_object_new_object();
		json_object_object_add(jobj, "srv_local", json_object_new_int(0));
		json_object_object_add(jobj, "port", json_object_new_string(tl->settings.port));
		json_object_object_add(jobj, "defed", json_object_new_string(tl->settings.defed));
		json_object_object_add(jobj, "defimg", json_object_new_string(tl->settings.defimg));
		FILE *F = fopen(fpath, "w");
		fputs(json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_PRETTY), F);
		fclose(F);
		free(path);free(fpath);
		json_object_object_del(jobj, "srv_local");
		json_object_object_del(jobj, "port");
		json_object_object_del(jobj, "defed");
		json_object_object_del(jobj, "defimg");
		return;
	}
	FILE* F = fopen(fpath, "r");
	char* buff = calloc(st.st_size+1, 1);
	fread(buff, 1, st.st_size, F);
	fclose(F);
	struct json_object* jobj = json_tokener_parse(buff);
	struct json_object* lan;
	struct json_object* port;
	struct json_object* defed;
	struct json_object* defimg;
	json_object_object_get_ex(jobj, "srv_local", &lan);
	json_object_object_get_ex(jobj, "port", &port);
	json_object_object_get_ex(jobj, "defed", &defed);
	json_object_object_get_ex(jobj, "defimg", &defimg);
	tl->settings.srv_local = json_object_get_int(lan);
	tl->settings.port = json_object_get_string(port);
	tl->settings.defed = json_object_get_string(defed);
	tl->settings.defimg = json_object_get_string(defimg);
	free(path);free(fpath);free(buff);
}

void write_settings(struct TabList* tl) {
	char* home = getenv("HOME");
	int home_len = strlen(home);
	char* path = malloc(home_len+19+1);
	strcpy(path, home);
	strcat(path, "/.local/share/efti/");
	char* fpath = strdup(path);
	fpath = realloc(fpath, home_len+19+9+1);
	strcat(fpath, "conf.json");
	FILE* F = fopen(fpath, "w");
	struct json_object* jobj = json_object_new_object();
	json_object_object_add(jobj, "srv_local", json_object_new_int(tl->settings.srv_local));
	json_object_object_add(jobj, "port", json_object_new_string(tl->settings.port));
	json_object_object_add(jobj, "defed", json_object_new_string(tl->settings.defed));
	json_object_object_add(jobj, "defimg", json_object_new_string(tl->settings.defimg));
	fputs(json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_PRETTY), F);
	fclose(F);
	free(path);free(fpath);	
	json_object_object_del(jobj, "srv_local");
	json_object_object_del(jobj, "port");
	json_object_object_del(jobj, "defed");
	json_object_object_del(jobj, "defimg");
	free(jobj);
}

int mod_strsetting(WINDOW* win, struct Data* data, void* d) {
	void** x = (void**)d;
	char** S = (char**)x[0];
	int y = ((int*)x[1])[0];
	int X = ((int*)x[1])[1];
	int charlim = ((int*)x[1])[2];
	ampsread(win, S, y, X, 15, charlim, 0, 1);
	return 1;
}

int mod_boolsetting(WINDOW* tl, struct Data* data, void* d) {
	int* S = (int*)d;
	*S = !(*S);
	return 1;
}

int settings(struct TabList* tl, struct Data* data, void* d) {
	struct TabList *otl = (struct TabList*)d;
	WINDOW* wr = otl->wobj[0].data->wins[4];
	WINDOW* wfiles = otl->wobj[0].data->wins[5];
	WINDOW* main = otl->wobj[0].data->wins[4];
	WINDOW* tabwin = otl->wobj[0].data->wins[2];
	WINDOW* stdscr = otl->wobj[0].data->wins[0];
	WINDOW* wins[] = {stdscr, main, wfiles};
	int y, x; getmaxyx(stdscr, y, x);
	WINDOW* wsettings = newwin(9, 45, y/2-4, x/2-22);
	getmaxyx(wsettings, y, x);
	wbkgd(wsettings, COLOR_PAIR(1));
	keypad(wsettings, 1);
	box(wsettings, ACS_VLINE, ACS_HLINE);
	mvwaddstr(wsettings, 1, x/2-4, "Settings");
	wrefresh(wsettings);
	int emph_color[2] = {2, 4};
	struct Mobj mobj[4] = {
		NewField(3, 23, 15),
		NewField(4, 28, 15),
		NewField(5, 15, 6),
		NewCheck(6, 2, otl->settings.srv_local, "Server: Local only")
	};

	mvwaddnstr(wsettings, 3, 23, otl->settings.defed, 15);
	mvwaddnstr(wsettings, 4, 28, otl->settings.defimg, 15);
	mvwaddnstr(wsettings, 5, 15, otl->settings.port, 6);
	wrefresh(wsettings);
	int na[] = {3,23,15}; int nb[] = {4,28,15}; int nc[] = {5,15,6};
	void* a[3] = {&otl->settings.defed, na};
	void* b[2] = {&otl->settings.defimg, nb};
	void* c[2] = {&otl->settings.port, &nc};
	int (*func[])(WINDOW*, struct Data*, void*) = {mod_strsetting, mod_strsetting, mod_strsetting, mod_boolsetting};
	void *args[] = {a, b, c, &otl->settings.srv_local};
	struct Callback cb = {func, args, 4};
	for (;;) {
		mvwaddstr(wsettings, 3, 2, "Default text editor: ");
		mvwaddstr(wsettings, 4, 2, "Default image visualizer: ");
		mvwaddstr(wsettings, 5, 2, "Server port: ");
		int res = navigate(wsettings, emph_color, mobj, cb);
		if (!res) break;
		write_settings(otl);
	}
	delwin(wsettings);
	touchwin(main); touchwin(wfiles);
	wrefresh(main); wrefresh(wfiles);
	return 1;
}

/*
󰙯  
*/
