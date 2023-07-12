#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ncurses.h>
#include <dirent.h>
#include "gears.h"

int SERVER_STATUS = 0;

int main() {
	WINDOW* stdscr = initscr();
	curs_set(0);
	noecho();
	start_color();
	init_pair(1, 15, 160); /*red_bg, white_fg*/
	init_pair(2, 15, 16); /*black_bg, white_fg*/
	init_pair(3, 15, 237); /*grey_bg, white_fg*/
	init_pair(4, 207, 237); /*grey_bg, pink_fg*/
	wbkgd(stdscr, COLOR_PAIR(2));

	int std_y, std_x; getmaxyx(stdscr, std_y, std_x);
	WINDOW* upbar = newwin(1,std_x,0,0);
	WINDOW* lowbar = newwin(1, std_x, std_y-1, 0);

	refresh();wrefresh(upbar);wrefresh(lowbar);

	wbkgd(upbar, COLOR_PAIR(1));
	wbkgd(lowbar, COLOR_PAIR(1));
	wrefresh(upbar); wrefresh(lowbar);

	mvwaddstr(upbar, 0, std_x/2-2, "EFTI");
	char uptime_buff[21] = {0};
	uptime(uptime_buff);
	mvwaddstr(lowbar, 0, 2, "Uptime: ");
	waddstr(lowbar, uptime_buff);
	mvwaddstr(lowbar, 0, std_x-24, "Server status: ");
	if (SERVER_STATUS) {waddstr(lowbar, "Online");}
	else {waddstr(lowbar, "Offline");}
	wrefresh(upbar); wrefresh(lowbar);

	WINDOW* main = newwin(std_y-4, std_x-4, 2, 2);
	int main_y, main_x; getmaxyx(main, main_y, main_x);
	WINDOW* wfiles = newwin(main_y-2, 50, 3, 4); keypad(wfiles, 1);
	wbkgd(main, COLOR_PAIR(3));
	wbkgd(wfiles, COLOR_PAIR(3));
	wrefresh(stdscr); wrefresh(main); wrefresh(wfiles);

	char *pwd = getcwd(NULL, 0);
	pwd = realloc(pwd, strlen(pwd)+1);
	strcat(pwd, "/");

	/* PASS char*pwd somehow to menu. Menu should use dir_up and dir_cd,
	 * not chdir.
	 * Remove the log FILE object.
	 * Solve dirs not chargin completely. I suspect it has something to
	 * do with the "if (size<top)" part.*/

	struct Binding bind;
	int keys[9] = {'x','v','u','h','M','r','m','c','D'};
	int (*binfunc[9])(struct Data*, char*);
	binfunc[0]=execute; binfunc[1]=view; binfunc[2]=updir;
	binfunc[3]=hideDot;
	bind.keys = keys; bind.func = binfunc;

	struct Data data; data.dotfiles=0;

	for (;;) {
		wmove(main,0,0);wclrtoeol(main);
		/*char *pwd = getcwd(NULL, 0);*/
		wattron(main, COLOR_PAIR(4));
		mvwaddstr(main, 0, 0, pwd);
		wattroff(main, COLOR_PAIR(4));
		wrefresh(main);

		struct Callback cb;
		data.data=pwd;

		char** ls = NULL;
		int size = list(pwd, &ls, data.dotfiles);
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
