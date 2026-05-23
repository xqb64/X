#ifndef MINI_COMPILER_UTIL_H
#define MINI_COMPILER_UTIL_H

#include "common.h"

/* util */

int mktmp(void);
char *mkstr(const char *fmt, ...);
char *mkuniq(char *s);
char *mklbl(char *s, int d);
void print_indent(int spaces);
struct ReadFileResult read_file(const char *path);

#endif /* MINI_COMPILER_UTIL_H */
