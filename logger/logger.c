#include <stdio.h>
#include <time.h>
#include "logger.h"

/* [23:12:00 - main.c:23] Hello world*/
/* [23:12:00 - main.c:23] (x &0xf0000) "Hello world" */

void slog(const char* str, const char* file, int line) {
	time_t T = time(0);
	struct tm* Tt = localtime(&T);
	char buff[9]={0};
	strftime(buff, 9, "%H:%M:%S", Tt);
	FILE* F = fopen("log", "a");
	fprintf(F, "[%s - %s:%d] %s\n", buff, file, line, str);
	fclose(F);
}
void vlog(void* var, char *vname, enum log_type type, const char* file, int line) {
	time_t T = time(0);
	struct tm* Tt = localtime(&T);
	char buff[9]={0};
	strftime(buff, 9, "%H:%M:%S", Tt);
	FILE* F = fopen("log", "a");
	fprintf(F, "[%s - %s:%d] (%s &%p) ", buff, file, line, vname, var);
	switch(type) {
		case CHAR:
			fprintf(F, "'%c'\n", *((char*)var));
			break;
		case INT:
			fprintf(F, "%d\n", *((int*)var));
			break;
		case DOUBLE:
			fprintf(F, "%f\n", *((double*)var));
			break;
		case STR:
			fprintf(F, "\"%s\"\n", ((char*)var));
			break;
	}
	fclose(F);
}
void vsrlog(char* var, int size, char *vname, const char* file, int line) {
	time_t T = time(0);
	struct tm* Tt = localtime(&T);
	char buff[9]={0};
	strftime(buff, 9, "%H:%M:%S", Tt);
	FILE* F = fopen("log", "a");
	fprintf(F, "[%s - %s:%d] (%s &%p) \"", buff, file, line, vname, var);
	fwrite(var, 1, size, F);
	fputs("\"\n", F);
	fclose(F);
}
