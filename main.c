#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ncurses.h>
#include <dirent.h>
#include <locale.h>
#include "gears.h"

int SERVER_STATUS = 0;
int popup_menu(struct Data *data, char* file);

int main() {
	setlocale(LC_ALL, "");
	WINDOW* stdscr = initscr();
	curs_set(0);
	noecho();
	start_color();
	init_pair(1, 15, 160); /*red bg, white fg*/
	init_pair(2, 15, 16); /*black bg, white fg*/
	init_pair(3, 15, 237); /*grey bg, white fg*/
	init_pair(4, 207, 237); /*grey bg, pink fg*/
	init_pair(5, 16, 15); /*white bg, black fg*/

	init_pair(6, 15, 237); /*dark grey bg, white fg*/
	init_pair(7, 15, 240); /*light grey bg, white fg*/
	wbkgd(stdscr, COLOR_PAIR(2));

	int std_y, std_x; getmaxyx(stdscr, std_y, std_x);
	WINDOW* upbar = newwin(1,std_x,0,0);
	WINDOW* tabwin = newwin(1, std_x-4, std_y-2, 2);
	WINDOW* lowbar = newwin(1, std_x, std_y-1, 0);

	refresh();wrefresh(upbar);wrefresh(tabwin);wrefresh(lowbar);

	wbkgd(upbar, COLOR_PAIR(1));
	/*wbkgd(tabwin, COLOR_PAIR(6));*/
	wbkgd(lowbar, COLOR_PAIR(1));
	wrefresh(upbar); wrefresh(tabwin); wrefresh(lowbar);

	mvwaddstr(upbar, 0, std_x/2-2, "EFTI");
	char uptime_buff[21] = {0};
	uptime(uptime_buff);
	struct TabList tl;
	tab_init(&tl);
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
	pwd = realloc(pwd, strlen(pwd)+2);
	strcat(pwd, "/");

	/* PASS char*pwd somehow to menu. Menu should use dir_up and dir_cd,
	 * not chdir.
	 * Remove the log FILE object.
	 * Solve dirs not chargin completely. I suspect it has something to
	 * do with the "if (size<top)" part.*/

	struct Binding bind;
	int keys[12] = {'x','v','u','h','M','r','s','m','c','D','n','N'};
	int (*binfunc[12])(struct Data*, char*);
	binfunc[0]=execute; binfunc[1]=view; binfunc[2]=updir;
	binfunc[3]=hideDot; binfunc[4]=popup_menu; binfunc[5]=fileRename;
	binfunc[6]=fselect; binfunc[7]=fmove; binfunc[8]=fcopy;
	binfunc[9]=fdelete; binfunc[10]=fnew; binfunc[11]=dnew;
	bind.keys = keys; bind.func = binfunc; bind.nmemb=12;

	struct Data data; struct Fopt fdata; fdata.dotfiles=0; fdata.tmp_path=NULL; data.data=&fdata;
	WINDOW* wins[5] = {stdscr, upbar, lowbar, main, wfiles};
	data.wins=wins; data.wins_size = 5;

	for (;;) {
		wmove(main,0,0);wclrtoeol(main);
		wattron(main, COLOR_PAIR(4));
		mvwaddstr(main, 0, 0, pwd);
		wattroff(main, COLOR_PAIR(4));
		wrefresh(main);

		struct Callback cb;
		fdata.pwd=pwd;

		char** ls = NULL;
		int size = list(pwd, &ls, fdata.dotfiles);
		alph_sort(ls, size);

		int (*func[size])(struct Data*,void*);void* args[size];
		for (int i=0;i<size;i++){
			func[i]=handleFile;
			args[i]=(void*)ls[i];
		}
		cb.func=func;cb.args=args;cb.nmemb=size;

		int res = menu(wfiles, ls, display_files, cb, &data, bind);
		if (res) {
			wmove(wfiles,0,0);wclrtobot(wfiles);
			wrefresh(stdscr);wrefresh(main);wrefresh(wfiles);
		} else break;
	}
	/*while(1){if(getch()==10)break;}*/
	endwin();
}

int popup_menu(struct Data *data, char* file) {
	WINDOW* win = newwin(7, 16, 1, 0);
	WINDOW* mwin = newwin(5, 14, 2, 1);
	keypad(mwin,1);
	wbkgd(win, COLOR_PAIR(1));
	wbkgd(mwin, COLOR_PAIR(1));
	box(win, ACS_VLINE, ACS_HLINE);
	wrefresh(win);

	//wgetch(win);
	char *ls[4] = {"Start server", "Quick launcher", "Settings", "Close"};
	struct Callback cb; struct Data _data; struct Binding bind;

	int (*func[4])(struct Data*, void*) = {NULL, NULL, NULL, menu_close};
	void *args[4] = {NULL, NULL, NULL, NULL};
	cb.func=func;cb.args=args;cb.nmemb=4;

	bind.nmemb=0;

	struct Nopt nopt; nopt.underline=0; nopt.str_size=15;
	_data.data = &nopt; _data.wins_size=0;
	for (;;) {
		int res = menu(mwin, ls, display_opts, cb, &_data, bind);
		if (res) {
			wmove(mwin,0,0);wclrtobot(mwin);
			wrefresh(win);wrefresh(mwin);
		} else break;
	}

	delwin(win);
	for (int i=0; i<data->wins_size; i++) {
		touchwin(data->wins[i]);
		wrefresh(data->wins[i]);
	}
	return 1;
}
