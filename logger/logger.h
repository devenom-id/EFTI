#ifndef LOGGER_H
#define LOGGER_H
enum log_type {
	CHAR,
	INT,
	DOUBLE,
	STR
};
void slog(const char* str, const char* file, int line);
void vlog(void* var, char *vname, enum log_type type, const char* file, int line);
void vsrlog(char* var, int size, char *vname, const char* file, int line);
#endif
