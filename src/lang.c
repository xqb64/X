#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

struct ReadFileResult {
  bool is_ok;
  char *msg;
  char *contents;
};

struct ReadFileResult read_file(const char *path)
{
  struct ReadFileResult result;
  FILE *f;
  int seek_result;
  size_t bytes_read;
  long offset;
  char *buf;

  result =
      (struct ReadFileResult){.is_ok = true, .msg = NULL, .contents = NULL};

  f = fopen(path, "r");
  if (!f) {
    perror("fopen");
    result.is_ok = false;
    result.msg = "fopen";
    goto end;
  }

  seek_result = fseek(f, 0L, SEEK_END);
  if (seek_result != 0) {
    perror("fseek");
    result.is_ok = false;
    result.msg = "fseek";
    goto end;
  }

  offset = ftell(f);
  if (offset == -1) {
    perror("ftell");
    result.is_ok = false;
    result.msg = "ftell";
    goto end;
  }

  rewind(f);

  buf = malloc(offset + 1);
  if (!buf) {
    perror("malloc");
    result.is_ok = false;
    result.msg = "malloc";
    goto end;
  }

  /* Let's ignore the return value of fread for now.  */
  bytes_read = fread(buf, offset, 1L, f);

  buf[offset] = '\0';

  result.contents = buf;

  fclose(f);

end:
  return result;
}

int main(void)
{
  const char *path;
  struct ReadFileResult read_file_result;

  path = "spam.x";

  read_file_result = read_file(path);
  if (!read_file_result.is_ok) {
    fprintf(stderr, "Couldn't read file.\n");
    goto end;
  }

  printf("%s", read_file_result.contents);

end:
  free(read_file_result.contents);
  return 0;
}
