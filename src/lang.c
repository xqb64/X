#include <stdio.h>
#include <stdlib.h>

char *read_file(const char *path)
{
  FILE *f;
  size_t bytes_read;
  long len;
  char *buf;

  f = fopen(path, "r");
  if (!f) {
    perror("fopen");
    exit(1);
  }

  fseek(f, 0L, SEEK_END);
  len = ftell(f);
  rewind(f);

  buf = malloc(len + 1);

  bytes_read = fread(buf, len, 1L, f);

  buf[len] = '\0';

  fclose(f);

  return buf;
}

int main(void)
{
  const char *path;
  char *contents;

  path = "spam.x";

  contents = read_file(path);
  printf("%s", contents);

  return 0;
}
