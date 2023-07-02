#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/sysinfo.h>

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

char** list(char *path) {
	char **ls = NULL;
	int size = 0;
	struct dirent *dt;
	DIR *dir = opendir(path);
	while ((dt=readdir(dir)) != NULL) {
		ls = realloc(ls, sizeof(char*)*(size+1));
		ls[size] = dt->d_name;
		size++;
	}
	ls = realloc(ls, sizeof(char*)*(size+1));
	ls[size] = NULL;
	return ls;
}
