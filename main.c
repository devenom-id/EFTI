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
	WINDOW* wfiles = newwin(main_y-2, 50, 3, 2);
	wbkgd(main, COLOR_PAIR(3));
	wbkgd(wfiles, COLOR_PAIR(3));
	wrefresh(stdscr); wrefresh(main); wrefresh(wfiles);

	char *pwd = getcwd(NULL, 0);
	wattron(main, COLOR_PAIR(4));
	mvwaddstr(main, 0, 0, pwd);
	wattroff(main, COLOR_PAIR(4));
	wrefresh(main);

	char** ls = list(pwd);
	while (*ls != NULL) {
		if (!strcmp(*ls, ".")) {ls++;continue;}
		waddstr(wfiles, *ls);
		waddstr(wfiles, "\n");
		ls++;
	}
	wrefresh(wfiles);

	while(1){if(getch()==10)break;}
	endwin();
}
