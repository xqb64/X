#include "util.h"

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int mktmp(void)
{
  static int i = 0;
  return i++;
}

char *mkstr(const char *fmt, ...)
{
  va_list args1, args2;
  int len;
  char *str;

  va_start(args1, fmt);
  va_copy(args2, args1);

  len = vsnprintf(NULL, 0, fmt, args1);
  va_end(args1);

  if (len < 0) {
    va_end(args2);
    return NULL;
  }

  str = malloc(len + 1);
  if (!str) {
    va_end(args2);
    return NULL;
  }

  vsnprintf(str, len + 1, fmt, args2);
  va_end(args2);

  return str;
}

char *mkuniq(char *s)
{
  return mkstr("var.%s.%d", s, mktmp());
}

char *mklbl(char *s, int d)
{
  return mkstr("%s.%d", s, d);
}

void print_indent(int spaces)
{
  assert(spaces >= 0 && "print_indent called with a negative number");

  /* `printf` normally looks like `printf("%4s", str)`, which tells it
   * to right-align with a minimum width of 4 spaces.  (Negative number
   * is left-align.)
   *
   * By using the star, we are telling it we will pass the number as argument.
   *
   * Since the padding defualt to space, we pass an empty string to print.  */
  printf("%*s", spaces, "");
}
