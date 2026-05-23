#ifndef X_UTIL_H
#define X_UTIL_H

#define ALLOC(obj) (memcpy(malloc(sizeof((obj))), &(obj), sizeof(obj)))

int mktmp(void);
char *mkstr(const char *fmt, ...);
char *mkuniq(char *s);
char *mklbl(char *s, int d);
char *replace_ext(char *path, char *ext);
char *strip_ext(char *path);
void print_indent(int spaces);

#endif
