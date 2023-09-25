#include <stdio.h>
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include "ncread.h"
#include "vector.h"

struct string vslice(struct string v,int a, int b) {
	struct string s;
	string_init(&s);
        for (int i=a;i<b;i++){
		if (i >= v.size) break;
                string_addch(&s, v.str[i]);
        }
        return s;
}

char* listostr(struct string l) {
	char* s;
	if (!l.size) s=calloc(1,1);
	else s = malloc(l.size);
	for (int i=0;i<l.size;i++) {
		s[i] = l.str[i];
	}
	s[l.size] = 0;
	return s;
}

void clrbox(WINDOW* win, int y, int x, int minlim, int vislim) {
	for (int i=x;i<x+(vislim-minlim)+1;i++) {
		mvwaddch(win,y,i,32);
	}
}

void right(WINDOW* win, int y, int x, int* p, int* sp, int* minlim, int* vislim, struct string String, int mode) {
	if (*sp == *vislim) {
		(*vislim)++;(*minlim)++;
		clrbox(win,y,x,*minlim,*vislim);
		int acum1=0;
		struct string vsliced = vslice(String,*minlim,(*vislim)+1);
		for (int i=0;i<vsliced.size;i++) {
			if (mode) mvwaddch(win,y,x+acum1,'*');
			else mvwaddch(win,y,x+acum1,vsliced.str[i]);
			acum1++;
		}
		wrefresh(win);
		(*sp)++;return;
	}
	(*p)++;
	(*sp)++;

}
void left(WINDOW* win, int y, int x, int* p, int* sp, int* minlim, int* vislim, struct string String, int mode) {
	if (!(*p) && *sp) {
		(*vislim)--;(*minlim)--;
		clrbox(win,y,x,*minlim,*vislim);
		int acum1 = 0;
		struct string vsliced = vslice(String,*minlim,(*vislim)+1);
		for (int i=0;i<vsliced.size;i++) {
			if (mode) mvwaddch(win,y,x+acum1,'*');
			else mvwaddch(win,y,x+acum1,vsliced.str[i]);
			acum1++;
		}
		wrefresh(win);
		(*sp)--;return;
	}
	(*p)--;
	(*sp)--;
}
void _erase(WINDOW* win, int y, int x, int* p, int* sp, int* minlim, int* vislim, struct string* String, int mode) {
	string_popat(String, (*sp)-1);
	clrbox(win,y,x,*minlim,*vislim);
	if (!p && sp) {(*vislim)--;(*minlim)--;}
	int acum1 = 0;
	struct string vsliced = vslice(*String,*minlim,(*vislim)+1);
	for (int i=0;i<vsliced.size;i++) {
		if (mode) mvwaddch(win,y,x+acum1,'*');
		else mvwaddch(win,y,x+acum1,vsliced.str[i]);
		acum1++;
	}
	wrefresh(win);
	if (!(!(*p) && *sp)) (*p)--;
	(*sp)--;
}
void end(WINDOW* win, int y, int x, int* p, int* sp, int* minlim, int* vislim, int ovislim, struct string String, int mode) {
	int cpyvslm = ovislim;
	if (String.size > ovislim) {*vislim=String.size;}
	else if (String.size < ovislim) {cpyvslm=String.size;}
	*minlim = String.size-cpyvslm;
	*p = cpyvslm;
	*sp = String.size;
	clrbox(win, y, x, *minlim, *vislim);
	int acum1 = 0;
	struct string vsliced = vslice(String,*minlim,(*vislim)+1);
	for (int i=0;i<vsliced.size;i++) {
		if (mode) mvwaddch(win,y,x+acum1,'*');
		else mvwaddch(win,y,x+acum1,vsliced.str[i]);
		acum1++;
	}
	wrefresh(win);
	return;
}

int ampsread(WINDOW* win, char** ptr, int y, int x, int vislim, int chlim, int mode, int curs_off) {
	curs_set(1);
	struct string String;
	struct string vsliced;
	string_init(&String); string_init(&vsliced);
	int p = 0;
	int sp = 0;
	int minlim = 0;
	int ovislim = vislim;
	noecho();
	clrbox(win,y,x,minlim,vislim);
	if (*ptr && strlen(*ptr)) {
		string_add(&String, *ptr);
		end(win, y, x, &p, &sp, &minlim, &vislim, ovislim, String, mode);
	}
	wrefresh(win);

	while (1) {
		int ch = mvwgetch(win,y,x+p);
		if (ch == 127) ch = KEY_BACKSPACE;
		else if (ch == 8) ch = KEY_BACKSPACE;
		if (ch == 4) {}
		else if (ch == 27) {if (curs_off) curs_set(0); return 1;}
		else if (ch == '\n') {*ptr = String.str;if (curs_off) curs_set(0);return 0;}
		else if (ch == KEY_BACKSPACE) {
			if (!sp) continue;
			_erase(win, y, x, &p, &sp, &minlim, &vislim, &String, mode);
			continue;
		}
		else if (ch == KEY_RIGHT) {
			if (sp == String.size) continue;
			right(win, y, x, &p, &sp, &minlim, &vislim, String, mode);
			continue;
		}
		else if (ch == KEY_LEFT) {
			if (!sp) continue;
			left(win, y, x, &p, &sp, &minlim, &vislim, String, mode);
			continue;
		}
		if (String.size == chlim) continue;
		if (ch < 32 || ch > 250) continue;
		string_addchat(&String, ch, sp);
		clrbox(win,y,x,minlim,vislim);
		if (sp == vislim) {
			vislim++;minlim++;
			int acum1=0;
			vsliced = vslice(String,minlim,vislim+1);
			for (int i=0;i<vsliced.size;i++) {
				if (mode) mvwaddch(win,y,x+acum1,'*');
				else mvwaddch(win,y,x+acum1,vsliced.str[i]);
				acum1++;
			}
			sp++;continue;
		}
		int acum1=0;
		vsliced = vslice(String,minlim,vislim+1);
		for (int i=0;i<vsliced.size;i++) {
			if (mode) mvwaddch(win,y,x+acum1,'*');
			else mvwaddch(win,y,x+acum1,vsliced.str[i]);
			acum1++;
		}
		sp++;
		p++;
	}
}
