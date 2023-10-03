#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ncurses.h>
#include <dirent.h>
#include <locale.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include "gears.h"
#include "efti_srv.h"
#include "logger/logger.h"

int SERVER_STATUS = 0;
int popup_menu(struct TabList *tl, struct Data *data, char* file);

int main() {
	setlocale(LC_ALL, "");
	signal(SIGTERM, SIG_IGN);
	WINDOW* stdscr = initscr();
	curs_set(0);
	noecho();
	start_color();
	init_pair(1, 15, 160); /*fondo rojo con letras blancas*/
	init_pair(2, 15, 16); /*fondo negro con letras blancas*/
	init_pair(3, 15, 237); /*fondo gris con letras blancas*/
	init_pair(4, 207, 237); /*fondo gris con letras rosas*/
	init_pair(5, 16, 15); /*fondo blanco con letras negras*/

	init_pair(6, 15, 237); /*fondo gris oscuro con letras blancas*/
	init_pair(7, 15, 240); /*fondo gris claro con letras blancas*/
	init_pair(8, 40, 160); /*fondo rojo con letras verdes*/
	wbkgd(stdscr, COLOR_PAIR(2));

	int std_y, std_x; getmaxyx(stdscr, std_y, std_x);
	WINDOW* upbar = newwin(1,std_x,0,0);
	WINDOW* tabwin = newwin(1, std_x-4, std_y-2, 2);
	WINDOW* lowbar = newwin(1, std_x, std_y-1, 0);

	refresh();wrefresh(upbar);wrefresh(tabwin);wrefresh(lowbar);

	wbkgd(upbar, COLOR_PAIR(1));
	wbkgd(tabwin, COLOR_PAIR(2));
	wbkgd(lowbar, COLOR_PAIR(1));
	wrefresh(upbar); wrefresh(tabwin); wrefresh(lowbar);

	mvwaddstr(upbar, 0, std_x/2-2, "EFTI");
	char uptime_buff[21] = {0};
	uptime(uptime_buff);
	struct TabList tl;
	tab_init(&tl);
	load_settings(&tl);
	add_tab(tabwin, &tl);
	mvwaddstr(lowbar, 0, 2, "Uptime: ");
	waddstr(lowbar, uptime_buff);
	mvwaddstr(lowbar, 0, std_x-24, "Server status: ");
	if (SERVER_STATUS) {waddstr(lowbar, "Online");}
	else {waddstr(lowbar, "Offline");}
	wrefresh(upbar); wrefresh(tabwin); wrefresh(lowbar);

	WINDOW* main = newwin(std_y-4, std_x-4, 2, 2);
	int main_y, main_x; getmaxyx(main, main_y, main_x);
	WINDOW* wfiles = newwin(main_y-2, 50, 3, 4); keypad(wfiles, 1);
	wbkgd(main, COLOR_PAIR(3));
	wbkgd(wfiles, COLOR_PAIR(3));
	wrefresh(stdscr); wrefresh(main); wrefresh(wfiles);

	char *pwd = getcwd(NULL, 0);
	pwd = realloc(pwd, strlen(pwd)+2);handleMemError(pwd, "realloc(2) on main");
	strcat(pwd, "/");

	struct Binding bind;
	int keys[15] = {'x','v','u','h','M','r','s','m','c','D','n','N', 'X', 'C', 9};
	int (*binfunc[15])(struct TabList*, struct Data*, char *) = {execute, view, updir, hideDot, popup_menu, fileRename,
						fselect, fmove, fcopy, fdelete, fnew, dnew, execwargs, client_connect, b_tab_switch};
	bind.keys = keys; bind.func = binfunc; bind.nmemb=15;

	struct Data data; struct Fopt fdata; fdata.dotfiles=tl.settings.hideDot; data.data=&fdata;
	WINDOW* wins[6] = {stdscr, upbar, tabwin, lowbar, main, wfiles};
	data.wins=wins; data.wins_size = 6;

	struct Wobj *wobj = get_current_tab(&tl);handleMemError(wobj, "malloc(2) on main");
	wobj->data=&data;
	wobj->bind=bind;
	wobj->local=1;
	wobj->win=wfiles;
	wobj->pwd=pwd;
	wobj->ls = NULL;
	tl.tmp_path.path=NULL;

	for (;;) {
		struct Wobj *wobj = get_current_tab(&tl);
		struct Fopt* fdata = (struct Fopt*)wobj->data->data;
		wmove(main,0,0);wclrtoeol(main);
		wattron(main, COLOR_PAIR(4));
		int pwd_size = strlen(wobj->pwd);
		if (pwd_size+1 >= main_x) {
			char *a = wobj->pwd;
			a += pwd_size-(main_x-1);
			char *b = calloc(1+pwd_size+1, 1);
			strcpy(b, "<"); strcat(b, a);
			mvwaddstr(main, 0, 0, b);
			free(b);
		} else mvwaddstr(main, 0, 0, wobj->pwd);
		wattroff(main, COLOR_PAIR(4));
		wrefresh(main);

		struct TCallback cb;

		int size = list(&tl, fdata->dotfiles);
		if (size == -1) continue;

		int (*func[size])(struct TabList*, struct Data*,void*);void* args[size];
		char **ls = wobj->ls;
		for (int i=0;i<size;i++){
			func[i]=handleFile;
			args[i]=(void*)ls[i];
		}
		cb.func=func;cb.args=args;cb.nmemb=size;
		wobj->cb=cb;

		int res = menu(&tl, display_files);
		wobj = get_current_tab(&tl);
		if (wobj->ls != NULL) {
			/*free everything*/
			for (int i=0; i<wobj->cb.nmemb; i++) {
				free(wobj->ls[i]);
			}
			free(wobj->ls); wobj->ls=NULL;
		}
		if (res) {
			wmove(wfiles,0,0);wclrtobot(wfiles);
			wrefresh(stdscr);wrefresh(main);wrefresh(wfiles);
		} else break;
	}
	endwin();
	wobj=get_current_tab(&tl);
	free(wobj->pwd);
	free(tl.list);
}

int launch_create(struct TabList *tl, struct Data *data, void* d) {
	struct TabList *otl = (struct TabList*)d;
	WINDOW* stdscr = otl->wobj[0].data->wins[0];
	WINDOW* lowbar = otl->wobj[0].data->wins[3];
	server_create(otl);
	int y,x; getmaxyx(stdscr, y, x);
	wmove(lowbar, 0, x-9); wclrtoeol(lowbar);
	mvwaddstr(lowbar, 0, x-9, "Online");
	wrefresh(lowbar);
	return 1;
}

int launch_stop(struct TabList *tl, struct Data *data, void* d) {
	struct TabList *otl = (struct TabList*)d;
	WINDOW* stdscr = otl->wobj[0].data->wins[0];
	WINDOW* lowbar = otl->wobj[0].data->wins[3];
	server_kill();
	int y,x; getmaxyx(stdscr, y, x);
	wmove(lowbar, 0, x-9); wclrtoeol(lowbar);
	mvwaddstr(lowbar, 0, x-9, "Offline");
	wrefresh(lowbar);
	return 1;
}

int popup_menu(struct TabList *otl, struct Data *data, char* file) {
	char *tmp_path = TMP_PATH;
	char *tp1 = malloc(strlen(tmp_path)+9+1);
	strcpy(tp1, tmp_path); strcat(tp1, "/efti/pid");
	WINDOW* win = newwin(7, 16, 1, 0);
	WINDOW* mwin = newwin(5, 14, 2, 1);
	keypad(mwin,1);
	wbkgd(win, COLOR_PAIR(1));
	wbkgd(mwin, COLOR_PAIR(1));
	box(win, ACS_VLINE, ACS_HLINE);
	wrefresh(win);
	char* fname;
	int (*fn)(struct TabList*,struct Data*, void*);
	if (open(tp1, O_RDONLY) == -1) {fname="Start server"; fn=launch_create;}
	else {fname="Stop server";fn=launch_stop;}

	char* ls[4] = {fname, "Quick launcher", "Settings", "Close"};
	struct TCallback cb; struct Data _data; struct Binding bind;

	int (*func[4])(struct TabList*, struct Data*, void*) = {fn, NULL, settings, menu_close};
	void *args[4] = {otl, NULL, otl, NULL};
	cb.func=func;cb.args=args;cb.nmemb=4;

	bind.nmemb=0;

	struct Nopt nopt; nopt.underline=0; nopt.str_size=15;
	_data.data = &nopt; _data.wins_size=0;


	struct TabList tl;
	tab_init(&tl);
	struct Wobj *wobj = malloc(sizeof(struct Wobj));handleMemError(wobj, "malloc(2) on popup_menu");
	wobj->bind=bind; wobj->ls=ls; wobj->cb=cb; wobj->data=&_data;wobj->local=0;
	wobj->pwd=NULL;wobj->win=mwin;
	tl.wobj=wobj;
	for (;;) {
		int res = menu(&tl, display_opts);
		if (res) {
			if (open(tp1, O_RDONLY) == -1) {ls[0]="Start server";func[0]=launch_create;}
			else {ls[0]="Stop server";func[0]=launch_stop;}
			wmove(mwin,0,0);wclrtobot(mwin);
			touchwin(win);wrefresh(win);wrefresh(mwin);
		} else break;
	}

	delwin(win);
	for (int i=0; i<data->wins_size; i++) {
		touchwin(data->wins[i]);
		wrefresh(data->wins[i]);
	}
	free(tp1);
	free(wobj);
	return 1;
}
