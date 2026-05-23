#include "compiler.h"

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

struct ReadFileResult read_file(const char *path)
{
  struct ReadFileResult result;
  FILE *f;
  int seek_result;
  long offset;
  size_t bytes_read;
  char *buf;

  result.is_ok = true;
  result.msg = NULL;
  result.contents = NULL;

  f = fopen(path, "r");
  if (!f) {
    result.is_ok = false;
    result.msg = "fopen";
    goto end;
  }

  seek_result = fseek(f, 0L, SEEK_END);
  if (seek_result != 0) {
    result.is_ok = false;
    result.msg = "fseek";
    goto close_then_end;
  }

  offset = ftell(f);
  if (offset == -1) {
    result.is_ok = false;
    result.msg = "ftell";
    goto close_then_end;
  }

  rewind(f);

  buf = malloc(offset + 1);
  if (!buf) {
    result.is_ok = false;
    result.msg = "malloc";
    goto close_then_end;
  }

  bytes_read = fread(buf, 1, offset, f);
  if (bytes_read < (size_t) offset) {
    result.is_ok = false;
    if (ferror(f) != 0) {
      result.msg = "ferror";
    } else {
      if (feof(f) != 0) {
        result.msg = "feof";
      } else {
        result.msg = "unknown error during fread";
      }
      goto dealloc_then_close_then_end;
    }
  } else {
    buf[offset] = '\0';
    result.contents = buf;
    goto close_then_end;
  }

dealloc_then_close_then_end:
  free(buf);

close_then_end:
  fclose(f);

end:
  return result;
}

