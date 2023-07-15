#ifndef GEARS_H
#define GEARS_H
#include <ncurses.h>
struct Data {
	void* data;
	int dotfiles;
	int ptrs[2];
	WINDOW** wins;
	int wins_size;
};
struct Callback {
	int (**func)(struct Data*, void*);
	void **args;
	int nmemb;
};
struct Binding {
	int *keys;
	int (**func)(struct Data*, char*);
	int nmemb;
};
void uptime(char* buff);
void dir_up(char *pwd);
void dir_cd(char *pwd, char *dir);
int list(char *path, char*** ls, int dotfiles);
void alph_sort(char** ls, int size);
void pr_ls(char **ls, int size);
void display_opts(WINDOW* win, char **ls, int size, int start, int top, int* ptrs, void* data, int mode);
void display_files(WINDOW* win, char **ls, int size, int start, int top, int* ptrs, void* data, int mode);
int menu(WINDOW* win, char** ls, void (*dcb)(WINDOW*,char**,int,int,int,int*,void*,int), struct Callback cb, struct Data *data, struct Binding bind);
int handleFile(struct Data *data, void* f);
int execute(struct Data *data, char* file);
int view(struct Data *data, char *file);
int updir(struct Data *data, char *file);
int hideDot(struct Data *data, char* file);
#endif
