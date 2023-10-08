#ifndef GEARS_H
#define GEARS_H
#ifndef TERMUX
#define TERMUX 0
#endif
#ifndef TMP_PATH
#define TMP_PATH "/tmp"
#endif
#define TRANSF_LIMIT 400000000
#include <curses.h>
#include <ncurses.h>
struct Nopt {
	int str_size;
	int underline;
};
struct Fopt {
	int dotfiles;
};
struct Data {
	void* data;
	int* ptrs;
	WINDOW** wins;
	int wins_size;
};
struct TabList;
struct TCallback {
	int (**func)(struct TabList*, struct Data*, void*);
	void **args;
	int nmemb;
};
struct Callback {
	int (**func)(WINDOW* win, struct Data*, void*);
	void **args;
	int nmemb;
};
struct Binding {
	int *keys;
	int (**func)(struct TabList*, struct Data*, char*);
	int nmemb;
};
struct Wobj {
	WINDOW* win;
	struct Data *data;
	struct TCallback cb;
	struct Binding bind;
	int local;
	int fd;
	char* pwd;
	char** ls;
	char* attrls;
};
struct Tmp_path {
	char* path;
	int id;
};
struct Settings {
	int hideDot;
	int srv_local;
	const char* port;
	const char* defed;
	const char* defimg;
};
struct TabList {
	struct Tmp_path tmp_path;
	char *list;
	int point;
	int size;
	struct Wobj* wobj;
	struct Settings settings;
};
struct ObjText {
	int yx[2];
	const char* text;
};
struct ObjField {
	int yx[2];
	int size;
};
struct ObjCheck {
	int yx[2];
	int checked;
	const char* text;
};
struct ObjRect {
	int coords[4];
};
union Objtype {
	struct ObjText text;
	struct ObjField field;
	struct ObjCheck checkbox;
	struct ObjRect rectangle;
};
struct Mobj {
	int id;
	union Objtype object;
};
enum MobjType {T_TEXT,T_FIELD,T_CHECKBOX,T_RECTANGLE};
int popup_menu(struct TabList *otl, struct Data *data, char* file);
void handleMemError(void* m, const char* str);
void handleDisconnection(struct TabList* tl);
void dialog(WINDOW** wins, const char* s);
void create_dir_if_not_exist(const char* path);
char *itodg(int dig);
int enumdig(int n);
void uptime(char* buff);
void dir_up(struct TabList* tl, char **pwd);
void dir_cd(struct TabList* tl, char **pwd, char *dir);
int list(struct TabList *tl, int dotfiles);
void alph_sort(struct Wobj* wobj, int size);
void pr_ls(char **ls, int size);
void display_opts(struct TabList *tl, int start, int top, int* ptrs, int mode);
int W_ISDIR(struct Wobj *wobj, int index, const char* f);
int W_ISEXEC(struct Wobj *wobj, int index, const char* f);
void display_files(struct TabList *tl, int start, int top, int* ptrs, int mode);
int menu(struct TabList *tl, void (*dcb)(struct TabList*,int,int,int*,int));
int handleFile(struct TabList *tl, struct Data *data, void* f);
int execute(struct TabList *tl, struct Data *data, char* file);
int execwargs(struct TabList *tl, struct Data *data, char* file);
char* getExtension(char* file);
void high_SendOrder(int fd, int order, int dig, size_t size, char* param);
struct Srvdata high_GetFileData(struct TabList* tl, int fd, char* path);
void increase_max_tmp(int tmp);
char* high_GetTempFile(char *file);
int view(struct TabList *tl, struct Data *data, char *file);
int updir(struct TabList *tl, struct Data *data, char *file);
int hideDot(struct TabList *tl, struct Data *data, char* file);
int menu_close(struct TabList *tl, struct Data *data, void* args);
int fileRename(struct TabList *tl, struct Data *data, char* file);
int fselect(struct TabList *tl, struct Data *data, char* file);
int fmove(struct TabList *tl, struct Data *data, char* file);
void copy(struct Wobj* wobj, char *A, char *B);
int fcopy(struct TabList *tl, struct Data *data, char* file);
int fdelete(struct TabList *tl, struct Data *data, char* file);
int fnew(struct TabList *tl, struct Data *data, char* file);
int dnew(struct TabList *tl, struct Data *data, char* file);
void tab_init(struct TabList *tl);
void add_tab(WINDOW* tabwin, struct TabList *tl);
void del_tab(WINDOW* tabwin, struct TabList *tl);
void tab_switch(WINDOW* tabwin, struct TabList *tl);
int b_tab_switch(struct TabList *tl, struct Data* data, char* file);
struct Wobj* get_current_tab(struct TabList *tl);
struct Mobj NewText(int y, int x, const char* text); 
struct Mobj NewField(int y, int x, int size);
struct Mobj NewCheck(int y, int x, int checked, const char* text);
struct Mobj NewRect(int y1, int x1, int y2, int x2);
void print_mobj(WINDOW* win, int color, struct Mobj mobj);
int navigate(WINDOW* win, int emph_color[2], struct Mobj *mobj, struct Callback cb);
void load_settings(struct TabList* tl);
int settings(struct TabList* tl, struct Data* data, void* d);
int about(struct TabList* tl, struct Data* data, void* d);
int help(struct TabList* tl, struct Data* data, char* d);
#endif
