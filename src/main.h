#ifndef MINI_COMPILER_MAIN_H
#define MINI_COMPILER_MAIN_H

#include "common.h"

struct CompilerOptions parse_args(int argc, char **argv);
char *replace_ext(char *path, char *ext);
char *strip_ext(char *path);
struct RunResult run(struct CompilerOptions *opts);
int main(int argc, char **argv);

#endif /* MINI_COMPILER_MAIN_H */
