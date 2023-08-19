#include <curses.h>
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
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "gears.h"
#include "libncread/ncread.h"
#include "libncread/vector.h"

int _;
void handleError(int c, int n, const char* str) {
	if (c!=n)return;
	(void) endwin();
	(void) puts("\t\033[1;31mEFTI has encountered an error.\033[0m");
	(void) perror(str);
	exit(1);
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

void display_opts(struct TabList *tl, int start, int top, int* ptrs, int mode) {
	struct Wobj wobj = get_current_tab(tl);
	WINDOW *win = wobj.win;
	char **ls = wobj.ls;
	int size = wobj.cb.nmemb;
	void* data = wobj.data->data;

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

void display_files(struct TabList *tl, int start, int top, int *ptrs, int mode) {
	struct Wobj wobj = get_current_tab(tl);
	WINDOW *win = wobj.win;
	char **ls = wobj.ls;
	int size = wobj.cb.nmemb;
	void* data = wobj.data->data;
	int local = wobj.local;

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
				if (local) stat(path, &st);
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
			if (local) {stat(path, &st); if (S_ISDIR(st.st_mode)) mvwaddch(win, ptrs[0], nsize+2, '/');}

			wattron(win, A_UNDERLINE);
			len = strlen(ls[ptrs[1]-1]);
			path = realloc(path, len+strlen(pwd)+1);strcpy(path,pwd);strcat(path,ls[ptrs[1]-1]);
			nsize = (len<49-2) ?  len: 49-2;
			memset(str, ' ', 49);strncpy(str, ls[ptrs[1]-1], nsize);
			mvwaddwstr(win, ptrs[0]-1, 0, geticon(path));
			mvwaddnstr(win, ptrs[0]-1, 2, str, nsize);
			if (local) {stat(path, &st); if (S_ISDIR(st.st_mode)) mvwaddch(win, ptrs[0]-1, nsize+2, '/');}
			free(path);
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
			if (local) {stat(path, &st); if (S_ISDIR(st.st_mode)) mvwaddch(win, ptrs[0], nsize+2, '/');}

			wattron(win, A_UNDERLINE);
			len = strlen(ls[ptrs[1]+1]);
			path = realloc(path, len+strlen(pwd)+1);strcpy(path,pwd);strcat(path,ls[ptrs[1]+1]);
			nsize = (len<49-2) ?  len: 49-2;
			memset(str, ' ', 49);strncpy(str, ls[ptrs[1]+1], nsize);
			mvwaddwstr(win, ptrs[0]+1, 0, geticon(path));
			mvwaddnstr(win, ptrs[0]+1, 2, str, nsize);
			if (local) {stat(path, &st); if (S_ISDIR(st.st_mode)) mvwaddch(win, ptrs[0]+1, nsize+2, '/');}
			free(path);
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
			if (local) {stat(path, &st); if (S_ISDIR(st.st_mode)) mvwaddch(win, ptrs[0], nsize+2, '/');}
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
	struct Wobj wobj = get_current_tab(tl);
	struct TCallback cb = wobj.cb; struct Data *data = wobj.data;
	struct Binding bind = wobj.bind;
	char **ls = wobj.ls; char *pwd = wobj.pwd; WINDOW* win = wobj.win;
	int y; int x; getmaxyx(win, y, x);
	int p = 0;
	int sp = 0;
	int ptrs[2] = {0};
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
			if (index != -1) {
				char* param = size ? ls[sp] : NULL;
				return bind.func[index](tl, data, param);
			}
		}
	}
}

int handleFile(struct TabList *tl, struct Data *data, void* f) {
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

int execute(struct TabList *tl, struct Data *data, char* file) {
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
	ampsread(win, buff, 3, 7, 20, 100, 0);
	return 1;
}
int execwargs_ok(WINDOW* win, struct Data* data, void* n) {
	int* x = (int*)n;
	*x = !(*x);
	return 1;
}

int execwargs(struct TabList *tl, struct Data *data, char* file) {
	if (!file) return 1;
	char *pwd = ((struct Fopt*)data->data)->pwd;
	char *path = malloc(strlen(pwd)+strlen(file)+1);
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

	/*execution process begins*/
	delwin(win); touchwin(data->wins[4]); wrefresh(data->wins[4]);

	struct stat st;
	stat(path, &st);
	if (!S_ISDIR(st.st_mode) && st.st_mode & S_IXUSR) {  /*If it's not dir and is executable*/
		/*prepare argument vector*/
		struct vector str;
		if (buff!=NULL && strlen(buff)) str = string_split(buff, ' ');
		else {vector_init(&str);};
		str.str=realloc(str.str,sizeof(struct vector)*(str.size+1));
		str.str[str.size]=NULL;str.size++;
		char **tmpstr = malloc(sizeof(char**)*(str.size+1));
		tmpstr[0] = path;
		for (int i=1; i<str.size+1; i++) {
			tmpstr[i] = str.str[i-1];
		}
		free(str.str);
		str.str = tmpstr;
		str.size++;

		/*execution*/
		endwin();
		int PID = fork();
		if (!PID) {
			chdir(pwd);
			execv(path, str.str);
			exit(1);
		}
		wait(NULL);

		/*end execution*/
		struct termios tty, old; tcbreak(&tty, &old);
		printf("\n\033[2K[Presiona una tecla para continuar]\n");
		getchar();
		tnocbreak(&old);
		refresh();
	}
	free(path);
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
	if (!strcmp(s, ".png") || !strcmp(s, ".jpg")) {free(s);return 1;}
	free(s);
	return 0;
}

int view(struct TabList *tl, struct Data *data, char *file) {
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

int updir(struct TabList *tl, struct Data *data, char* file) {dir_up(((struct Fopt*)data->data)->pwd);return 1;}

int hideDot(struct TabList *tl, struct Data *data, char* file) {
	struct Fopt* fdata = (struct Fopt*)data->data;
	fdata->dotfiles = fdata->dotfiles ? 0: 1;
	return 1;
}

int menu_close(struct TabList *tl, struct Data *data, void* args) {return 0;}

int fileRename(struct TabList *tl, struct Data *data, char* file) {
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
	delwin(win); touchwin(data->wins[4]); wrefresh(data->wins[4]);
	if (buff==NULL) {free(buff);return 1;}
	char *A = malloc(strlen(pwd)+strlen(file)+1);
	char *B = malloc(strlen(pwd)+strlen(buff)+1);
	strcpy(A, pwd); strcat(A, file);
	strcpy(B, pwd); strcat(B, buff);
	rename(A, B);
	free(buff); free(A); free(B);
	return 1;
}

int fselect(struct TabList *tl, struct Data *data, char* file) {
	if (!file) return 1;
	struct Fopt *fdata = (struct Fopt*)data->data;
	char *pwd = fdata->pwd;
	if (fdata->tmp_path==NULL) {
		char *path = malloc(strlen(pwd)+strlen(file)+1);
		strcpy(path, pwd); strcat(path, file);
		fdata->tmp_path = path;
		mvwaddwstr(data->wins[1],0, 2, L"");
	} else {
		free(fdata->tmp_path);
		fdata->tmp_path=NULL;
		mvwaddstr(data->wins[1],0, 2, " ");
	}
	wrefresh(data->wins[1]);
	return 1;
}

int fmove(struct TabList *tl, struct Data *data, char* file) {
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
		if (rename(spath, path) == -1) {
			endwin();
			perror("Rename");
			exit(1);
		}
		free(s); free(path);

		free(fdata->tmp_path);
		fdata->tmp_path=NULL;

		mvwaddstr(data->wins[1],0, 2, " ");
		wrefresh(data->wins[1]);
	}
	delwin(win); touchwin(data->wins[4]); wrefresh(data->wins[4]);
	return 1;
}

void copy(char *A, char *B) {
	struct stat st;
	stat(A, &st);
	if S_ISDIR(st.st_mode) return;
	FILE *FA = fopen(A, "r");
	char *buff = malloc(st.st_size);
	fread(buff, 1, st.st_size, FA);

	FILE *FB = fopen(B, "w");
	fwrite(buff, 1, st.st_size, FB);

	fclose(FA); fclose(FB);
	free(buff);
}

int fcopy(struct TabList *tl, struct Data *data, char* file) {
	struct Fopt*fdata = (struct Fopt*)data->data;
	if (fdata->tmp_path==NULL) return 1;
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
	(void) strcpy(path,pwd); (void)strcat(path,s);
	(void) copy(spath,path);
	
	(void) free(fdata->tmp_path);
	fdata->tmp_path = NULL;

	mvwaddstr(data->wins[1],0, 2, " ");
	wrefresh(data->wins[1]);
	(void) free(s); (void)free(path);
	return 1;
}

int fdelete(struct TabList *tl, struct Data *data, char* file) {
	if (!file) return 1;

	WINDOW* stdscr = data->wins[0];
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
		char *pwd = ((struct Fopt*)data->data)->pwd;
		char *path = malloc(strlen(pwd)+strlen(file)+1);
		(void) strcpy(path, pwd); (void) strcat(path, file);
		_= remove(path); handleError(_, -1, "fdelete: remove");
		(void) free(path);
	}
	delwin(win); touchwin(data->wins[4]); wrefresh(data->wins[4]);
	return 1;
}

int fnew(struct TabList *tl, struct Data *data, char* file) {
	char *pwd = ((struct Fopt*)data->data)->pwd;
	WINDOW* stdscr = data->wins[0];
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
	(void) ampsread(win, &buff, 1, 7, 20, 20, 0);
	delwin(win); touchwin(data->wins[4]); wrefresh(data->wins[4]);
	if (buff==NULL) return 1;
	char *path = malloc(strlen(pwd)+strlen(buff)+1);
	(void) strcpy(path,pwd); (void)strcat(path, buff);
	struct stat st;
	if (stat(path, &st) != -1) return 1;
	FILE *F = fopen(path, "w"); (void)fclose(F);
	(void) free(path);
	return 1;
}

int dnew(struct TabList *tl, struct Data *data, char* file) {
	char *pwd = ((struct Fopt*)data->data)->pwd;
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
	(void) ampsread(win, &buff, 1, 7, 20, 20, 0);
	delwin(win); touchwin(data->wins[4]); wrefresh(data->wins[4]);
	if (buff==NULL) return 1;
	char *path = malloc(strlen(pwd)+strlen(buff)+1);
	(void) strcpy(path, pwd); (void)strcat(path, buff);
	struct stat st;
	if (stat(path, &st) != -1) return 1;
	_= mkdir(path, 0700); handleError(_,-1,"dnew: mkdir");
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
	tl->wobj = realloc(tl->wobj, tl->size+1);
	tl->list = realloc(tl->list, tl->size+1);
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
	tl->list = realloc(tl->list, tl->size-1);
	tl->size--;
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

struct Wobj get_current_tab(struct TabList *tl) {return tl->wobj[tl->point];}

struct Mobj NewText(int y, int x, const char* text) {
	struct ObjText a;
	a.text = text;
	a.yx[0]=y; a.yx[1]=x;
	struct Mobj m; m.id=0; m.object.text=a;
	return m;
}
struct Mobj NewField(int y, int x, int size) {
	struct ObjField a;
	a.size = size;
	a.yx[0]=y; a.yx[1]=x;
	struct Mobj m; m.id=1; m.object.field=a;
	return m;
}
struct Mobj NewCheck(int y, int x, int checked, const char* text) {
	struct ObjCheck a;
	a.checked=checked;
	a.text=text;
	a.yx[0]=y; a.yx[1]=x;
	struct Mobj m; m.id=2; m.object.checkbox=a;
	return m;
}
struct Mobj NewRect(int y1, int x1, int y2, int x2) {
	struct ObjRect a;
	a.coords[0]=y1; a.coords[1]=x1;
	a.coords[2]=y2; a.coords[3]=x2;
	struct Mobj m; m.id=3; m.object.rectangle=a;
	return m;
}

void print_mobj(WINDOW* win, int color, struct Mobj mobj) {
	switch (mobj.id) {
		case 0: /*text*/ {
			int * yx = mobj.object.text.yx;
			wattron(win, COLOR_PAIR(color));
			mvwaddstr(win, yx[0], yx[1], mobj.object.text.text);
			wattroff(win, COLOR_PAIR(color));
			break;
		}
		case 1: /*field*/ {
			int *yx = mobj.object.field.yx;
			for (int i=0; i<mobj.object.field.size; i++) {
				mvwaddch(win, yx[0], yx[1]+i, (mvwinch(win, yx[0], yx[1]+i)&A_CHARTEXT)|COLOR_PAIR(color));
				wrefresh(win);
			}
			break;
		}
		case 2: /*checkbox*/ {
			int *yx = mobj.object.checkbox.yx;
			char str[5] = {'(', ' ', ')', ' ', 0};
			if (mobj.object.checkbox.checked) str[1] = 'x';
			mvwaddstr(win, yx[0], yx[1], str);
			wattron(win, COLOR_PAIR(color));
			waddstr(win, mobj.object.checkbox.text);
			wattroff(win, COLOR_PAIR(color));
			break;
		}
		case 3: /*rectangle*/ {
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

/*
󰙯  
*/
