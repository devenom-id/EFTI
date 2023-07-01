#include <stdio.h>
#include <ncurses.h>
#include "gears.h"

int SERVER_STATUS = 0;

int main() {
	WINDOW* stdscr = initscr();
	curs_set(0);
	start_color();
	init_pair(1, 15, 160); /*red_bg, white_fg*/
	init_pair(2, 15, 16); /*black_bg, white_fg*/
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

	getch();
	endwin();
}
