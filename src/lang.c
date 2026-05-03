#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
  long offset;
  size_t bytes_read;
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
    goto close_then_end;
  }

  offset = ftell(f);
  if (offset == -1) {
    perror("ftell");
    result.is_ok = false;
    result.msg = "ftell";
    goto close_then_end;
  }

  rewind(f);

  buf = malloc(offset + 1);
  if (!buf) {
    perror("malloc");
    result.is_ok = false;
    result.msg = "malloc";
    goto close_then_end;
  }

  bytes_read = fread(buf, 1, offset, f);
  if (bytes_read < (size_t) offset) {
    result.is_ok = false;
    if (ferror(f) != 0) {
      perror("fread");
      result.msg = "ferror";
      goto dealloc_then_close_then_end;
    } else {
      if (feof(f) != 0) {
        result.msg = "feof";
        goto dealloc_then_close_then_end;
      }
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

enum TokenKind {
  TOKEN_FN,
  TOKEN_IDENTIFIER,
  TOKEN_LPAREN,
  TOKEN_VOID,
  TOKEN_RPAREN,
  TOKEN_LBRACE,
  TOKEN_RETURN,
  TOKEN_NUMBER,
  TOKEN_PLUS,
  TOKEN_STAR,
  TOKEN_SEMICOLON,
  TOKEN_RBRACE,
  TOKEN_ERROR,
};

struct Token {
  enum TokenKind kind;
  char *start;
  int len;
};

struct Tokenizer {
  char *src;
};

void init_tokenizer(struct Tokenizer *tokenizer, char *src)
{
  tokenizer->src = src;
}

bool is_alpha(char c)
{
  return ((c >= 'a') && (c <= 'z') || (c >= 'A') && (c <= 'Z'));
}

bool is_digit(char c)
{
  return c >= '0' && c <= '9';
}

bool is_space(char c)
{
  return c == ' ' || c == '\t' || c == '\n';
}

void advance(struct Tokenizer *tokenizer)
{
  tokenizer->src++;
}

#define Vector(T) \
  struct {        \
    int capacity; \
    int len;      \
    T *data;      \
  }

#define vec_insert(vec, item)                                           \
  if ((vec)->len >= (vec)->capacity) {                                  \
    (vec)->capacity = (vec)->capacity == 0 ? 2 : (vec)->capacity * 2;   \
    (vec)->data =                                                       \
        realloc((vec)->data, sizeof((vec)->data[0]) * (vec)->capacity); \
  }                                                                     \
  (vec)->data[(vec)->len++] = (item);

#define vec_free(vec) free((vec)->data);

typedef Vector(struct Token) VecToken;

struct TokenizeResult {
  bool is_ok;
  char *msg;
  VecToken tokens;
};

bool is_at_end(struct Tokenizer *tokenizer)
{
  return *tokenizer->src == '\0';
}

int lookahead(struct Tokenizer *tokenizer, int n, char *target)
{
  return memcmp(tokenizer->src + 1, target, n);
}

struct Token mktoken(struct Tokenizer *tokenizer, enum TokenKind kind, int len)
{
  struct Token token = {
      .kind = kind, .start = tokenizer->src - len, .len = len};
  tokenizer->src += len;
  return token;
}

struct Token number(struct Tokenizer *tokenizer)
{
  int len;
  char *start;

  len = 0;
  start = tokenizer->src;

  while (is_digit(*tokenizer->src)) {
    len++;
    advance(tokenizer);
  }

  return (struct Token){.kind = TOKEN_NUMBER, .len = len, .start = start};
}

struct Token identifier(struct Tokenizer *tokenizer)
{
  int len;
  char *start;

  len = 0;
  start = tokenizer->src;
  while (is_alpha(*tokenizer->src) || is_digit(*tokenizer->src)) {
    len++;
    advance(tokenizer);
  }

  return (struct Token){.kind = TOKEN_IDENTIFIER, .len = len, .start = start};
}

struct TokenizeResult tokenize(struct Tokenizer *tokenizer)
{
  struct TokenizeResult result = {.tokens = {0}, .is_ok = true, .msg = NULL};
  VecToken tokens = {0};

  for (;;) {
    while (is_space(*tokenizer->src)) {
      advance(tokenizer);
    }

    if (is_at_end(tokenizer)) {
      break;
    }

    if (is_digit(*tokenizer->src)) {
      vec_insert(&tokens, number(tokenizer));
      continue;
    }

    switch (*tokenizer->src) {
      case 'f': {
        if (lookahead(tokenizer, 1, "n") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_FN, 2));
        } else {
          vec_insert(&tokens, identifier(tokenizer));
        }

        break;
      }
      case 'r': {
        if (lookahead(tokenizer, 5, "eturn") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_RETURN, 6));
        } else {
          vec_insert(&tokens, identifier(tokenizer));
        }

        break;
      }
      case 'v': {
        if (lookahead(tokenizer, 3, "oid") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_VOID, 4));
        } else {
          vec_insert(&tokens, identifier(tokenizer));
        }

        break;
      }
      case '+': {
        vec_insert(&tokens, mktoken(tokenizer, TOKEN_PLUS, 1));
        break;
      }
      case '*': {
        vec_insert(&tokens, mktoken(tokenizer, TOKEN_STAR, 1));
        break;
      }
      case '(': {
        vec_insert(&tokens, mktoken(tokenizer, TOKEN_LPAREN, 1));
        break;
      }
      case ')': {
        vec_insert(&tokens, mktoken(tokenizer, TOKEN_RPAREN, 1));
        break;
      }
      case '{': {
        vec_insert(&tokens, mktoken(tokenizer, TOKEN_LBRACE, 1));
        break;
      }
      case '}': {
        vec_insert(&tokens, mktoken(tokenizer, TOKEN_RBRACE, 1));
        break;
      }
      case ';': {
        vec_insert(&tokens, mktoken(tokenizer, TOKEN_SEMICOLON, 1));
        break;
      }
      default:

        if (is_alpha(*tokenizer->src)) {
          vec_insert(&tokens, identifier(tokenizer));
        } else {
          return (struct TokenizeResult){.tokens = {0},
                                         .is_ok = false,
                                         .msg = "Encountered unexpected token"};
        }

        break;
    }
  }

  result.tokens = tokens;

  return result;
}

void print_token(struct Token *token)
{
  switch (token->kind) {
    case TOKEN_FN:
      printf("fn");
      break;
    case TOKEN_VOID:
      printf("void");
      break;
    case TOKEN_RETURN:
      printf("return");
      break;
    case TOKEN_IDENTIFIER:
      printf("ident(%.*s)", token->len, token->start);
      break;
    case TOKEN_NUMBER:
      printf("%.*s", token->len, token->start);
      break;
    case TOKEN_PLUS:
      printf("plus");
      break;
    case TOKEN_STAR:
      printf("star");
      break;
    case TOKEN_SEMICOLON:
      printf("semicolon");
      break;
    case TOKEN_LBRACE:
      printf("LBrace");
      break;
    case TOKEN_RBRACE:
      printf("RBrace");
      break;
    case TOKEN_LPAREN:
      printf("LParen");
      break;
    case TOKEN_RPAREN:
      printf("RParen");
      break;
    case TOKEN_ERROR:
      printf("ERROR");
      break;
    default:
      assert(0);
  }

  printf("\n");
}

void print_tokens(VecToken tokens)
{
  for (int i = 0; i < tokens.len; i++) {
    print_token(&tokens.data[i]);
  }
}

int main(void)
{
  const char *path;
  struct ReadFileResult read_file_result;
  char *src;
  struct Tokenizer tokenizer;
  struct TokenizeResult tokenize_result;
  VecToken tokens;

  path = "spam.x";

  read_file_result = read_file(path);
  if (!read_file_result.is_ok) {
    fprintf(stderr, "Couldn't read file.\n");
    goto free_up2_fread;
  }

  src = read_file_result.contents;

  init_tokenizer(&tokenizer, src);
  tokenize_result = tokenize(&tokenizer);

  if (!tokenize_result.is_ok) {
    fprintf(stderr, "err: %s\n", tokenize_result.msg);
    goto free_up2_tokenize;
  }

  tokens = tokenize_result.tokens;
  print_tokens(tokens);

free_up2_tokenize:
  vec_free(&tokenize_result.tokens);

free_up2_fread:
  free(read_file_result.contents);
  return 0;
}
