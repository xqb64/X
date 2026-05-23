#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define ALLOC(obj) (memcpy(malloc(sizeof((obj))), &(obj), sizeof(obj)))

#define Vector(T) \
  struct {        \
    int capacity; \
    int len;      \
    T *data;      \
  }

#define vec_insert(vec, item)                                             \
  /* Capacity grows exponentially (doubling) when the vector is full.     \
   * This geometric growth strategy ensures an amortized O(1) time        \
   * complexity for insertions, avoiding the O(n) bottleneck that         \
   * would occur with linear reallocation. */                             \
  do {                                                                    \
    if ((vec)->len >= (vec)->capacity) {                                  \
      (vec)->capacity = (vec)->capacity == 0 ? 2 : (vec)->capacity * 2;   \
      (vec)->data =                                                       \
          realloc((vec)->data, sizeof((vec)->data[0]) * (vec)->capacity); \
    }                                                                     \
    (vec)->data[(vec)->len++] = (item);                                   \
  } while (0)

#define vec_free(vec) free((vec)->data);

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

static inline void print_indent(int spaces)
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

enum TokenKind {
  /* keywords */
  TOKEN_FN,
  TOKEN_LET,
  TOKEN_MUT,
  TOKEN_AS,
  TOKEN_IF,
  TOKEN_ELSE,
  TOKEN_WHILE,
  TOKEN_LOOP,
  TOKEN_BREAK,
  TOKEN_CONTINUE,
  TOKEN_GOTO,
  TOKEN_RET,
  TOKEN_EXTERN,
  TOKEN_VOID,
  TOKEN_SIZEOF,
  TOKEN_STRUCT,
  TOKEN_UNION,
  TOKEN_ENUM,
  TOKEN_BOOL,
  TOKEN_TRUE,
  TOKEN_FALSE,
  TOKEN_I8,
  TOKEN_I16,
  TOKEN_I32,
  TOKEN_I64,
  TOKEN_U8,
  TOKEN_U16,
  TOKEN_U32,
  TOKEN_U64,
  TOKEN_F32,
  TOKEN_F64,
  TOKEN_STR,

  /* one char */
  TOKEN_LPAREN,
  TOKEN_RPAREN,
  TOKEN_LBRACE,
  TOKEN_RBRACE,
  TOKEN_PLUS,
  TOKEN_MINUS,
  TOKEN_STAR,
  TOKEN_SLASH,
  TOKEN_COMMA,
  TOKEN_COLON,
  TOKEN_SEMICOLON,
  TOKEN_LESS,
  TOKEN_GREATER,
  TOKEN_EQUAL,
  TOKEN_BANG,
  TOKEN_PIPE,
  TOKEN_AMPERSAND,
  TOKEN_CARET,
  TOKEN_TILDE,
  TOKEN_DOT,

  /* two chars */
  TOKEN_LESS_EQUAL,
  TOKEN_GREATER_EQUAL,
  TOKEN_EQUAL_EQUAL,
  TOKEN_BANG_EQUAL,
  TOKEN_PIPE_PIPE,
  TOKEN_AMPERSAND_AMPERSAND,
  TOKEN_ARROW,
  TOKEN_PLUS_EQUAL,
  TOKEN_MINUS_EQUAL,
  TOKEN_STAR_EQUAL,
  TOKEN_SLASH_EQUAL,
  TOKEN_AMPERSAND_EQUAL,
  TOKEN_PIPE_EQUAL,
  TOKEN_CARET_EQUAL,
  TOKEN_LESS_LESS,
  TOKEN_GREATER_GREATER,

  /* three chars */
  TOKEN_LESS_LESS_EQUAL,
  TOKEN_GREATER_GREATER_EQUAL,
  TOKEN_ELLIPSIS,

  /* variable length */
  TOKEN_IDENTIFIER,
  TOKEN_NUMBER,
  TOKEN_FP_NUMBER,
  TOKEN_STRING,

  /* control */
  TOKEN_ERROR,
};

struct Token {
  enum TokenKind kind;
  char *start;
  int len;
};

typedef Vector(struct Token) VecToken;

struct Tokenizer {
  char *src;
};

void init_tokenizer(struct Tokenizer *tokenizer, char *src)
{
  tokenizer->src = src;
}

bool is_alpha(char c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool is_digit(char c)
{
  return c >= '0' && c <= '9';
}

bool is_space(char c)
{
  return c == ' ' || c == '\t' || c == '\n';
}

bool is_underscore(char c)
{
  return c == '_';
}

bool is_dot(char c)
{
  return c == '.';
}

bool is_at_end(struct Tokenizer *tokenizer)
{
  return *tokenizer->src == '\0';
}

void advance(struct Tokenizer *tokenizer)
{
  tokenizer->src++;
}

int lookahead(struct Tokenizer *tokenizer, int n, char *target)
{
  return memcmp(tokenizer->src + 1, target, n);
}

struct Token mktoken(struct Tokenizer *tokenizer, enum TokenKind kind, int len)
{
  struct Token token;

  token.kind = kind;
  token.start = tokenizer->src;
  token.len = len;

  tokenizer->src += len;

  return token;
}

struct Token number(struct Tokenizer *tokenizer)
{
  int len;
  char *start;
  bool is_float;

  len = 0;
  start = tokenizer->src;
  is_float = false;

  while (is_digit(*tokenizer->src)) {
    len++;
    advance(tokenizer);
  }

  if (is_dot(tokenizer->src[0]) && is_digit(tokenizer->src[1])) {
    is_float = true;

    len++;
    advance(tokenizer);

    while (is_digit(*tokenizer->src)) {
      len++;
      advance(tokenizer);
    }
  }

  return (struct Token){.kind = is_float ? TOKEN_FP_NUMBER : TOKEN_NUMBER,
                        .len = len,
                        .start = start};
}

struct Token identifier(struct Tokenizer *tokenizer)
{
  int len;
  char *start;

  len = 0;
  start = tokenizer->src;
  while (is_alpha(*tokenizer->src) || is_digit(*tokenizer->src) ||
         is_underscore(*tokenizer->src)) {
    len++;
    advance(tokenizer);
  }

  return (struct Token){.kind = TOKEN_IDENTIFIER, .len = len, .start = start};
}

struct Token string(struct Tokenizer *tokenizer)
{
  int len;
  char *start;

  advance(tokenizer);

  len = 0;
  start = tokenizer->src;

  while (*tokenizer->src != '"') {
    if (is_at_end(tokenizer)) {
      return (struct Token){.kind = TOKEN_ERROR, .len = 1, .start = start};
    }

    len++;
    advance(tokenizer);
  }

  advance(tokenizer);

  return (struct Token){.kind = TOKEN_STRING, .len = len, .start = start};
}

struct TokenizeResult {
  bool is_ok;
  char *msg;
  VecToken tokens;
};

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
      case 'a': {
        if (lookahead(tokenizer, 1, "s") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_AS, 2));
        } else {
          vec_insert(&tokens, identifier(tokenizer));
        }

        break;
      }
      case 'b': {
        if (lookahead(tokenizer, 3, "ool") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_BOOL, 4));
        } else if (lookahead(tokenizer, 4, "reak") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_BREAK, 5));
        } else {
          vec_insert(&tokens, identifier(tokenizer));
        }

        break;
      }
      case 'c': {
        if (lookahead(tokenizer, 7, "ontinue") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_CONTINUE, 8));
        } else {
          vec_insert(&tokens, identifier(tokenizer));
        }

        break;
      }
      case 'e': {
        if (lookahead(tokenizer, 3, "num") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_ENUM, 4));
        } else if (lookahead(tokenizer, 3, "lse") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_ELSE, 4));
        } else if (lookahead(tokenizer, 5, "xtern") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_EXTERN, 6));
        } else {
          vec_insert(&tokens, identifier(tokenizer));
        }

        break;
      }
      case 'f': {
        if (lookahead(tokenizer, 4, "alse") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_FALSE, 5));
        } else if (lookahead(tokenizer, 2, "32") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_F32, 3));
        } else if (lookahead(tokenizer, 2, "64") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_F64, 3));
        } else if (lookahead(tokenizer, 1, "n") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_FN, 2));
        } else {
          vec_insert(&tokens, identifier(tokenizer));
        }

        break;
      }
      case 'g': {
        if (lookahead(tokenizer, 3, "oto") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_GOTO, 4));
        } else {
          vec_insert(&tokens, identifier(tokenizer));
        }
        break;
      }
      case 'i': {
        if (lookahead(tokenizer, 1, "8") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_I8, 2));
        } else if (lookahead(tokenizer, 1, "f") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_IF, 2));
        } else if (lookahead(tokenizer, 2, "16") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_I16, 3));
        } else if (lookahead(tokenizer, 2, "32") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_I32, 3));
        } else if (lookahead(tokenizer, 2, "64") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_I64, 3));
        } else {
          vec_insert(&tokens, identifier(tokenizer));
        }

        break;
      }
      case 'l': {
        if (lookahead(tokenizer, 2, "et") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_LET, 3));
        } else if (lookahead(tokenizer, 3, "oop") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_LOOP, 4));
        } else {
          vec_insert(&tokens, identifier(tokenizer));
        }

        break;
      }
      case 'm': {
        if (lookahead(tokenizer, 2, "ut") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_MUT, 3));
        } else {
          vec_insert(&tokens, identifier(tokenizer));
        }
        break;
      }
      case 'r': {
        if (lookahead(tokenizer, 2, "et") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_RET, 3));
        } else {
          vec_insert(&tokens, identifier(tokenizer));
        }

        break;
      }
      case 's': {
        if (lookahead(tokenizer, 5, "izeof") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_SIZEOF, 6));
        } else if (lookahead(tokenizer, 5, "truct") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_STRUCT, 6));
        } else if (lookahead(tokenizer, 2, "tr") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_STR, 3));
        } else {
          vec_insert(&tokens, identifier(tokenizer));
        }

        break;
      }
      case 't': {
        if (lookahead(tokenizer, 3, "rue") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_TRUE, 4));
        } else {
          vec_insert(&tokens, identifier(tokenizer));
        }

        break;
      }
      case 'u': {
        if (lookahead(tokenizer, 4, "nion") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_UNION, 5));
        } else if (lookahead(tokenizer, 1, "8") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_U8, 2));
        } else if (lookahead(tokenizer, 2, "16") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_U16, 3));
        } else if (lookahead(tokenizer, 2, "32") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_U32, 3));
        } else if (lookahead(tokenizer, 2, "64") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_U64, 3));
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
      case 'w': {
        if (lookahead(tokenizer, 4, "hile") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_WHILE, 5));
        } else {
          vec_insert(&tokens, identifier(tokenizer));
        }

        break;
      }
      case '+': {
        if (lookahead(tokenizer, 1, "=") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_PLUS_EQUAL, 2));
        } else {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_PLUS, 1));
        }
        break;
      }
      case '-': {
        if (lookahead(tokenizer, 1, ">") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_ARROW, 2));
        } else if (lookahead(tokenizer, 1, "=") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_MINUS_EQUAL, 2));
        } else {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_MINUS, 1));
        }
        break;
      }
      case '*': {
        if (lookahead(tokenizer, 1, "=") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_STAR_EQUAL, 2));
        } else {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_STAR, 1));
        }
        break;
      }
      case '/': {
        if (lookahead(tokenizer, 1, "=") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_SLASH_EQUAL, 2));
        } else {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_SLASH, 1));
        }
        break;
      }
      case '.': {
        if (lookahead(tokenizer, 2, "..") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_ELLIPSIS, 3));
        } else {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_DOT, 1));
        }

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
      case '!': {
        if (lookahead(tokenizer, 1, "=") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_BANG_EQUAL, 2));
        } else {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_BANG, 1));
        }
        break;
      }
      case '=': {
        if (lookahead(tokenizer, 1, "=") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_EQUAL_EQUAL, 2));
        } else {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_EQUAL, 1));
        }
        break;
      }
      case '|': {
        if (lookahead(tokenizer, 1, "|") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_PIPE_PIPE, 2));
        } else if (lookahead(tokenizer, 1, "=") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_PIPE_EQUAL, 2));
        } else {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_PIPE, 1));
        }
        break;
      }
      case '&': {
        if (lookahead(tokenizer, 1, "&") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_AMPERSAND_AMPERSAND, 2));
        } else if (lookahead(tokenizer, 1, "=") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_AMPERSAND_EQUAL, 2));
        } else {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_AMPERSAND, 1));
        }
        break;
      }
      case '^': {
        if (lookahead(tokenizer, 1, "=") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_CARET_EQUAL, 2));
        } else {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_CARET, 1));
        }
        break;
      }
      case '~': {
        vec_insert(&tokens, mktoken(tokenizer, TOKEN_TILDE, 1));
        break;
      }
      case ',': {
        vec_insert(&tokens, mktoken(tokenizer, TOKEN_COMMA, 1));
        break;
      }
      case ':': {
        vec_insert(&tokens, mktoken(tokenizer, TOKEN_COLON, 1));
        break;
      }
      case ';': {
        vec_insert(&tokens, mktoken(tokenizer, TOKEN_SEMICOLON, 1));
        break;
      }
      case '"': {
        vec_insert(&tokens, string(tokenizer));
        break;
      }
      case '>': {
        if (lookahead(tokenizer, 2, ">=") == 0) {
          vec_insert(&tokens,
                     mktoken(tokenizer, TOKEN_GREATER_GREATER_EQUAL, 3));
        } else if (lookahead(tokenizer, 1, ">") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_GREATER_GREATER, 2));
        } else if (lookahead(tokenizer, 1, "=") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_GREATER_EQUAL, 2));
        } else {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_GREATER, 1));
        }
        break;
      }
      case '<': {
        if (lookahead(tokenizer, 2, "<=") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_LESS_LESS_EQUAL, 3));
        } else if (lookahead(tokenizer, 1, "<") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_LESS_LESS, 2));
        } else if (lookahead(tokenizer, 1, "=") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_LESS_EQUAL, 2));
        } else {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_LESS, 1));
        }
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
    case TOKEN_LET:
      printf("let");
      break;
    case TOKEN_MUT:
      printf("mut");
      break;
    case TOKEN_AS:
      printf("as");
      break;
    case TOKEN_IF:
      printf("if");
      break;
    case TOKEN_ELSE:
      printf("else");
      break;
    case TOKEN_WHILE:
      printf("while");
      break;
    case TOKEN_LOOP:
      printf("loop");
      break;
    case TOKEN_BREAK:
      printf("break");
      break;
    case TOKEN_CONTINUE:
      printf("continue");
      break;
    case TOKEN_GOTO:
      printf("goto");
      break;
    case TOKEN_RET:
      printf("ret");
      break;
    case TOKEN_EXTERN:
      printf("extern");
      break;
    case TOKEN_VOID:
      printf("void");
      break;
    case TOKEN_SIZEOF:
      printf("sizeof");
      break;
    case TOKEN_STRUCT:
      printf("struct");
      break;
    case TOKEN_UNION:
      printf("union");
      break;
    case TOKEN_ENUM:
      printf("enum");
      break;
    case TOKEN_BOOL:
      printf("bool");
      break;
    case TOKEN_TRUE:
      printf("true");
      break;
    case TOKEN_FALSE:
      printf("false");
      break;
    case TOKEN_I8:
      printf("i8");
      break;
    case TOKEN_I16:
      printf("i16");
      break;
    case TOKEN_I32:
      printf("i32");
      break;
    case TOKEN_I64:
      printf("i64");
      break;
    case TOKEN_U8:
      printf("u8");
      break;
    case TOKEN_U16:
      printf("u16");
      break;
    case TOKEN_U32:
      printf("u32");
      break;
    case TOKEN_U64:
      printf("u64");
      break;
    case TOKEN_F32:
      printf("f32");
      break;
    case TOKEN_F64:
      printf("f64");
      break;
    case TOKEN_STR:
      printf("str");
      break;
    case TOKEN_LPAREN:
      printf("LParen");
      break;
    case TOKEN_RPAREN:
      printf("RParen");
      break;
    case TOKEN_LBRACE:
      printf("LBrace");
      break;
    case TOKEN_RBRACE:
      printf("RBrace");
      break;
    case TOKEN_PLUS:
      printf("plus");
      break;
    case TOKEN_MINUS:
      printf("minus");
      break;
    case TOKEN_STAR:
      printf("star");
      break;
    case TOKEN_SLASH:
      printf("slash");
      break;
    case TOKEN_COMMA:
      printf("comma");
      break;
    case TOKEN_COLON:
      printf("colon");
      break;
    case TOKEN_SEMICOLON:
      printf("semicolon");
      break;
    case TOKEN_LESS:
      printf("less");
      break;
    case TOKEN_GREATER:
      printf("greater");
      break;
    case TOKEN_EQUAL:
      printf("equal");
      break;
    case TOKEN_BANG:
      printf("bang");
      break;
    case TOKEN_PIPE:
      printf("pipe");
      break;
    case TOKEN_AMPERSAND:
      printf("ampersand");
      break;
    case TOKEN_CARET:
      printf("caret");
      break;
    case TOKEN_TILDE:
      printf("tilde");
      break;
    case TOKEN_DOT:
      printf("dot");
      break;
    case TOKEN_LESS_EQUAL:
      printf("less equal");
      break;
    case TOKEN_GREATER_EQUAL:
      printf("greater equal");
      break;
    case TOKEN_EQUAL_EQUAL:
      printf("equal equal");
      break;
    case TOKEN_BANG_EQUAL:
      printf("bang equal");
      break;
    case TOKEN_PIPE_PIPE:
      printf("pipe pipe");
      break;
    case TOKEN_AMPERSAND_AMPERSAND:
      printf("ampersand ampersand");
      break;
    case TOKEN_ARROW:
      printf("arrow");
      break;
    case TOKEN_PLUS_EQUAL:
      printf("plus equal");
      break;
    case TOKEN_MINUS_EQUAL:
      printf("minus equal");
      break;
    case TOKEN_STAR_EQUAL:
      printf("star equal");
      break;
    case TOKEN_SLASH_EQUAL:
      printf("slash equal");
      break;
    case TOKEN_AMPERSAND_EQUAL:
      printf("ampersand equal");
      break;
    case TOKEN_PIPE_EQUAL:
      printf("pipe equal");
      break;
    case TOKEN_CARET_EQUAL:
      printf("caret equal");
      break;
    case TOKEN_LESS_LESS:
      printf("less less");
      break;
    case TOKEN_GREATER_GREATER:
      printf("greater greater");
      break;
    case TOKEN_LESS_LESS_EQUAL:
      printf("less less equal");
      break;
    case TOKEN_GREATER_GREATER_EQUAL:
      printf("greater greater equal");
      break;
    case TOKEN_ELLIPSIS:
      printf("ellipsis");
      break;
    case TOKEN_IDENTIFIER:
      printf("ident(%.*s)", token->len, token->start);
      break;
    case TOKEN_NUMBER:
    case TOKEN_FP_NUMBER:
      printf("%.*s", token->len, token->start);
      break;
    case TOKEN_STRING:
      printf("string(\"%.*s\")", token->len, token->start);
      break;
    case TOKEN_ERROR:
      printf("ERROR");
      break;
    default:
      assert(0 && "unhandled TokenKind variant");
  }

  printf("\n");
}

void print_tokens(VecToken *tokens)
{
  for (int i = 0; i < tokens->len; i++) {
    print_token(&tokens->data[i]);
  }
}

enum LiteralKind {
  LITERAL_NUM,
  LITERAL_BOOL,
  LITERAL_STR,
};

enum TypeKind {
  I8_T,
  I16_T,
  I32_T,
  I64_T,
  U8_T,
  U16_T,
  U32_T,
  U64_T,
  F32_T,
  F64_T,
  BOOL_T,
  STR_T,
  FN_T,
  VOID_T,
  PTR_T,
  STRUCT_T,
  UNKNOWN_T,
};

typedef struct Type Type;

typedef Vector(Type) VecType;

struct Type {
  enum TypeKind kind;
  union {
    struct {
      VecType params;
      Type *retval;
      bool is_variadic;
    } func;
    struct Type *base;
    char *struct_name;
  } as;
};

struct StructField {
  char *name;
  Type type;
  int offset;
};

typedef Vector(struct StructField) VecStructField;

struct StructDef {
  char *name;
  VecStructField fields;
  int size;
  int alignment;
  bool is_union;
};

struct StructTable {
  struct StructTable *next;
  struct StructDef def;
};

struct StructTable *struct_table = NULL;

void free_struct_table(struct StructTable *table)
{
  while (table) {
    struct StructTable *tmp = table;
    table = table->next;
    free(tmp);
  }
}

void struct_insert(struct StructTable **table, struct StructDef def)
{
  struct StructTable *node;

  node = malloc(sizeof(struct StructTable));
  node->def = def;
  node->def.name = strdup(def.name);
  node->next = *table;

  *table = node;
}

struct StructDef *struct_get(struct StructTable *table, char *name)
{
  while (table) {
    if (strcmp(table->def.name, name) == 0) {
      return &table->def;
    }
    table = table->next;
  }
  return NULL;
}

struct EnumTypeItem {
  char *name;
  struct EnumTypeItem *next;
};
struct EnumTypeItem *enum_types = NULL;

void free_enum_types(void)
{
  struct EnumTypeItem *curr_t = enum_types;
  while (curr_t) {
    struct EnumTypeItem *tmp = curr_t;
    curr_t = curr_t->next;
    free(tmp->name);
    free(tmp);
  }
  enum_types = NULL;
}

void enum_type_insert(char *name)
{
  struct EnumTypeItem *item;

  item = malloc(sizeof(*item));
  item->name = strdup(name);
  item->next = enum_types;
  enum_types = item;
}

bool is_enum_type(char *name)
{
  struct EnumTypeItem *curr;

  curr = enum_types;
  while (curr) {
    if (strcmp(curr->name, name) == 0) {
      return true;
    }
    curr = curr->next;
  }
  return false;
}

struct EnumVariantItem {
  char *name;
  int value;
  struct EnumVariantItem *next;
};
struct EnumVariantItem *enum_variants = NULL;

void free_enum_variants(void)
{
  struct EnumVariantItem *curr_v = enum_variants;
  while (curr_v) {
    struct EnumVariantItem *tmp = curr_v;
    curr_v = curr_v->next;
    free(tmp->name);
    free(tmp);
  }
  enum_variants = NULL;
}

void enum_variant_insert(char *name, int value)
{
  struct EnumVariantItem *item;

  item = malloc(sizeof(*item));
  item->name = strdup(name);
  item->value = value;
  item->next = enum_variants;
  enum_variants = item;
}

bool enum_variant_get(char *name, int *out_val)
{
  struct EnumVariantItem *curr;

  curr = enum_variants;
  while (curr) {
    if (strcmp(curr->name, name) == 0) {
      *out_val = curr->value;
      return true;
    }
    curr = curr->next;
  }
  return false;
}

Type clone_type(Type t)
{
  Type copy = t;
  if (t.kind == PTR_T) {
    copy.as.base = malloc(sizeof(Type));
    *copy.as.base = clone_type(*t.as.base);
  } else if (t.kind == STRUCT_T) {
    copy.as.struct_name = strdup(t.as.struct_name);
  }
  return copy;
}

void print_type(Type *type, int spaces);

void free_type(Type *t)
{
  switch (t->kind) {
    case U8_T:
    case U16_T:
    case U32_T:
    case U64_T:
    case I8_T:
    case I16_T:
    case I32_T:
    case I64_T:
    case F32_T:
    case F64_T:
    case STR_T:
    case BOOL_T:
    case VOID_T:
    case UNKNOWN_T:
      break;
    case PTR_T: {
      free_type(t->as.base);
      free(t->as.base);
      break;
    }
    case STRUCT_T: {
      free(t->as.struct_name);
      break;
    }
    case FN_T: {
      for (int i = 0; i < t->as.func.params.len; i++) {
        free_type(&t->as.func.params.data[i]);
      }
      vec_free(&t->as.func.params);

      if (t->as.func.retval) {
        free_type(t->as.func.retval);
        free(t->as.func.retval);
      }
      break;
    }
  }
}

struct Literal {
  enum LiteralKind kind;
  union {
    char *str;
    unsigned char u8;
    char i8;
    unsigned short u16;
    short i16;
    unsigned int u32;
    int i32;
    unsigned long long u64;
    long long i64;
    float f32;
    double f64;
    bool boolean;
  } as;
  Type type;
};

enum ExprKind {
  EXPR_LITERAL,
  EXPR_VARIABLE,
  EXPR_UNARY,
  EXPR_BINARY,
  EXPR_ASSIGN,
  EXPR_COMPOUND_ASSIGN,
  EXPR_CALL,
  EXPR_SIZEOF,
  EXPR_ADDROF,
  EXPR_DEREF,
  EXPR_CAST,
  EXPR_STRUCT_INIT,
  EXPR_MEMBER,
};

enum ExprBinKind {
  EXPR_BIN_ADD,
  EXPR_BIN_SUB,
  EXPR_BIN_MUL,
  EXPR_BIN_DIV,
  EXPR_BIN_LESS,
  EXPR_BIN_GREATER,
  EXPR_BIN_LESS_EQUAL,
  EXPR_BIN_GREATER_EQUAL,
  EXPR_BIN_EQUAL_EQUAL,
  EXPR_BIN_BANG_EQUAL,
  EXPR_BIN_BITWISE_AND,
  EXPR_BIN_BITWISE_XOR,
  EXPR_BIN_BITWISE_OR,
  EXPR_BIN_SHIFT_LEFT,
  EXPR_BIN_SHIFT_RIGHT,
  EXPR_BIN_LOGICAL_AND,
  EXPR_BIN_LOGICAL_OR,
};

void print_binary_op(enum ExprBinKind kind)
{
  switch (kind) {
    case EXPR_BIN_ADD:
      printf("ADD");
      break;
    case EXPR_BIN_SUB:
      printf("SUB");
      break;
    case EXPR_BIN_MUL:
      printf("MUL");
      break;
    case EXPR_BIN_DIV:
      printf("DIV");
      break;
    case EXPR_BIN_LESS:
      printf("LESS");
      break;
    case EXPR_BIN_GREATER:
      printf("GREATER");
      break;
    case EXPR_BIN_LESS_EQUAL:
      printf("LESS EQUAL");
      break;
    case EXPR_BIN_GREATER_EQUAL:
      printf("GREATER EQUAL");
      break;
    case EXPR_BIN_EQUAL_EQUAL:
      printf("EQUAL EQUAL");
      break;
    case EXPR_BIN_BANG_EQUAL:
      printf("BANG EQUAL");
      break;
    case EXPR_BIN_BITWISE_AND:
      printf("BITWISE AND");
      break;
    case EXPR_BIN_BITWISE_XOR:
      printf("BITWISE XOR");
      break;
    case EXPR_BIN_BITWISE_OR:
      printf("BITWISE OR");
      break;
    case EXPR_BIN_SHIFT_LEFT:
      printf("SHIFT LEFT");
      break;
    case EXPR_BIN_SHIFT_RIGHT:
      printf("SHIFT RIGHT");
      break;
    case EXPR_BIN_LOGICAL_AND:
      printf("LOGICAL AND");
      break;
    case EXPR_BIN_LOGICAL_OR:
      printf("LOGICAL OR");
      break;
    default:
      assert(0);
  }
}

struct ExprVar {
  char *name;
  Type type;
};

struct ExprUnary {
  char *op;
  struct Expr *expr;
};

struct ExprBin {
  enum ExprBinKind kind;
  struct Expr *lhs;
  struct Expr *rhs;
};

struct ExprAssign {
  struct Expr *lhs;
  struct Expr *rhs;
};

struct ExprCompoundAssign {
  enum ExprBinKind kind;
  struct Expr *lhs;
  struct Expr *rhs;
};

typedef Vector(struct Expr) VecExpr;

struct ExprCall {
  struct Expr *target;
  VecExpr arguments;
};

struct ExprDeref {
  struct Expr *expr;
};

struct ExprAddrOf {
  struct Expr *expr;
};

struct ExprSizeof {
  struct Expr *expr;
};

struct ExprCast {
  struct Expr *expr;
  Type target_type;
};

struct StructInitItem {
  char *designator;
  struct Expr *expr;
  int resolved_offset;
};
typedef Vector(struct StructInitItem) VecStructInitItem;

struct ExprStructInit {
  char *struct_name;
  VecStructInitItem values;
};

struct ExprMember {
  struct Expr *target;
  char *field_name;
  bool is_arrow;
};

struct ExprAs {
  struct Expr *target;
  Type target_type;
};

struct Expr {
  enum ExprKind kind;
  union {
    struct Literal literal;
    struct ExprBin binary;
    struct ExprVar var;
    struct ExprCall call;
    struct ExprAssign assign;
    struct ExprCompoundAssign compound_assign;
    struct ExprUnary unary;
    struct ExprAddrOf addrof;
    struct ExprSizeof sizeof_expr;
    struct ExprDeref deref;
    struct ExprCast cast;
    struct ExprStructInit struct_init;
    struct ExprMember member;
    struct ExprAs as;
  } as;
  Type type;
};

bool types_equal(Type a, Type b);

bool vectype_equal(VecType a, VecType b)
{
  if (a.len != b.len) {
    return false;
  }

  for (int i = 0; i < a.len; i++) {
    if (!types_equal(a.data[i], b.data[i])) {
      return false;
    }
  }

  return true;
}

bool types_equal(Type a, Type b)
{
  if (a.kind != b.kind) {
    return false;
  }

  switch (a.kind) {
    case STRUCT_T:
      return strcmp(a.as.struct_name, b.as.struct_name) == 0;
    case PTR_T:
      return types_equal(*a.as.base, *b.as.base);
    case I8_T:
    case I16_T:
    case I32_T:
    case I64_T:
    case U8_T:
    case U16_T:
    case U32_T:
    case U64_T:
    case F32_T:
    case F64_T:
    case STR_T:
    case BOOL_T:
    case UNKNOWN_T:
      return true;

    case FN_T: {
      if (a.as.func.retval == NULL || b.as.func.retval == NULL) {
        return a.as.func.retval == b.as.func.retval;
      }

      if (!types_equal(*a.as.func.retval, *b.as.func.retval)) {
        return false;
      }

      return vectype_equal(a.as.func.params, b.as.func.params);
    }

    default:
      return false;
  }
}

void print_expr(struct Expr *expr, int spaces)
{
  switch (expr->kind) {
    case EXPR_LITERAL: {
      if (expr->as.literal.kind == LITERAL_BOOL) {
        printf("Literal(\n");
        print_indent(spaces + 2);
        printf("v: %s,\n", expr->as.literal.as.boolean ? "true" : "false");
        printf("\n");
        print_indent(spaces);
        printf(")");
      } else if (expr->as.literal.kind == LITERAL_NUM) {
        printf("Literal(\n");
        print_indent(spaces + 2);

        switch (expr->as.literal.type.kind) {
          case I8_T: {
            printf("v: %d,\n", expr->as.literal.as.i8);
            break;
          }
          case U8_T: {
            printf("v: %d,\n", expr->as.literal.as.u8);
            break;
          }
          case I16_T: {
            printf("v: %d,\n", expr->as.literal.as.i16);
            break;
          }
          case U16_T: {
            printf("v: %d,\n", expr->as.literal.as.u16);
            break;
          }
          case I32_T: {
            printf("v: %d,\n", expr->as.literal.as.i32);
            break;
          }
          case U32_T: {
            printf("v: %d,\n", expr->as.literal.as.u32);
            break;
          }
          case I64_T: {
            printf("v: %lld,\n", expr->as.literal.as.i64);
            break;
          }
          case U64_T: {
            printf("v: %llu,\n", expr->as.literal.as.u64);
            break;
          }
          case F32_T: {
            printf("v: %f,\n", expr->as.literal.as.f32);
            break;
          }
          case F64_T: {
            printf("v: %f,\n", expr->as.literal.as.f64);
            break;
          }
          default:
            assert(0);
        }
        print_indent(spaces + 2);
        printf("type: ");
        print_type(&expr->as.literal.type, spaces);
        printf("\n");
        print_indent(spaces);
        printf(")");
      } else {
        printf("Literal(\"%s\")", expr->as.literal.as.str);
      }
      break;
    }
    case EXPR_VARIABLE: {
      printf("Variable(%s)", expr->as.var.name);
      break;
    }
    case EXPR_UNARY: {
      printf("Unary(\n");

      print_indent(spaces + 2);
      printf("expr = ");
      print_expr(expr->as.unary.expr, spaces + 4);
      printf(",\n");

      print_indent(spaces);
      printf(")");

      break;
    }
    case EXPR_BINARY: {
      printf("Binary(\n");

      print_indent(spaces + 2);
      printf("lhs = ");
      print_expr(expr->as.binary.lhs, spaces + 4);
      printf(",\n");

      print_indent(spaces + 2);
      printf("rhs = ");
      print_expr(expr->as.binary.rhs, spaces + 4);
      printf(",\n");

      print_indent(spaces + 2);
      printf("kind = ");
      print_binary_op(expr->as.binary.kind);
      printf("\n");

      print_indent(spaces);
      printf(")");
      break;
    }
    case EXPR_ASSIGN: {
      printf("Assign(\n");

      print_indent(spaces + 2);
      printf("lhs = ");
      print_expr(expr->as.assign.lhs, spaces + 4);
      printf(",\n");

      print_indent(spaces + 2);
      printf("rhs = ");
      print_expr(expr->as.assign.rhs, spaces + 4);
      printf(",\n");

      print_indent(spaces);
      printf(")");
      break;
    }
    case EXPR_COMPOUND_ASSIGN: {
      printf("CompoundAssign(\n");
      print_indent(spaces + 2);
      printf("op = ");
      print_binary_op(expr->as.compound_assign.kind);
      printf(",\n");
      print_indent(spaces + 2);
      printf("lhs = ");
      print_expr(expr->as.compound_assign.lhs, spaces + 4);
      printf(",\n");
      print_indent(spaces + 2);
      printf("rhs = ");
      print_expr(expr->as.compound_assign.rhs, spaces + 4);
      printf(",\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case EXPR_CALL: {
      printf("Call(\n");

      print_indent(spaces + 2);
      printf("target = ");
      print_expr(expr->as.call.target, spaces + 2);
      printf(",\n");

      print_indent(spaces + 2);
      printf("arguments: [\n");

      for (int i = 0; i < expr->as.call.arguments.len; i++) {
        print_indent(spaces + 4);
        print_expr(&expr->as.call.arguments.data[i], spaces + 4);
        printf(",\n");
      }

      print_indent(spaces + 2);
      printf("]\n");

      print_indent(spaces);
      printf(")");
      break;
    }
    case EXPR_SIZEOF: {
      printf("SizeOf(\n");
      print_indent(spaces + 2);
      print_expr(expr->as.sizeof_expr.expr, spaces + 2);
      printf("\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case EXPR_ADDROF: {
      printf("AddrOf(\n");
      print_indent(spaces + 2);
      print_expr(expr->as.addrof.expr, spaces + 2);
      printf("\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case EXPR_DEREF: {
      printf("Deref(\n");
      print_indent(spaces + 2);
      print_expr(expr->as.deref.expr, spaces + 2);
      printf("\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case EXPR_CAST: {
      printf("Cast(\n");
      print_indent(spaces + 2);
      print_expr(expr->as.cast.expr, spaces + 2);
      printf("\n");
      print_indent(spaces + 2);
      printf("target_type: ");
      print_type(&expr->as.cast.target_type, spaces + 2);
      printf("\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case EXPR_STRUCT_INIT: {
      printf("StructInit(\n");
      print_indent(spaces + 2);
      printf("name = %s,\n", expr->as.struct_init.struct_name);
      print_indent(spaces + 2);
      printf("values = [\n");
      for (int i = 0; i < expr->as.struct_init.values.len; i++) {
        print_indent(spaces + 4);
        print_expr(expr->as.struct_init.values.data[i].expr, spaces + 4);
        printf(",\n");
      }
      print_indent(spaces + 2);
      printf("]\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case EXPR_MEMBER: {
      printf("Member(\n");
      print_indent(spaces + 2);
      printf("target = ");
      print_expr(expr->as.member.target, spaces + 4);
      printf(",\n");
      print_indent(spaces + 2);
      printf("field = %s,\n", expr->as.member.field_name);
      print_indent(spaces + 2);
      printf("is_arrow = %s\n", expr->as.member.is_arrow ? "true" : "false");
      print_indent(spaces);
      printf(")");
      break;
    }
  }
}

void free_expr(struct Expr *expr)
{
  switch (expr->kind) {
    case EXPR_LITERAL: {
      switch (expr->as.literal.kind) {
        case LITERAL_NUM:
        case LITERAL_BOOL:
          break;
        case LITERAL_STR:
          free(expr->as.literal.as.str);
          break;
      }
      break;
    }
    case EXPR_VARIABLE: {
      free(expr->as.var.name);
      break;
    }
    case EXPR_UNARY: {
      free_expr(expr->as.unary.expr);
      free(expr->as.unary.expr);
      free(expr->as.unary.op);
      break;
    }
    case EXPR_BINARY: {
      free_expr(expr->as.binary.lhs);
      free_expr(expr->as.binary.rhs);
      free(expr->as.binary.lhs);
      free(expr->as.binary.rhs);
      break;
    }
    case EXPR_ASSIGN: {
      free_expr(expr->as.assign.lhs);
      free_expr(expr->as.assign.rhs);
      free(expr->as.assign.lhs);
      free(expr->as.assign.rhs);
      break;
    }
    case EXPR_COMPOUND_ASSIGN: {
      free_expr(expr->as.compound_assign.lhs);
      free_expr(expr->as.compound_assign.rhs);
      free(expr->as.compound_assign.lhs);
      free(expr->as.compound_assign.rhs);
      break;
    }
    case EXPR_CALL: {
      free_expr(expr->as.call.target);
      free(expr->as.call.target);
      for (int i = 0; i < expr->as.call.arguments.len; i++) {
        free_expr(&expr->as.call.arguments.data[i]);
      }
      vec_free(&expr->as.call.arguments);
      break;
    }
    case EXPR_ADDROF: {
      free_expr(expr->as.addrof.expr);
      free(expr->as.addrof.expr);
      break;
    }
    case EXPR_SIZEOF: {
      free_expr(expr->as.sizeof_expr.expr);
      free(expr->as.sizeof_expr.expr);
      break;
    }
    case EXPR_DEREF: {
      free_expr(expr->as.deref.expr);
      free(expr->as.deref.expr);
      break;
    }
    case EXPR_CAST: {
      free_expr(expr->as.cast.expr);
      free(expr->as.cast.expr);
      break;
    }
    case EXPR_STRUCT_INIT: {
      free(expr->as.struct_init.struct_name);
      for (int i = 0; i < expr->as.struct_init.values.len; i++) {
        if (expr->as.struct_init.values.data[i].designator) {
          free(expr->as.struct_init.values.data[i].designator);
        }
        free_expr(expr->as.struct_init.values.data[i].expr);
      }
      vec_free(&expr->as.struct_init.values);
      break;
    }
    case EXPR_MEMBER: {
      free(expr->as.member.field_name);
      free_expr(expr->as.member.target);
      break;
    }
  }
}

struct Parameter {
  char *name;
  Type type;
  bool is_mut;
};

typedef Vector(struct Stmt) VecStmt;
typedef Vector(struct Parameter) VecParam;

enum StmtKind {
  STMT_FN,
  STMT_LET,
  STMT_RET,
  STMT_IF,
  STMT_WHILE,
  STMT_LOOP,
  STMT_BREAK,
  STMT_CONTINUE,
  STMT_GOTO,
  STMT_LABELED,
  STMT_BLOCK,
  STMT_EXTERN,
  STMT_EXPR,
  STMT_STRUCT,
  STMT_ENUM,
};

struct StmtFn {
  char *name;
  VecParam params;
  Type retval;
  VecStmt body;
};

struct StmtLet {
  char *name;
  Type type;
  struct Expr *init;
  bool is_mut;
};

struct StmtRet {
  struct Expr *val;
  Type expected_retval;
};

struct StmtIf {
  struct Expr cond;
  struct Stmt *then_block;
  struct Stmt *else_block;
};

struct StmtWhile {
  struct Expr cond;
  struct Stmt *body;
  char *label;
};

struct StmtLoop {
  struct Stmt *body;
  char *label;
};

struct StmtBreak {
  char *label;
};

struct StmtContinue {
  char *label;
};

struct StmtGoto {
  char *label;
};

struct StmtLabeled {
  char *label;
  struct Stmt *stmt;
};

struct StmtBlock {
  VecStmt stmts;
};

struct StmtExtern {
  char *name;
  VecParam params;
  Type retval;
  bool is_variadic;
};

struct StmtExpr {
  struct Expr expr;
};

struct StmtStruct {
  char *name;
  VecStructField fields;
  bool is_union;
};

struct EnumVariant {
  char *name;
  int value;
};

typedef Vector(struct EnumVariant) VecEnumVariant;

struct StmtEnum {
  char *name;
  VecEnumVariant variants;
};

struct Stmt {
  enum StmtKind kind;
  union {
    struct StmtFn fn;
    struct StmtLet let;
    struct StmtRet ret;
    struct StmtIf if_stmt;
    struct StmtWhile while_stmt;
    struct StmtLoop loop;
    struct StmtBreak break_stmt;
    struct StmtContinue continue_stmt;
    struct StmtGoto goto_stmt;
    struct StmtLabeled labeled;
    struct StmtBlock block;
    struct StmtExtern extern_stmt;
    struct StmtExpr expr_stmt;
    struct StmtStruct struct_stmt;
    struct StmtEnum enum_stmt;
  } as;
};

void print_stmt(struct Stmt *stmt, int spaces)
{
  switch (stmt->kind) {
    case STMT_FN: {
      print_indent(spaces);
      printf("STMT_FN(\n");

      print_indent(spaces + 2);
      printf("name = %s,\n", stmt->as.fn.name);

      print_indent(spaces + 2);
      printf("params = [\n");
      for (int i = 0; i < stmt->as.fn.params.len; i++) {
        print_indent(spaces + 4);
        printf("%s: ", stmt->as.fn.params.data[i].name);
        print_type(&stmt->as.fn.params.data[i].type, 0);
        printf(",\n");
      }
      print_indent(spaces + 2);
      printf("],\n");

      print_indent(spaces + 2);
      printf("body = [\n");
      for (int i = 0; i < stmt->as.fn.body.len; i++) {
        print_stmt(&stmt->as.fn.body.data[i], spaces + 4);
      }
      print_indent(spaces + 2);
      printf("],\n");

      print_indent(spaces + 2);
      printf("retval = ");
      print_type(&stmt->as.fn.retval, spaces + 4);
      printf("\n");

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_LET: {
      print_indent(spaces);
      printf("STMT_LET(\n");

      print_indent(spaces + 2);
      printf("name = %s,\n", stmt->as.let.name);

      print_indent(spaces + 2);
      printf("type = ");
      print_type(&stmt->as.let.type, spaces + 4);
      printf(",\n");

      print_indent(spaces + 2);
      printf("init = ");
      print_expr(stmt->as.let.init, spaces + 2);
      printf("\n");

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_RET: {
      print_indent(spaces);
      printf("STMT_RET(\n");

      print_indent(spaces + 2);
      printf("val = ");
      if (stmt->as.ret.val) {
        print_expr(stmt->as.ret.val, spaces + 2);
      } else {
        printf("NULL");
      }
      printf("\n");

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_IF: {
      print_indent(spaces);
      printf("STMT_IF(\n");

      print_indent(spaces + 2);
      printf("cond = ");
      print_expr(&stmt->as.if_stmt.cond, spaces + 2);
      printf(",\n");

      print_indent(spaces + 2);
      printf("then = \n");
      print_stmt(stmt->as.if_stmt.then_block, spaces + 2);

      if (stmt->as.if_stmt.else_block) {
        print_indent(spaces + 2);
        printf("else = \n");
        print_stmt(stmt->as.if_stmt.else_block, spaces + 4);
      }

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_WHILE: {
      print_indent(spaces);
      printf("STMT_WHILE(\n");

      print_indent(spaces + 2);
      printf("label = %s,\n",
             stmt->as.while_stmt.label ? stmt->as.while_stmt.label : "NULL");

      print_indent(spaces + 2);
      printf("cond = ");
      print_expr(&stmt->as.while_stmt.cond, spaces + 2);
      printf(",\n");

      print_indent(spaces + 2);
      printf("body = \n");
      print_stmt(stmt->as.while_stmt.body, spaces + 2);

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_LOOP: {
      print_indent(spaces);
      printf("STMT_LOOP(\n");

      print_indent(spaces + 2);
      printf("body = \n");
      print_stmt(stmt->as.loop.body, spaces + 2);

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_BREAK: {
      print_indent(spaces);
      printf("STMT_BREAK(\n");

      print_indent(spaces + 2);
      printf("label = %s\n",
             stmt->as.break_stmt.label ? stmt->as.break_stmt.label : "NULL");

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_CONTINUE: {
      print_indent(spaces);
      printf("STMT_CONTINUE(\n");

      print_indent(spaces + 2);
      printf("label = %s\n", stmt->as.continue_stmt.label
                                 ? stmt->as.continue_stmt.label
                                 : "NULL");

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_BLOCK: {
      print_indent(spaces);
      printf("STMT_BLOCK(\n");

      print_indent(spaces + 2);
      printf("body = [\n");
      for (int i = 0; i < stmt->as.block.stmts.len; i++) {
        print_stmt(&stmt->as.block.stmts.data[i], spaces + 4);
      }

      print_indent(spaces + 2);
      printf("]\n");

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_EXTERN: {
      print_indent(spaces);
      printf("STMT_EXTERN(\n");

      print_indent(spaces + 2);
      printf("name = %s,\n", stmt->as.extern_stmt.name);

      print_indent(spaces + 2);
      printf("params = [\n");
      for (int i = 0; i < stmt->as.extern_stmt.params.len; i++) {
        print_indent(spaces + 4);
        printf("%s: ", stmt->as.extern_stmt.params.data[i].name);
        print_type(&stmt->as.extern_stmt.params.data[i].type, 0);
        printf(",\n");
      }
      if (stmt->as.extern_stmt.is_variadic) {
        print_indent(spaces + 4);
        printf("...\n");
      }
      print_indent(spaces + 2);
      printf("],\n");

      print_indent(spaces + 2);
      printf("retval = ");
      print_type(&stmt->as.extern_stmt.retval, spaces + 4);
      printf("\n");

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_EXPR: {
      print_indent(spaces);
      printf("STMT_EXPR(\n");

      print_indent(spaces + 2);
      printf("expr = ");
      print_expr(&stmt->as.expr_stmt.expr, spaces + 2);
      printf("\n");

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_STRUCT: {
      print_indent(spaces);
      printf("%s(\n",
             stmt->as.struct_stmt.is_union ? "STMT_UNION" : "STMT_STRUCT");

      print_indent(spaces + 2);
      printf("name = %s,\n", stmt->as.struct_stmt.name);

      print_indent(spaces + 2);
      printf("fields = [\n");

      for (int i = 0; i < stmt->as.struct_stmt.fields.len; i++) {
        print_indent(spaces + 4);
        printf("Field(name: %s, type: ",
               stmt->as.struct_stmt.fields.data[i].name);

        print_type(&stmt->as.struct_stmt.fields.data[i].type, spaces + 6);

        printf(", offset: %d),\n", stmt->as.struct_stmt.fields.data[i].offset);
      }

      print_indent(spaces + 2);
      printf("]\n");

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_ENUM: {
      print_indent(spaces);
      printf("STMT_ENUM(\n");
      print_indent(spaces + 2);
      printf("name = %s,\n", stmt->as.enum_stmt.name);
      print_indent(spaces + 2);
      printf("variants = [\n");
      for (int i = 0; i < stmt->as.enum_stmt.variants.len; i++) {
        print_indent(spaces + 4);
        printf("%s = %d,\n", stmt->as.enum_stmt.variants.data[i].name,
               stmt->as.enum_stmt.variants.data[i].value);
      }
      print_indent(spaces + 2);
      printf("]\n");
      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_LABELED: {
      print_indent(spaces);
      printf("STMT_LABELED(\n");
      print_stmt(stmt->as.labeled.stmt, spaces + 2);
      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_GOTO: {
      print_indent(spaces);
      printf("STMT_GOTO(\n");
      print_indent(spaces + 2);
      printf("label = \"%s\",\n", stmt->as.goto_stmt.label);
      print_indent(spaces);
      printf(")\n");
      break;
    }
    default:
      assert(0 && "Unhandled statement kind in print_stmt");
  }
}

void free_stmt(struct Stmt *stmt)
{
  switch (stmt->kind) {
    case STMT_LOOP: {
      free(stmt->as.loop.label);
      free_stmt(stmt->as.loop.body);
      break;
    }
    case STMT_ENUM: {
      free(stmt->as.enum_stmt.name);
      for (int i = 0; i < stmt->as.enum_stmt.variants.len; i++) {
        free(stmt->as.enum_stmt.variants.data[i].name);
      }
      vec_free(&stmt->as.enum_stmt.variants);
      break;
    }
    case STMT_STRUCT: {
      free(stmt->as.struct_stmt.name);
      for (int i = 0; i < stmt->as.struct_stmt.fields.len; i++) {
        free(stmt->as.struct_stmt.fields.data[i].name);
        free_type(&stmt->as.struct_stmt.fields.data[i].type);
      }

      break;
    }
    case STMT_EXTERN: {
      free(stmt->as.extern_stmt.name);
      for (int i = 0; i < stmt->as.extern_stmt.params.len; i++) {
        free(stmt->as.extern_stmt.params.data[i].name);
        free_type(&stmt->as.extern_stmt.params.data[i].type);
      }
      vec_free(&stmt->as.extern_stmt.params);
      free_type(&stmt->as.extern_stmt.retval);
      break;
    }
    case STMT_BREAK:
    case STMT_CONTINUE:
      break;
    case STMT_GOTO: {
      free(stmt->as.goto_stmt.label);
      break;
    }
    case STMT_LABELED: {
      free(stmt->as.labeled.label);
      free_stmt(stmt->as.labeled.stmt);
      free(stmt->as.labeled.stmt);
      break;
    }
    case STMT_LET: {
      free(stmt->as.let.name);
      free_expr(stmt->as.let.init);
      free(stmt->as.let.init);
      free_type(&stmt->as.let.type);
      break;
    }
    case STMT_WHILE: {
      free_expr(&stmt->as.while_stmt.cond);
      free_stmt(stmt->as.while_stmt.body);
      free(stmt->as.while_stmt.body);
      if (stmt->as.while_stmt.label) {
        free(stmt->as.while_stmt.label);
      }
      break;
    }
    case STMT_IF: {
      free_expr(&stmt->as.if_stmt.cond);
      free_stmt(stmt->as.if_stmt.then_block);
      free(stmt->as.if_stmt.then_block);
      if (stmt->as.if_stmt.else_block) {
        free_stmt(stmt->as.if_stmt.else_block);
        free(stmt->as.if_stmt.else_block);
      }
      break;
    }
    case STMT_FN: {
      free(stmt->as.fn.name);
      for (int i = 0; i < stmt->as.fn.params.len; i++) {
        free(stmt->as.fn.params.data[i].name);
        free_type(&stmt->as.fn.params.data[i].type);
      }
      vec_free(&stmt->as.fn.params);
      for (int i = 0; i < stmt->as.fn.body.len; i++) {
        free_stmt(&stmt->as.fn.body.data[i]);
      }
      free_type(&stmt->as.fn.retval);
      vec_free(&stmt->as.fn.body);
      break;
    }
    case STMT_BLOCK: {
      for (int i = 0; i < stmt->as.block.stmts.len; i++) {
        free_stmt(&stmt->as.block.stmts.data[i]);
      }
      vec_free(&stmt->as.block.stmts);
      break;
    }
    case STMT_RET: {
      if (stmt->as.ret.val) {
        free_expr(stmt->as.ret.val);
        free(stmt->as.ret.val);
      }
      break;
    }
    case STMT_EXPR: {
      free_expr(&stmt->as.expr_stmt.expr);
      break;
    }
  }
}

struct AST {
  VecStmt stmts;
};

void print_ast(struct AST *ast)
{
  for (int i = 0; i < ast->stmts.len; i++) {
    print_stmt(&ast->stmts.data[i], 0);
  }
}

void free_ast(struct AST *ast)
{
  if (!ast) {
    return;
  }
  for (int i = 0; i < ast->stmts.len; i++) {
    free_stmt(&ast->stmts.data[i]);
  }
  vec_free(&ast->stmts);
  free(ast);
}

struct Parser {
  struct StmtFn *current_fn;
  struct Token *curr;
  struct Token *prev;
  VecToken *tokens;
  int idx;
  VecStmt *global_stmts;
};

struct Token *advance_parser(struct Parser *parser);

void init_parser(struct Parser *parser, VecToken *tokens)
{
  parser->idx = 0;
  parser->tokens = tokens;
  parser->prev = NULL;
  parser->curr = NULL;
  parser->current_fn = NULL;

  advance_parser(parser);
}

struct Token *next_token(struct Parser *parser)
{
  if (parser->idx < parser->tokens->len) {
    return &parser->tokens->data[parser->idx++];
  }
  return NULL;
}

struct Token *advance_parser(struct Parser *parser)
{
  parser->prev = parser->curr;
  parser->curr = next_token(parser);

  return parser->prev;
}

bool check(struct Parser *parser, enum TokenKind kind)
{
  return parser->curr->kind == kind;
}

bool check2(struct Parser *parser, enum TokenKind kind1, enum TokenKind kind2)
{
  return parser->prev->kind == kind1 && parser->curr->kind == kind2;
}

bool check_next(struct Parser *parser, enum TokenKind kind)
{
  return parser->idx < parser->tokens->len &&
         parser->tokens->data[parser->idx].kind == kind;
}

bool match(struct Parser *parser, int size, ...)
{
  va_list ap;
  va_start(ap, size);
  for (int i = 0; i < size; i++) {
    enum TokenKind kind = va_arg(ap, enum TokenKind);
    if (check(parser, kind)) {
      advance_parser(parser);
      va_end(ap);
      return true;
    }
  }
  va_end(ap);
  return false;
}

struct Token *consume(struct Parser *parser, enum TokenKind kind)
{
  if (parser->curr && parser->curr->kind == kind) {
    return advance_parser(parser);
  }
  printf("Encountered wrong token.  Prev is: ");
  print_token(parser->prev);
  printf("\n");
  printf("Encountered wrong token.  Curr is: ");
  print_token(parser->curr);
  printf("\n");
  return NULL;
}

struct Token *consume_any(struct Parser *parser, int n, ...)
{
  va_list ap;
  va_start(ap, n);

  for (int i = 0; i < n; i++) {
    enum TokenKind kind;

    kind = va_arg(ap, enum TokenKind);
    if (parser->curr && parser->curr->kind == kind) {
      va_end(ap);
      return advance_parser(parser);
    }
  }

  va_end(ap);
  return NULL;
}

struct ParseFnResult {
  bool is_ok;
  char *msg;
  union {
    struct Expr expr;
    struct Stmt stmt;
  } as;
};

struct ParseResult {
  bool is_ok;
  char *msg;
  struct AST *ast;
};

struct ParseFnResult parse_stmt(struct Parser *parser);
struct ParseFnResult parse_expr(struct Parser *parser);

struct ParseFnResult primary(struct Parser *parser)
{
  struct ParseFnResult res;

  res.is_ok = true;
  res.msg = NULL;

  if (check(parser, TOKEN_NUMBER) || check(parser, TOKEN_FP_NUMBER)) {
    struct Literal literal;
    struct Token *token_literal;
    unsigned long long val;
    bool is_float;

    is_float = check(parser, TOKEN_FP_NUMBER);

    token_literal = consume(parser, is_float ? TOKEN_FP_NUMBER : TOKEN_NUMBER);
    if (!token_literal) {
      return (struct ParseFnResult){
          .is_ok = false, .as.expr = {0}, .msg = "Expected number"};
    }

    literal.kind = LITERAL_NUM;

    if (!is_float) {
      val = strtoull(parser->prev->start, NULL, 10);

      if (val <= UCHAR_MAX) {
        literal.type = (Type){.kind = U8_T};
        literal.as.u8 = val;
      } else if (val <= USHRT_MAX) {
        literal.type = (Type){.kind = U16_T};
        literal.as.u16 = val;
      } else if (val <= UINT_MAX) {
        literal.type = (Type){.kind = U32_T};
        literal.as.u32 = val;
      } else if (val <= ULLONG_MAX) {
        literal.type = (Type){.kind = U64_T};
        literal.as.u64 = val;
      } else {
        return (struct ParseFnResult){
            .is_ok = false, .msg = "Can't parse literal", .as.expr = {0}};
      }
    } else {
      double fval;

      fval = strtod(parser->prev->start, NULL);

      literal.type = (Type){.kind = F64_T};
      literal.as.f64 = fval;
    }
    res.as.expr = (struct Expr){
        .kind = EXPR_LITERAL, .as.literal = literal, .type = literal.type};
  } else if (check(parser, TOKEN_STRING)) {
    struct Literal literal = {0};
    struct Token *token_literal;

    token_literal = consume(parser, TOKEN_STRING);
    if (!token_literal) {
      return (struct ParseFnResult){
          .is_ok = false, .as.expr = {0}, .msg = "Expected string"};
    }

    literal.kind = LITERAL_STR;
    literal.as.str = strndup(token_literal->start, token_literal->len);
    literal.type = (Type){.kind = STR_T};

    res.as.expr = (struct Expr){.kind = EXPR_LITERAL, .as.literal = literal};
  } else if (check(parser, TOKEN_IDENTIFIER)) {
    struct Token *token_id = consume(parser, TOKEN_IDENTIFIER);
    if (!token_id) {
      return (struct ParseFnResult){
          .is_ok = false, .as.expr = {0}, .msg = "Expected identifier"};
    }

    if (match(parser, 1, TOKEN_LBRACE)) {
      VecStructInitItem values = {0};

      while (!check(parser, TOKEN_RBRACE)) {
        char *designator = NULL;

        /* Parse designated initializer chain: .a.b: expr */
        if (check(parser, TOKEN_DOT)) {
          advance_parser(parser);
          char buf[256] = {0};

          while (true) {
            struct Token *id = consume(parser, TOKEN_IDENTIFIER);
            if (!id) {
              return (struct ParseFnResult){.is_ok = false,
                                            .msg = "Expected identifier"};
            }

            if (buf[0] != '\0') {
              strcat(buf, ".");
            }
            strncat(buf, id->start, id->len);

            if (check(parser, TOKEN_DOT)) {
              advance_parser(parser);
            } else {
              break;
            }
          }
          consume(parser, TOKEN_COLON);
          designator = strdup(buf);
        }
        /* Fallback: positional or legacy `ident: expr` */
        else if (check(parser, TOKEN_IDENTIFIER) &&
                 parser->idx < parser->tokens->len &&
                 parser->tokens->data[parser->idx].kind == TOKEN_COLON) {
          struct Token *id = consume(parser, TOKEN_IDENTIFIER);
          consume(parser, TOKEN_COLON);
          designator = strndup(id->start, id->len);
        }

        struct ParseFnResult val_res = parse_expr(parser);
        if (!val_res.is_ok) {
          return val_res;
        }

        struct StructInitItem item;
        item.designator = designator;
        item.expr = ALLOC(val_res.as.expr);
        item.resolved_offset = 0;
        vec_insert(&values, item);

        if (check(parser, TOKEN_COMMA)) {
          advance_parser(parser);
        }
      }

      consume(parser, TOKEN_RBRACE);

      struct ExprStructInit init;
      init.struct_name = strndup(token_id->start, token_id->len);
      init.values = values;
      res.as.expr =
          (struct Expr){.kind = EXPR_STRUCT_INIT, .as.struct_init = init};
    } else {
      struct ExprVar var;
      var.name = strndup(token_id->start, token_id->len);
      var.type = (Type){.kind = UNKNOWN_T};

      res.is_ok = true;
      res.as.expr.kind = EXPR_VARIABLE;
      res.as.expr.as.var = var;
    }
  } else if (match(parser, 1, TOKEN_LPAREN)) {
    struct ParseFnResult expr_res;
    struct Token *token_rparen;

    expr_res = parse_expr(parser);
    if (!expr_res.is_ok) {
      return expr_res;
    }

    token_rparen = consume(parser, TOKEN_RPAREN);
    if (!token_rparen) {
      return (struct ParseFnResult){
          .is_ok = false, .msg = "Expected ')'", .as.stmt = {0}};
    }

    res = expr_res;
  }

  return res;
}

struct ParseFnResult finish_call(struct Parser *parser, struct Expr callee)
{
  VecExpr arguments = {0};
  if (!check(parser, TOKEN_RPAREN)) {
    do {
      struct ParseFnResult r;

      r = parse_expr(parser);
      if (!r.is_ok) {
        for (int i = 0; i < arguments.len; i++) {
          free_expr(&arguments.data[i]);
        }
        free_expr(&callee);
        vec_free(&arguments);
        return r;
      }
      vec_insert(&arguments, r.as.expr);
    } while (match(parser, 1, TOKEN_COMMA));
  }

  struct Token *token_rparen;

  token_rparen = consume(parser, TOKEN_RPAREN);
  if (!token_rparen) {
    for (int i = 0; i < arguments.len; i++) {
      free_expr(&arguments.data[i]);
    }
    free_expr(&callee);
    vec_free(&arguments);
    return (struct ParseFnResult){.is_ok = false,
                                  .as.expr = {0},
                                  .msg = "Expected ')' after expression."};
  }

  struct ExprCall call_expr = {
      .target = ALLOC(callee),
      .arguments = arguments,
  };

  struct Expr e;

  e.kind = EXPR_CALL;
  e.as.call = call_expr;

  return (struct ParseFnResult){.as.expr = e, .is_ok = true, .msg = NULL};
}

bool parse_enum_body(struct Parser *parser, VecEnumVariant *out_variants)
{
  int current_val = 0;

  while (!check(parser, TOKEN_RBRACE)) {
    struct Token *var_id = consume(parser, TOKEN_IDENTIFIER);
    if (!var_id) {
      return false;
    }
    char *var_name = strndup(var_id->start, var_id->len);

    if (check(parser, TOKEN_EQUAL)) {
      advance_parser(parser);

      struct ParseFnResult expr_res = parse_expr(parser);
      if (!expr_res.is_ok) {
        return false;
      }

      struct Expr *e = &expr_res.as.expr;
      int sign = 1;

      if (e->kind == EXPR_UNARY && strncmp(e->as.unary.op, "-", 1) == 0) {
        sign = -1;
        e = e->as.unary.expr;
      }

      if (e->kind == EXPR_LITERAL && e->as.literal.kind == LITERAL_NUM) {
        unsigned long long raw_val = 0;
        switch (e->as.literal.type.kind) {
          case U8_T:
            raw_val = e->as.literal.as.u8;
            break;
          case I8_T:
            raw_val = e->as.literal.as.i8;
            break;
          case U16_T:
            raw_val = e->as.literal.as.u16;
            break;
          case I16_T:
            raw_val = e->as.literal.as.i16;
            break;
          case U32_T:
            raw_val = e->as.literal.as.u32;
            break;
          case I32_T:
            raw_val = e->as.literal.as.i32;
            break;
          case U64_T:
            raw_val = e->as.literal.as.u64;
            break;
          case I64_T:
            raw_val = e->as.literal.as.i64;
            break;
          default:
            break;
        }
        current_val = sign * (int) raw_val;
      } else {
        free_expr(&expr_res.as.expr);
        return false;
      }
      free_expr(&expr_res.as.expr);
    }

    struct EnumVariant var;
    var.name = var_name;
    var.value = current_val;
    vec_insert(out_variants, var);

    enum_variant_insert(var_name, current_val);

    current_val++;

    if (check(parser, TOKEN_COMMA)) {
      advance_parser(parser);
    }
  }
  return true;
}

Type parse_type(struct Parser *parser)
{
  if (match(parser, 1, TOKEN_STAR)) {
    Type *base = malloc(sizeof(Type));
    *base = parse_type(parser);
    return (Type){.kind = PTR_T, .as.base = base};
  }

  if (check(parser, TOKEN_VOID)) {
    advance_parser(parser);
    return (Type){.kind = VOID_T};
  }

  if (check(parser, TOKEN_UNION) || check(parser, TOKEN_STRUCT)) {
    bool is_union = check(parser, TOKEN_UNION);
    advance_parser(parser);

    char *anon_name = mkuniq(is_union ? "anon_union" : "anon_struct");

    consume(parser, TOKEN_LBRACE);
    VecStructField fields = {0};

    while (!check(parser, TOKEN_RBRACE)) {
      char *field_name = NULL;
      Type field_type;

      if (check(parser, TOKEN_IDENTIFIER) &&
          parser->idx < parser->tokens->len &&
          parser->tokens->data[parser->idx].kind == TOKEN_COLON) {
        struct Token *name_tok = consume(parser, TOKEN_IDENTIFIER);
        consume(parser, TOKEN_COLON);
        field_type = parse_type(parser);
        field_name = strndup(name_tok->start, name_tok->len);
      } else {
        field_type = parse_type(parser);
        struct Token *name_tok = consume(parser, TOKEN_IDENTIFIER);
        if (name_tok) {
          field_name = strndup(name_tok->start, name_tok->len);
        } else {
          field_name = strdup("");
        }
      }

      struct StructField field;
      field.name = field_name;
      field.type = field_type;
      field.offset = 0;
      vec_insert(&fields, field);

      if (check(parser, TOKEN_COMMA)) {
        advance_parser(parser);
      }
    }
    consume(parser, TOKEN_RBRACE);

    struct StmtStruct struct_stmt;
    struct_stmt.name = strdup(anon_name);
    struct_stmt.fields = fields;
    struct_stmt.is_union = is_union;

    struct Stmt s;
    s.kind = STMT_STRUCT;
    s.as.struct_stmt = struct_stmt;

    if (parser->global_stmts) {
      vec_insert(parser->global_stmts, s);
    }

    Type custom_type;
    custom_type.kind = STRUCT_T;
    custom_type.as.struct_name = anon_name;
    return custom_type;
  }

  if (check(parser, TOKEN_ENUM)) {
    advance_parser(parser);

    if (check(parser, TOKEN_LBRACE)) {
      char *anon_name = mkuniq("anon_enum");
      enum_type_insert(anon_name);

      consume(parser, TOKEN_LBRACE);
      VecEnumVariant variants = {0};

      if (!parse_enum_body(parser, &variants)) {
        return (Type){.kind = UNKNOWN_T};
      }
      consume(parser, TOKEN_RBRACE);

      struct StmtEnum enum_stmt;
      enum_stmt.name = anon_name;
      enum_stmt.variants = variants;

      struct Stmt s;
      s.kind = STMT_ENUM;
      s.as.enum_stmt = enum_stmt;

      if (parser->global_stmts) {
        vec_insert(parser->global_stmts, s);
      }

      return (Type){.kind = I32_T};
    }

    struct Token *id_token = consume(parser, TOKEN_IDENTIFIER);
    if (id_token) {
      return (Type){.kind = I32_T};
    }
  }

  struct Token *type_token =
      consume_any(parser, 12, TOKEN_U8, TOKEN_U16, TOKEN_U32, TOKEN_U64,
                  TOKEN_I8, TOKEN_I16, TOKEN_I32, TOKEN_I64, TOKEN_F32,
                  TOKEN_F64, TOKEN_BOOL, TOKEN_STR);

  if (type_token) {
    if (strncmp(type_token->start, "i8", type_token->len) == 0) {
      return (Type){.kind = I8_T};
    } else if (strncmp(type_token->start, "i16", type_token->len) == 0) {
      return (Type){.kind = I16_T};
    } else if (strncmp(type_token->start, "i32", type_token->len) == 0) {
      return (Type){.kind = I32_T};
    } else if (strncmp(type_token->start, "i64", type_token->len) == 0) {
      return (Type){.kind = I64_T};
    } else if (strncmp(type_token->start, "u8", type_token->len) == 0) {
      return (Type){.kind = U8_T};
    } else if (strncmp(type_token->start, "u16", type_token->len) == 0) {
      return (Type){.kind = U16_T};
    } else if (strncmp(type_token->start, "u32", type_token->len) == 0) {
      return (Type){.kind = U32_T};
    } else if (strncmp(type_token->start, "u64", type_token->len) == 0) {
      return (Type){.kind = U64_T};
    } else if (strncmp(type_token->start, "f32", type_token->len) == 0) {
      return (Type){.kind = F32_T};
    } else if (strncmp(type_token->start, "f64", type_token->len) == 0) {
      return (Type){.kind = F64_T};
    } else if (strncmp(type_token->start, "bool", type_token->len) == 0) {
      return (Type){.kind = BOOL_T};
    } else if (strncmp(type_token->start, "str", type_token->len) == 0) {
      return (Type){.kind = STR_T};
    }
  } else if (check(parser, TOKEN_IDENTIFIER)) {
    struct Token *id_token = consume(parser, TOKEN_IDENTIFIER);
    char *name = strndup(id_token->start, id_token->len);

    if (is_enum_type(name)) {
      free(name);
      return (Type){.kind = I32_T};
    } else {
      Type custom_type;
      custom_type.kind = STRUCT_T;
      custom_type.as.struct_name = name;
      return custom_type;
    }
  }

  if (check(parser, TOKEN_ENUM)) {
    advance_parser(parser);
    struct Token *id_token = consume(parser, TOKEN_IDENTIFIER);
    if (id_token) {
      return (Type){.kind = I32_T};
    }
  }
  return (Type){.kind = UNKNOWN_T};
}

struct ParseFnResult postfix(struct Parser *parser)
{
  struct ParseFnResult expr_result;

  expr_result = primary(parser);
  if (!expr_result.is_ok) {
    return expr_result;
  }

  struct Expr expr = expr_result.as.expr;

  for (;;) {
    if (match(parser, 1, TOKEN_LPAREN)) {
      struct ParseFnResult finish_call_result;

      finish_call_result = finish_call(parser, expr);
      if (!finish_call_result.is_ok) {
        return finish_call_result;
      }
      expr = finish_call_result.as.expr;
    } else if (match(parser, 2, TOKEN_DOT, TOKEN_ARROW)) {
      bool is_arrow;
      struct Token *field;

      is_arrow = (parser->prev->kind == TOKEN_ARROW);
      field = consume(parser, TOKEN_IDENTIFIER);

      struct ExprMember member_expr = {
          .target = ALLOC(expr),
          .field_name = strndup(field->start, field->len),
          .is_arrow = is_arrow,
      };

      expr.kind = EXPR_MEMBER;
      expr.as.member = member_expr;
    } else if (match(parser, 1, TOKEN_AS)) {
      Type t;

      t = parse_type(parser);

      struct ExprCast cast_expr = {
          .expr = ALLOC(expr),
          .target_type = t,
      };

      expr.kind = EXPR_CAST;
      expr.as.cast = cast_expr;
    } else {
      break;
    }
  }

  return (struct ParseFnResult){.as.expr = expr, .is_ok = true, .msg = NULL};
}

struct ParseFnResult unary(struct Parser *parser)
{
  if (match(parser, 5, TOKEN_MINUS, TOKEN_BANG, TOKEN_TILDE, TOKEN_AMPERSAND,
            TOKEN_STAR)) {
    char *op = strndup(parser->prev->start, parser->prev->len);

    if (strncmp(op, "-", 1) == 0 || strncmp(op, "!", 1) == 0 ||
        strncmp(op, "~", 1) == 0) {
      struct ParseFnResult right_result;

      right_result = unary(parser);
      if (!right_result.is_ok) {
        free(op);
        return right_result;
      }

      struct Expr right, e;

      right = right_result.as.expr;

      struct ExprUnary unary_expr = {.expr = ALLOC(right), .op = op};

      e.kind = EXPR_UNARY;
      e.as.unary = unary_expr;

      return (struct ParseFnResult){.as.expr = e, .is_ok = true, .msg = NULL};
    } else if (strncmp(op, "*", 1) == 0) {
      struct ParseFnResult right_result;

      right_result = unary(parser);
      if (!right_result.is_ok) {
        free(op);
        return right_result;
      }

      struct Expr right, e;

      right = right_result.as.expr;

      struct ExprDeref deref_expr = {.expr = ALLOC(right)};

      e.kind = EXPR_DEREF;
      e.as.deref = deref_expr;

      free(op);

      return (struct ParseFnResult){.as.expr = e, .is_ok = true, .msg = NULL};

    } else if (strncmp(op, "&", 1) == 0) {
      struct ParseFnResult right_result;

      right_result = unary(parser);
      if (!right_result.is_ok) {
        free(op);
        return right_result;
      }

      struct Expr right, e;

      right = right_result.as.expr;

      struct ExprAddrOf addrof_expr = {.expr = ALLOC(right)};

      e.kind = EXPR_ADDROF;
      e.as.addrof = addrof_expr;

      free(op);

      return (struct ParseFnResult){.as.expr = e, .is_ok = true, .msg = NULL};
    } else {
      assert(0);
    }
  } else if (match(parser, 1, TOKEN_SIZEOF)) {
    struct ParseFnResult right_result;
    struct Token *token_lparen, *token_rparen;

    token_lparen = consume(parser, TOKEN_LPAREN);
    if (!token_lparen) {
      return (struct ParseFnResult){
          .is_ok = false, .msg = "Expected '(' after sizeoef", .as.stmt = {0}};
    }

    right_result = unary(parser);
    if (!right_result.is_ok) {
      return right_result;
    }

    token_rparen = consume(parser, TOKEN_RPAREN);
    if (!token_rparen) {
      return (struct ParseFnResult){.is_ok = false,
                                    .msg = "Expected ')' after sizeof expr",
                                    .as.stmt = {0}};
    }

    struct Expr right, e;

    right = right_result.as.expr;

    struct ExprSizeof sizeof_expr = {.expr = ALLOC(right)};

    e.kind = EXPR_SIZEOF;
    e.as.sizeof_expr = sizeof_expr;
    return (struct ParseFnResult){.as.expr = e, .is_ok = true, .msg = NULL};
  }

  return postfix(parser);
}

struct ParseFnResult factor(struct Parser *parser)
{
  struct ParseFnResult left_res, right_res;
  struct Expr left, right;

  left_res = unary(parser);
  if (!left_res.is_ok) {
    return left_res;
  }

  left = left_res.as.expr;
  while (match(parser, 2, TOKEN_STAR, TOKEN_SLASH)) {
    char *op = parser->prev->start;

    right_res = unary(parser);
    if (!right_res.is_ok) {
      return right_res;
    }

    right = right_res.as.expr;

    struct ExprBin binexpr;

    switch (*op) {
      case '*':
        binexpr.kind = EXPR_BIN_MUL;
        break;
      case '/':
        binexpr.kind = EXPR_BIN_DIV;
        break;
      default:
        assert(0);
    }

    binexpr.lhs = ALLOC(left);
    binexpr.rhs = ALLOC(right);

    left = (struct Expr){.kind = EXPR_BINARY, .as.binary = binexpr};

    left_res.as.expr = left;
  }

  return left_res;
}

struct ParseFnResult term(struct Parser *parser)
{
  struct ParseFnResult left_res, right_res;
  struct Expr left, right;

  left_res = factor(parser);
  if (!left_res.is_ok) {
    return left_res;
  }

  left = left_res.as.expr;
  while (match(parser, 2, TOKEN_PLUS, TOKEN_MINUS)) {
    char *op = parser->prev->start;

    right_res = factor(parser);
    if (!right_res.is_ok) {
      return right_res;
    }

    right = right_res.as.expr;

    struct ExprBin binexpr;

    switch (*op) {
      case '+':
        binexpr.kind = EXPR_BIN_ADD;
        break;
      case '-':
        binexpr.kind = EXPR_BIN_SUB;
        break;
      default:
        assert(0);
    }

    binexpr.lhs = ALLOC(left);
    binexpr.rhs = ALLOC(right);

    left = (struct Expr){.kind = EXPR_BINARY, .as.binary = binexpr};

    left_res.as.expr = left;
  }

  return left_res;
}

struct ParseFnResult shift(struct Parser *parser)
{
  struct ParseFnResult left_res, right_res;
  struct Expr left, right;

  left_res = term(parser);
  if (!left_res.is_ok) {
    return left_res;
  }

  left = left_res.as.expr;
  while (match(parser, 2, TOKEN_LESS_LESS, TOKEN_GREATER_GREATER)) {
    enum TokenKind op = parser->prev->kind;

    right_res = term(parser);
    if (!right_res.is_ok) {
      return right_res;
    }

    right = right_res.as.expr;

    struct ExprBin binexpr;
    binexpr.kind =
        (op == TOKEN_LESS_LESS) ? EXPR_BIN_SHIFT_LEFT : EXPR_BIN_SHIFT_RIGHT;
    binexpr.lhs = ALLOC(left);
    binexpr.rhs = ALLOC(right);

    left = (struct Expr){.kind = EXPR_BINARY, .as.binary = binexpr};
    left_res.as.expr = left;
  }

  return left_res;
}

struct ParseFnResult comparison(struct Parser *parser)
{
  struct ParseFnResult left_res, right_res;
  struct Expr left, right;

  left_res = shift(parser);
  if (!left_res.is_ok) {
    return left_res;
  }

  left = left_res.as.expr;
  while (match(parser, 6, TOKEN_LESS, TOKEN_LESS_EQUAL, TOKEN_GREATER,
               TOKEN_GREATER_EQUAL, TOKEN_EQUAL_EQUAL, TOKEN_BANG_EQUAL)) {
    char *op = parser->prev->start;

    right_res = shift(parser);
    if (!right_res.is_ok) {
      return right_res;
    }

    right = right_res.as.expr;

    struct ExprBin binexpr;

    if (strncmp(op, "<=", 2) == 0) {
      binexpr.kind = EXPR_BIN_LESS_EQUAL;
    } else if (strncmp(op, ">=", 2) == 0) {
      binexpr.kind = EXPR_BIN_GREATER_EQUAL;
    } else if (strncmp(op, "==", 2) == 0) {
      binexpr.kind = EXPR_BIN_EQUAL_EQUAL;
    } else if (strncmp(op, "!=", 2) == 0) {
      binexpr.kind = EXPR_BIN_BANG_EQUAL;
    } else if (strncmp(op, "<", 1) == 0) {
      binexpr.kind = EXPR_BIN_LESS;
    } else if (strncmp(op, ">", 1) == 0) {
      binexpr.kind = EXPR_BIN_GREATER;
    }

    binexpr.lhs = ALLOC(left);
    binexpr.rhs = ALLOC(right);

    left = (struct Expr){.kind = EXPR_BINARY, .as.binary = binexpr};
    left_res.as.expr = left;
  }

  return left_res;
}

struct ParseFnResult bitwise_and(struct Parser *parser)
{
  struct ParseFnResult left_res, right_res;
  struct Expr left, right;

  left_res = comparison(parser);
  if (!left_res.is_ok) {
    return left_res;
  }

  left = left_res.as.expr;
  while (match(parser, 1, TOKEN_AMPERSAND)) {
    right_res = comparison(parser);
    if (!right_res.is_ok) {
      return right_res;
    }

    right = right_res.as.expr;

    struct ExprBin binexpr;
    binexpr.kind = EXPR_BIN_BITWISE_AND;
    binexpr.lhs = ALLOC(left);
    binexpr.rhs = ALLOC(right);

    left = (struct Expr){.kind = EXPR_BINARY, .as.binary = binexpr};
    left_res.as.expr = left;
  }

  return left_res;
}

struct ParseFnResult bitwise_xor(struct Parser *parser)
{
  struct ParseFnResult left_res, right_res;
  struct Expr left, right;

  left_res = bitwise_and(parser);
  if (!left_res.is_ok) {
    return left_res;
  }

  left = left_res.as.expr;
  while (match(parser, 1, TOKEN_CARET)) {
    right_res = bitwise_and(parser);
    if (!right_res.is_ok) {
      return right_res;
    }

    right = right_res.as.expr;

    struct ExprBin binexpr;
    binexpr.kind = EXPR_BIN_BITWISE_XOR;
    binexpr.lhs = ALLOC(left);
    binexpr.rhs = ALLOC(right);

    left = (struct Expr){.kind = EXPR_BINARY, .as.binary = binexpr};
    left_res.as.expr = left;
  }

  return left_res;
}

struct ParseFnResult bitwise_or(struct Parser *parser)
{
  struct ParseFnResult left_res, right_res;
  struct Expr left, right;

  left_res = bitwise_xor(parser);
  if (!left_res.is_ok) {
    return left_res;
  }

  left = left_res.as.expr;
  while (match(parser, 1, TOKEN_PIPE)) {
    right_res = bitwise_xor(parser);
    if (!right_res.is_ok) {
      return right_res;
    }

    right = right_res.as.expr;

    struct ExprBin binexpr;
    binexpr.kind = EXPR_BIN_BITWISE_OR;
    binexpr.lhs = ALLOC(left);
    binexpr.rhs = ALLOC(right);

    left = (struct Expr){.kind = EXPR_BINARY, .as.binary = binexpr};
    left_res.as.expr = left;
  }

  return left_res;
}

struct ParseFnResult logical_and(struct Parser *parser)
{
  struct ParseFnResult left_res, right_res;
  struct Expr left, right;

  left_res = bitwise_or(parser);
  if (!left_res.is_ok) {
    return left_res;
  }

  left = left_res.as.expr;
  while (match(parser, 1, TOKEN_AMPERSAND_AMPERSAND)) {
    right_res = bitwise_or(parser);
    if (!right_res.is_ok) {
      return right_res;
    }

    right = right_res.as.expr;

    struct ExprBin binexpr;
    binexpr.kind = EXPR_BIN_LOGICAL_AND;
    binexpr.lhs = ALLOC(left);
    binexpr.rhs = ALLOC(right);

    left = (struct Expr){.kind = EXPR_BINARY, .as.binary = binexpr};
    left_res.as.expr = left;
  }

  return left_res;
}

struct ParseFnResult logical_or(struct Parser *parser)
{
  struct ParseFnResult left_res, right_res;
  struct Expr left, right;

  left_res = logical_and(parser);
  if (!left_res.is_ok) {
    return left_res;
  }

  left = left_res.as.expr;
  while (match(parser, 1, TOKEN_PIPE_PIPE)) {
    right_res = logical_and(parser);
    if (!right_res.is_ok) {
      return right_res;
    }

    right = right_res.as.expr;

    struct ExprBin binexpr;
    binexpr.kind = EXPR_BIN_LOGICAL_OR;
    binexpr.lhs = ALLOC(left);
    binexpr.rhs = ALLOC(right);

    left = (struct Expr){.kind = EXPR_BINARY, .as.binary = binexpr};
    left_res.as.expr = left;
  }

  return left_res;
}

struct ParseFnResult assignment(struct Parser *parser)
{
  struct ParseFnResult expr_result, right_result;
  struct Expr expr, right;

  expr_result = logical_or(parser);
  if (!expr_result.is_ok) {
    return expr_result;
  }

  expr = expr_result.as.expr;
  if (match(parser, 10, TOKEN_EQUAL, TOKEN_PLUS_EQUAL, TOKEN_MINUS_EQUAL,
            TOKEN_STAR_EQUAL, TOKEN_SLASH_EQUAL, TOKEN_AMPERSAND_EQUAL,
            TOKEN_PIPE_EQUAL, TOKEN_CARET_EQUAL, TOKEN_LESS_LESS_EQUAL,
            TOKEN_GREATER_GREATER_EQUAL)) {
    enum TokenKind op_kind = parser->prev->kind;

    right_result = assignment(parser);
    if (!right_result.is_ok) {
      free_expr(&expr);
      return right_result;
    }

    right = right_result.as.expr;

    if (op_kind == TOKEN_EQUAL) {
      struct ExprAssign assignexp = {.lhs = ALLOC(expr), .rhs = ALLOC(right)};
      expr = (struct Expr){.kind = EXPR_ASSIGN, .as.assign = assignexp};
    } else {
      enum ExprBinKind bin_kind;

      if (op_kind == TOKEN_PLUS_EQUAL) {
        bin_kind = EXPR_BIN_ADD;
      } else if (op_kind == TOKEN_MINUS_EQUAL) {
        bin_kind = EXPR_BIN_SUB;
      } else if (op_kind == TOKEN_STAR_EQUAL) {
        bin_kind = EXPR_BIN_MUL;
      } else if (op_kind == TOKEN_SLASH_EQUAL) {
        bin_kind = EXPR_BIN_DIV;
      } else if (op_kind == TOKEN_AMPERSAND_EQUAL) {
        bin_kind = EXPR_BIN_BITWISE_AND;
      } else if (op_kind == TOKEN_PIPE_EQUAL) {
        bin_kind = EXPR_BIN_BITWISE_OR;
      } else if (op_kind == TOKEN_CARET_EQUAL) {
        bin_kind = EXPR_BIN_BITWISE_XOR;
      } else if (op_kind == TOKEN_LESS_LESS_EQUAL) {
        bin_kind = EXPR_BIN_SHIFT_LEFT;
      } else {
        bin_kind = EXPR_BIN_SHIFT_RIGHT;
      }

      struct ExprCompoundAssign comp;

      comp.kind = bin_kind;
      comp.lhs = ALLOC(expr);
      comp.rhs = ALLOC(right);

      expr = (struct Expr){.kind = EXPR_COMPOUND_ASSIGN,
                           .as.compound_assign = comp};

      printf("from compound assign: \n");
      print_expr(&expr, 0);
    }
  }

  return (struct ParseFnResult){.as.expr = expr, .is_ok = true, .msg = NULL};
}

struct ParseFnResult parse_expr(struct Parser *parser)
{
  struct ParseFnResult res;
  struct Expr expr;

  res.is_ok = false;
  res.msg = NULL;

  res = assignment(parser);
  if (!res.is_ok) {
    return res;
  }

  expr = res.as.expr;

  return (struct ParseFnResult){.is_ok = true, .msg = NULL, .as.expr = expr};
}

struct ParseFnResult block(struct Parser *parser)
{
  VecStmt stmts = {0};
  struct ParseFnResult result;
  struct Token *token_lbrace, *token_rbrace;

  result.is_ok = true;
  result.msg = NULL;

  token_lbrace = consume(parser, TOKEN_LBRACE);
  if (!token_lbrace) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected '{'", .as.stmt = {0}};
  }

  while (!check(parser, TOKEN_RBRACE)) {
    struct ParseFnResult r;

    r = parse_stmt(parser);
    if (!r.is_ok) {
      return r;
    }

    vec_insert(&stmts, r.as.stmt);
  }

  token_rbrace = consume(parser, TOKEN_RBRACE);
  if (!token_rbrace) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected '}'", .as.stmt = {0}};
  }

  struct Stmt block_stmt;
  block_stmt.kind = STMT_BLOCK;
  block_stmt.as.block.stmts = stmts;

  result.as.stmt = block_stmt;

  return result;
}

struct ParseFnResult parse_fn_stmt(struct Parser *parser)
{
  struct ParseFnResult result;
  struct Token *token_fn, *token_id, *token_lparen, *token_void, *token_rparen,
      *token_arrow;

  VecParam parameters = {0};

  result.is_ok = true;
  result.msg = NULL;

  token_fn = consume(parser, TOKEN_FN);
  if (!token_fn) {
    return (struct ParseFnResult){
        .is_ok = false, .as.stmt = {0}, .msg = "Expected token 'fn'"};
  }

  token_id = consume(parser, TOKEN_IDENTIFIER);
  if (!token_id) {
    return (struct ParseFnResult){.is_ok = false,
                                  .as.stmt = {0},
                                  .msg = "Expected token 'id' after 'fn'"};
  }

  token_lparen = consume(parser, TOKEN_LPAREN);
  if (!token_lparen) {
    return (struct ParseFnResult){
        .is_ok = false, .as.stmt = {0}, .msg = "Expected token '(' after 'id'"};
  }

  if (check(parser, TOKEN_VOID)) {
    token_void = consume(parser, TOKEN_VOID);
    if (!token_void) {
      return (struct ParseFnResult){.is_ok = false,
                                    .as.stmt = {0},
                                    .msg = "Expected token 'void' after '('"};
    }
  } else {
    while (!check(parser, TOKEN_RPAREN)) {
      struct Token *name_token, *semicolon_token;
      char *name;
      Type type;

      bool is_mut = match(parser, 1, TOKEN_MUT);

      name_token = consume(parser, TOKEN_IDENTIFIER);
      if (!name_token) {
        return (struct ParseFnResult){
            .is_ok = false,
            .as.stmt = {0},
            .msg = "Expected `name: type` format for parameters"};
      }

      semicolon_token = consume(parser, TOKEN_COLON);
      if (!semicolon_token) {
        return (struct ParseFnResult){
            .is_ok = false,
            .as.stmt = {0},
            .msg = "Expected `name: type` format for parameters"};
      }

      name = strndup(name_token->start, name_token->len);
      type = parse_type(parser);

      struct Parameter p;

      p.name = name;
      p.type = type;

      vec_insert(&parameters, p);

      if (check(parser, TOKEN_COMMA)) {
        consume(parser, TOKEN_COMMA);
      }
    }
  }

  token_rparen = consume(parser, TOKEN_RPAREN);
  if (!token_rparen) {
    return (struct ParseFnResult){.is_ok = false,
                                  .as.stmt = {0},
                                  .msg = "Expected token ')' after 'void'"};
  }

  token_arrow = consume(parser, TOKEN_ARROW);
  if (!token_arrow) {
    return (struct ParseFnResult){
        .is_ok = false, .as.stmt = {0}, .msg = "Expected token '->' after ')'"};
  }

  Type retval = parse_type(parser);

  struct StmtFn stmt_fn;
  stmt_fn.name = strndup(token_id->start, token_id->len);
  stmt_fn.params = parameters;
  stmt_fn.retval = retval;

  struct StmtFn *prev_fn = parser->current_fn;
  parser->current_fn = &stmt_fn;

  struct ParseFnResult block_result = block(parser);

  parser->current_fn = prev_fn;

  if (!block_result.is_ok) {
    return block_result;
  }

  struct Stmt body = block_result.as.stmt;

  stmt_fn.body = body.as.block.stmts;
  stmt_fn.retval = retval;

  struct Stmt stmt;
  stmt.kind = STMT_FN;
  stmt.as.fn = stmt_fn;

  result.as.stmt = stmt;

  return result;
}

struct ParseFnResult parse_let_stmt(struct Parser *parser)
{
  struct ParseFnResult result, init_res;
  struct Token *token_let, *token_id, *token_colon, *token_equal,
      *token_semicolon;
  Type type;
  struct Expr init;

  result.is_ok = true;
  result.msg = NULL;

  token_let = consume(parser, TOKEN_LET);
  if (!token_let) {
    return (struct ParseFnResult){
        .is_ok = false, .as.stmt = {0}, .msg = "Expected token 'let'"};
  }

  bool is_mut = match(parser, 1, TOKEN_MUT);

  token_id = consume(parser, TOKEN_IDENTIFIER);
  if (!token_id) {
    return (struct ParseFnResult){.is_ok = false,
                                  .as.stmt = {0},
                                  .msg = "Expected identifier after 'let'"};
  }

  char *name = strndup(token_id->start, token_id->len);

  token_colon = consume(parser, TOKEN_COLON);
  if (!token_colon) {
    free(name);
    return (struct ParseFnResult){
        .is_ok = false,
        .as.stmt = {0},
        .msg = "Expected token ':' after identifier in let stmt"};
  }

  type = parse_type(parser);

  token_equal = consume(parser, TOKEN_EQUAL);
  if (!token_equal) {
    free(name);
    return (struct ParseFnResult){
        .is_ok = false,
        .as.stmt = {0},
        .msg = "Expected token '=' after type in let stmt"};
  }

  init_res = parse_expr(parser);
  if (!init_res.is_ok) {
    free(name);
    return (struct ParseFnResult){
        .is_ok = false,
        .as.stmt = {0},
        .msg = "Expected expr after equal in let stmt"};
  }

  init = init_res.as.expr;

  token_semicolon = consume(parser, TOKEN_SEMICOLON);
  if (!token_semicolon) {
    free(name);
    return (struct ParseFnResult){
        .is_ok = false,
        .as.stmt = {0},
        .msg = "Expected semicolon after init expr in let stmt"};
  }

  struct StmtLet let_stmt;

  let_stmt.name = name;
  let_stmt.type = type;
  let_stmt.init = ALLOC(init);
  let_stmt.is_mut = is_mut;
  result.as.stmt = (struct Stmt){.kind = STMT_LET, .as.let = let_stmt};

  return result;
}

struct ParseFnResult parse_ret_stmt(struct Parser *parser)
{
  struct ParseFnResult result;
  struct Token *token_ret, *token_semicolon;
  struct Expr *expr;

  expr = NULL;

  result.is_ok = true;
  result.msg = NULL;

  token_ret = consume(parser, TOKEN_RET);
  if (!token_ret) {
    return (struct ParseFnResult){
        .is_ok = false, .as.stmt = {0}, .msg = "Expected token 'return'"};
  }

  if (check(parser, TOKEN_SEMICOLON)) {
    goto skip_parsing_expr;
  }

  struct ParseFnResult expr_res = parse_expr(parser);
  if (!expr_res.is_ok) {
    return (struct ParseFnResult){.is_ok = false,
                                  .as.stmt = {0},
                                  .msg = "Failed to parse expr after 'return'"};
  }

  expr = expr_res.is_ok ? ALLOC(expr_res.as.expr) : NULL;

skip_parsing_expr:
  token_semicolon = consume(parser, TOKEN_SEMICOLON);
  if (!token_semicolon) {
    return (struct ParseFnResult){
        .is_ok = false,
        .as.stmt = {0},
        .msg = "Expected token semicolon at the end of expr after 'return'"};
  }

  struct StmtRet ret_stmt;
  ret_stmt.val = expr;
  ret_stmt.expected_retval = parser->current_fn->retval;

  struct Stmt s;
  s.kind = STMT_RET;
  s.as.ret = ret_stmt;

  result.as.stmt = s;

  return result;
}

struct ParseFnResult parse_if_stmt(struct Parser *parser)
{
  struct ParseFnResult result, cond_res, then_res, else_res;
  struct Token *token_if, *token_lparen, *token_rparen, *token_else;
  struct Expr cond;
  struct Stmt *then_block, *else_block;

  result.is_ok = true;
  result.msg = NULL;

  token_if = consume(parser, TOKEN_IF);
  if (!token_if) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected 'if'", .as.stmt = {0}};
  }

  token_lparen = consume(parser, TOKEN_LPAREN);
  if (!token_lparen) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected '(' after 'if'", .as.stmt = {0}};
  }

  cond_res = parse_expr(parser);
  if (!cond_res.is_ok) {
    return cond_res;
  }

  cond = cond_res.as.expr;

  token_rparen = consume(parser, TOKEN_RPAREN);
  if (!token_rparen) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected ')' after 'if' cond", .as.stmt = {0}};
  }

  then_res = block(parser);
  if (!then_res.is_ok) {
    return then_res;
  }

  then_block = ALLOC(then_res.as.stmt);

  else_block = NULL;
  if (check(parser, TOKEN_ELSE)) {
    token_else = consume(parser, TOKEN_ELSE);
    if (!token_else) {
      return (struct ParseFnResult){
          .is_ok = false, .msg = "Expected 'else'", .as.stmt = {0}};
    }

    else_res = block(parser);
    if (!else_res.is_ok) {
      return else_res;
    }

    else_block = ALLOC(else_res.as.stmt);
  }

  struct StmtIf if_stmt;
  if_stmt.cond = cond;
  if_stmt.then_block = then_block;
  if_stmt.else_block = else_block;

  struct Stmt stmt;
  stmt.kind = STMT_IF;
  stmt.as.if_stmt = if_stmt;

  result.as.stmt = stmt;

  return result;
}

struct ParseFnResult parse_while_stmt(struct Parser *parser)
{
  struct ParseFnResult result, cond_result, body_result;
  struct Token *token_while, *token_lparen, *token_rparen;
  struct Expr cond;
  struct Stmt body;

  result.is_ok = true;
  result.msg = NULL;

  token_while = consume(parser, TOKEN_WHILE);
  if (!token_while) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected 'while'", .as.stmt = {0}};
  }

  token_lparen = consume(parser, TOKEN_LPAREN);
  if (!token_lparen) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected '(' after while", .as.stmt = {0}};
  }

  cond_result = parse_expr(parser);
  if (!cond_result.is_ok) {
    return cond_result;
  }

  cond = cond_result.as.expr;

  token_rparen = consume(parser, TOKEN_RPAREN);
  if (!token_rparen) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected '(' after while cond", .as.stmt = {0}};
  }

  body_result = block(parser);
  if (!body_result.is_ok) {
    return body_result;
  }

  body = body_result.as.stmt;

  struct StmtWhile while_stmt;
  while_stmt.cond = cond;
  while_stmt.body = ALLOC(body);

  struct Stmt s;
  s.kind = STMT_WHILE;
  s.as.while_stmt = while_stmt;

  result.as.stmt = s;

  return result;
}

struct ParseFnResult parse_loop_stmt(struct Parser *parser)
{
  struct ParseFnResult result, body_res;
  struct Token *token_loop;

  result.is_ok = true;
  result.msg = NULL;

  token_loop = consume(parser, TOKEN_LOOP);
  if (!token_loop) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected 'loop'", .as.stmt = {0}};
  }

  body_res = block(parser);
  if (!body_res.is_ok) {
    return body_res;
  }

  struct StmtLoop loop_stmt;
  loop_stmt.body = ALLOC(body_res.as.stmt);
  loop_stmt.label = "";

  struct Stmt s;
  s.kind = STMT_LOOP;
  s.as.loop = loop_stmt;

  result.as.stmt = s;

  return result;
}

struct ParseFnResult parse_break_stmt(struct Parser *parser)
{
  struct ParseFnResult result;
  struct Token *token_break, *token_semicolon;

  result.is_ok = true;
  result.msg = NULL;

  token_break = consume(parser, TOKEN_BREAK);
  if (!token_break) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected 'break'", .as.stmt = {0}};
  }

  token_semicolon = consume(parser, TOKEN_SEMICOLON);
  if (!token_semicolon) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected ';' after 'break'", .as.stmt = {0}};
  }

  struct StmtBreak break_stmt = {0};
  break_stmt.label = "";

  result.as.stmt =
      (struct Stmt){.kind = STMT_BREAK, .as.break_stmt = break_stmt};

  return result;
}

struct ParseFnResult parse_continue_stmt(struct Parser *parser)
{
  struct ParseFnResult result;
  struct Token *token_continue, *token_semicolon;

  result.is_ok = true;
  result.msg = NULL;

  token_continue = consume(parser, TOKEN_CONTINUE);
  if (!token_continue) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected 'break'", .as.stmt = {0}};
  }

  token_semicolon = consume(parser, TOKEN_SEMICOLON);
  if (!token_semicolon) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected ';' after 'continue'", .as.stmt = {0}};
  }

  struct StmtContinue continue_stmt = {0};
  continue_stmt.label = "";

  result.as.stmt =
      (struct Stmt){.kind = STMT_CONTINUE, .as.continue_stmt = continue_stmt};

  return result;
}

struct ParseFnResult parse_extern_stmt(struct Parser *parser)
{
  struct ParseFnResult result;
  struct Token *token_extern, *token_fn, *token_identifier, *token_lparen,
      *token_void, *token_rparen, *token_arrow, *token_semicolon;
  VecParam parameters = {0};
  bool is_variadic;

  is_variadic = false;

  result.is_ok = true;
  result.msg = NULL;

  token_extern = consume(parser, TOKEN_EXTERN);
  if (!token_extern) {
    return (struct ParseFnResult){
        .is_ok = false, .as.stmt = {0}, .msg = "Expected 'extern'"};
  }

  token_fn = consume(parser, TOKEN_FN);
  if (!token_fn) {
    return (struct ParseFnResult){
        .is_ok = false, .as.stmt = {0}, .msg = "Expected 'fn' after 'extern'"};
  }

  token_identifier = consume(parser, TOKEN_IDENTIFIER);
  if (!token_identifier) {
    return (struct ParseFnResult){
        .is_ok = false,
        .as.stmt = {0},
        .msg = "Expected identifier after 'fn' in 'extern' stmt"};
  }

  token_lparen = consume(parser, TOKEN_LPAREN);
  if (!token_lparen) {
    return (struct ParseFnResult){
        .is_ok = false,
        .msg = "Expected '(' after identifier in extern stmt",
        .as.stmt = {0}};
  }

  if (check(parser, TOKEN_VOID)) {
    token_void = consume(parser, TOKEN_VOID);
    if (!token_void) {
      return (struct ParseFnResult){.is_ok = false,
                                    .as.stmt = {0},
                                    .msg = "Expected token 'void' after '('"};
    }
  } else {
    while (!check(parser, TOKEN_RPAREN)) {
      if (check(parser, TOKEN_ELLIPSIS)) {
        consume(parser, TOKEN_ELLIPSIS);
        is_variadic = true;
        break;
      }

      struct Token *name_token, *colon_token;
      char *name;
      Type type;

      bool is_mut = match(parser, 1, TOKEN_MUT);

      name_token = consume(parser, TOKEN_IDENTIFIER);
      if (!name_token) {
        return (struct ParseFnResult){
            .is_ok = false,
            .as.stmt = {0},
            .msg = "Expected `name: type` format for parameters"};
      }

      colon_token = consume(parser, TOKEN_COLON);
      if (!colon_token) {
        return (struct ParseFnResult){
            .is_ok = false,
            .as.stmt = {0},
            .msg = "Expected `name: type` format for parameters"};
      }

      name = strndup(name_token->start, name_token->len);
      type = parse_type(parser);

      struct Parameter p;

      p.name = name;
      p.type = type;

      vec_insert(&parameters, p);

      if (check(parser, TOKEN_COMMA)) {
        consume(parser, TOKEN_COMMA);
      }
    }
  }

  token_rparen = consume(parser, TOKEN_RPAREN);
  if (!token_rparen) {
    return (struct ParseFnResult){.is_ok = false,
                                  .as.stmt = {0},
                                  .msg = "Expected token ')' after 'void'"};
  }

  token_arrow = consume(parser, TOKEN_ARROW);
  if (!token_arrow) {
    return (struct ParseFnResult){
        .is_ok = false, .as.stmt = {0}, .msg = "Expected token '->' after ')'"};
  }

  Type retval = parse_type(parser);

  token_semicolon = consume(parser, TOKEN_SEMICOLON);
  if (!token_semicolon) {
    return (struct ParseFnResult){
        .is_ok = false,
        .msg = "Expected ';' at the end of extern stmt",
        .as.stmt = {0}};
  }

  struct StmtExtern extern_stmt;
  extern_stmt.name = strndup(token_identifier->start, token_identifier->len);
  extern_stmt.params = parameters;
  extern_stmt.retval = retval;
  extern_stmt.is_variadic = is_variadic;

  struct Stmt s;
  s.kind = STMT_EXTERN;
  s.as.extern_stmt = extern_stmt;

  result.as.stmt = s;

  return result;
}

struct ParseFnResult parse_expr_stmt(struct Parser *parser)
{
  struct ParseFnResult result, expr_res;
  struct Token *token_semicolon;
  struct Expr expr;

  result.is_ok = true;
  result.msg = NULL;

  expr_res = parse_expr(parser);
  if (!expr_res.is_ok) {
    return expr_res;
  }

  expr = expr_res.as.expr;

  token_semicolon = consume(parser, TOKEN_SEMICOLON);
  if (!token_semicolon) {
    return (struct ParseFnResult){.is_ok = false,
                                  .msg = "Expected ';' at the end of expr stmt",
                                  .as.stmt = {0}};
  }

  struct StmtExpr expr_stmt;
  expr_stmt.expr = expr;

  struct Stmt s;
  s.kind = STMT_EXPR;
  s.as.expr_stmt = expr_stmt;

  result.as.stmt = s;

  return result;
}

struct ParseFnResult parse_struct_stmt(struct Parser *parser)
{
  struct ParseFnResult result;
  struct Token *token_struct_or_union, *token_id, *token_lbrace, *token_rbrace;
  VecStructField fields = {0};

  result.is_ok = true;
  result.msg = NULL;

  token_struct_or_union = consume_any(parser, 2, TOKEN_STRUCT, TOKEN_UNION);
  if (!token_struct_or_union) {
    return (struct ParseFnResult){
        .is_ok = false, .as.stmt = {0}, .msg = "Expected token 'struct'"};
  }

  bool is_union = (token_struct_or_union->kind == TOKEN_UNION);

  token_id = consume(parser, TOKEN_IDENTIFIER);
  if (!token_id) {
    return (struct ParseFnResult){.is_ok = false,
                                  .as.stmt = {0},
                                  .msg = "Expected identifier after 'struct'"};
  }

  char *name = strndup(token_id->start, token_id->len);

  token_lbrace = consume(parser, TOKEN_LBRACE);
  if (!token_lbrace) {
    free(name);
    return (struct ParseFnResult){.is_ok = false,
                                  .as.stmt = {0},
                                  .msg = "Expected '{' in struct declaration"};
  }

  while (!check(parser, TOKEN_RBRACE)) {
    char *field_name = NULL;
    Type field_type;

    /* Handle both `a: i8` and `union { ... } as;` gracefully */
    if (check(parser, TOKEN_IDENTIFIER) && parser->idx < parser->tokens->len &&
        parser->tokens->data[parser->idx].kind == TOKEN_COLON) {
      struct Token *name_tok = consume(parser, TOKEN_IDENTIFIER);
      consume(parser, TOKEN_COLON);
      field_type = parse_type(parser);
      field_name = strndup(name_tok->start, name_tok->len);
    }

    struct StructField field;
    field.name = field_name;
    field.type = field_type;
    field.offset = 0;
    vec_insert(&fields, field);

    if (check(parser, TOKEN_COMMA)) {
      advance_parser(parser);
    }
  }

  token_rbrace = consume(parser, TOKEN_RBRACE);
  if (!token_rbrace) {
    return (struct ParseFnResult){.is_ok = false,
                                  .as.stmt = {0},
                                  .msg = "Expected '}' after struct fields"};
  }

  struct StmtStruct struct_stmt;
  struct_stmt.name = name;
  struct_stmt.fields = fields;
  struct_stmt.is_union = is_union;

  result.as.stmt =
      (struct Stmt){.kind = STMT_STRUCT, .as.struct_stmt = struct_stmt};

  return result;
}

struct ParseFnResult parse_goto_stmt(struct Parser *parser)
{
  struct ParseFnResult result = {.is_ok = true, .msg = NULL};

  struct Token *token_goto, *token_label, *token_semicolon;

  token_goto = consume(parser, TOKEN_GOTO);
  if (!token_goto) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected 'goto'", .as.stmt = {0}};
  }

  token_label = consume(parser, TOKEN_IDENTIFIER);
  if (!token_label) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected ident after goto", .as.stmt = {0}};
  }

  token_semicolon = consume(parser, TOKEN_SEMICOLON);
  if (!token_semicolon) {
    return (struct ParseFnResult){
        .is_ok = false,
        .msg = "Expected ';' at the end of 'goto' stmt",
        .as.stmt = {0}};
  }

  struct StmtGoto goto_stmt;
  goto_stmt.label = strndup(token_label->start, token_label->len);

  struct Stmt s;
  s.kind = STMT_GOTO;
  s.as.goto_stmt = goto_stmt;

  result.as.stmt = s;

  return result;
}

struct ParseFnResult parse_enum_stmt(struct Parser *parser)
{
  struct ParseFnResult result = {.is_ok = true, .msg = NULL};

  consume(parser, TOKEN_ENUM);

  char *name;
  if (check(parser, TOKEN_IDENTIFIER)) {
    struct Token *id = consume(parser, TOKEN_IDENTIFIER);
    name = strndup(id->start, id->len);
  } else {
    name = mkuniq("anon_enum");
  }
  enum_type_insert(name);

  consume(parser, TOKEN_LBRACE);

  VecEnumVariant variants = {0};
  if (!parse_enum_body(parser, &variants)) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Failed to parse enum body", .as.stmt = {0}};
  }

  consume(parser, TOKEN_RBRACE);

  struct StmtEnum enum_stmt;
  enum_stmt.name = name;
  enum_stmt.variants = variants;

  result.as.stmt = (struct Stmt){.kind = STMT_ENUM, .as.enum_stmt = enum_stmt};
  return result;
}

struct ParseFnResult parse_stmt(struct Parser *parser)
{
  struct ParseFnResult result;

  result.is_ok = true;
  result.msg = NULL;

  if (check(parser, TOKEN_IDENTIFIER) && check_next(parser, TOKEN_COLON)) {
    struct Token *token_label, *token_colon;

    token_label = consume(parser, TOKEN_IDENTIFIER);
    if (!token_label) {
      return (struct ParseFnResult){
          .is_ok = false, .msg = "Expected label", .as.stmt = {0}};
    }

    token_colon = consume(parser, TOKEN_COLON);
    if (!token_colon) {
      return (struct ParseFnResult){
          .is_ok = false, .msg = "Expected ':' after label", .as.stmt = {0}};
    }

    char *label;

    label = strndup(token_label->start, token_label->len);

    struct ParseFnResult labeled_res;

    labeled_res = parse_stmt(parser);
    if (!labeled_res.is_ok) {
      return labeled_res;
    }

    struct StmtLabeled labeled;

    labeled.label = label;
    labeled.stmt = ALLOC(labeled_res.as.stmt);

    return (struct ParseFnResult){
        .is_ok = true,
        .msg = NULL,
        .as.stmt =
            ((struct Stmt){.kind = STMT_LABELED, .as.labeled = labeled})};
  }

  switch (parser->curr->kind) {
    case TOKEN_FN: {
      struct ParseFnResult fn_res = parse_fn_stmt(parser);
      if (!fn_res.is_ok) {
        return fn_res;
      }
      result.as.stmt = fn_res.as.stmt;
      break;
    }
    case TOKEN_LET: {
      struct ParseFnResult let_res = parse_let_stmt(parser);
      if (!let_res.is_ok) {
        return let_res;
      }
      result.as.stmt = let_res.as.stmt;
      break;
    }
    case TOKEN_RET: {
      struct ParseFnResult ret_res = parse_ret_stmt(parser);
      if (!ret_res.is_ok) {
        return ret_res;
      }
      result.as.stmt = ret_res.as.stmt;
      break;
    }
    case TOKEN_IF: {
      struct ParseFnResult if_res = parse_if_stmt(parser);
      if (!if_res.is_ok) {
        return if_res;
      }
      result.as.stmt = if_res.as.stmt;
      break;
    }
    case TOKEN_WHILE: {
      struct ParseFnResult while_res = parse_while_stmt(parser);
      if (!while_res.is_ok) {
        return while_res;
      }
      result.as.stmt = while_res.as.stmt;
      break;
    }
    case TOKEN_LOOP: {
      struct ParseFnResult loop_res = parse_loop_stmt(parser);
      if (!loop_res.is_ok) {
        return loop_res;
      }
      result.as.stmt = loop_res.as.stmt;
      break;
    }
    case TOKEN_BREAK: {
      struct ParseFnResult break_res = parse_break_stmt(parser);
      if (!break_res.is_ok) {
        return break_res;
      }
      result.as.stmt = break_res.as.stmt;
      break;
    }
    case TOKEN_CONTINUE: {
      struct ParseFnResult continue_res = parse_continue_stmt(parser);
      if (!continue_res.is_ok) {
        return continue_res;
      }
      result.as.stmt = continue_res.as.stmt;
      break;
    }
    case TOKEN_GOTO: {
      struct ParseFnResult goto_res = parse_goto_stmt(parser);
      if (!goto_res.is_ok) {
        return goto_res;
      }
      result.as.stmt = goto_res.as.stmt;
      break;
    }
    case TOKEN_EXTERN: {
      struct ParseFnResult extern_res = parse_extern_stmt(parser);
      if (!extern_res.is_ok) {
        return extern_res;
      }
      result.as.stmt = extern_res.as.stmt;
      break;
    }
    case TOKEN_LBRACE: {
      struct ParseFnResult block_res = block(parser);
      if (!block_res.is_ok) {
        return block_res;
      }
      result.as.stmt = block_res.as.stmt;
      break;
    }
    case TOKEN_STRUCT:
    case TOKEN_UNION: {
      struct ParseFnResult struct_res = parse_struct_stmt(parser);
      if (!struct_res.is_ok) {
        return struct_res;
      }
      result.as.stmt = struct_res.as.stmt;
      break;
    }
    case TOKEN_ENUM: {
      struct ParseFnResult enum_res = parse_enum_stmt(parser);
      if (!enum_res.is_ok) {
        return enum_res;
      }
      result.as.stmt = enum_res.as.stmt;
      break;
    }
    default: {
      struct ParseFnResult expr_stmt_res = parse_expr_stmt(parser);
      if (!expr_stmt_res.is_ok) {
        return expr_stmt_res;
      }
      result.as.stmt = expr_stmt_res.as.stmt;
      break;
    }
  }

  return result;
}

struct ParseResult parse(struct Parser *parser)
{
  struct ParseResult result;

  result.is_ok = true;
  result.msg = NULL;

  result.ast = malloc(sizeof(struct AST));
  result.ast->stmts = (VecStmt){0};

  parser->global_stmts = &result.ast->stmts;

  while (parser->curr) {
    struct ParseFnResult r;

    r = parse_stmt(parser);
    if (!r.is_ok) {
      return (struct ParseResult){.is_ok = false, .msg = r.msg, .ast = NULL};
    }
    vec_insert(&result.ast->stmts, r.as.stmt);
  }

  return result;
}

struct LoopLabelResult {
  bool is_ok;
  char *msg;
  struct AST *ast;
};

struct LoopLabelResult loop_label_stmt(struct Stmt *stmt, char *label)
{
  switch (stmt->kind) {
    case STMT_LOOP: {
      char *new_label;
      int tmp;

      tmp = mktmp();

      new_label = mklbl("Loop", tmp);

      stmt->as.loop.label = new_label;

      loop_label_stmt(stmt->as.loop.body, new_label);
      break;
    }
    case STMT_WHILE: {
      char *new_label;
      int tmp;

      tmp = mktmp();

      new_label = mklbl("While", tmp);

      stmt->as.while_stmt.label = new_label;

      loop_label_stmt(stmt->as.while_stmt.body, new_label);
      break;
    }
    case STMT_BREAK: {
      stmt->as.break_stmt.label = label;
      break;
    }
    case STMT_CONTINUE: {
      stmt->as.continue_stmt.label = label;
      break;
    }
    case STMT_LABELED: {
      struct LoopLabelResult r;

      r = loop_label_stmt(stmt->as.labeled.stmt, label);
      if (!r.is_ok) {
        return r;
      }

      break;
    }
    case STMT_FN: {
      for (int i = 0; i < stmt->as.fn.body.len; i++) {
        struct LoopLabelResult r;
        r = loop_label_stmt(&stmt->as.fn.body.data[i], label);
        if (!r.is_ok) {
          return r;
        }
      }
      break;
    }
    case STMT_IF: {
      struct LoopLabelResult then_res, else_res;

      then_res = loop_label_stmt(stmt->as.if_stmt.then_block, label);
      if (!then_res.is_ok) {
        return then_res;
      }

      if (stmt->as.if_stmt.else_block) {
        else_res = loop_label_stmt(stmt->as.if_stmt.else_block, label);
        if (!else_res.is_ok) {
          return else_res;
        }
      }
      break;
    }
    case STMT_BLOCK: {
      for (int i = 0; i < stmt->as.block.stmts.len; i++) {
        struct LoopLabelResult r;
        r = loop_label_stmt(&stmt->as.block.stmts.data[i], label);
        if (!r.is_ok) {
          return r;
        }
      }
      break;
    }
    case STMT_RET:
    case STMT_EXPR:
    case STMT_LET:
    case STMT_EXTERN:
    case STMT_STRUCT:
    case STMT_ENUM:
    case STMT_GOTO:
      break;
    default:
      assert(0);
  }
  return (struct LoopLabelResult){.is_ok = true, .msg = NULL};
}

struct LoopLabelResult loop_label(struct AST *ast)
{
  for (int i = 0; i < ast->stmts.len; i++) {
    struct LoopLabelResult r;
    r = loop_label_stmt(&ast->stmts.data[i], NULL);
    if (!r.is_ok) {
      return r;
    }
  }
  return (struct LoopLabelResult){.is_ok = true, .msg = NULL, .ast = ast};
}

typedef Vector(char *) VecCharPtr;

struct CollectLabelsResult {
  bool is_ok;
  char *msg;
  VecCharPtr labels;
  ;
};

struct CollectLabelsResult collect_labels_stmt(struct Stmt *stmt,
                                               VecCharPtr *labels,
                                               char *funcname)
{
  switch (stmt->kind) {
    case STMT_LABELED: {
      struct CollectLabelsResult r;
      char *label;

      label = mkstr("%s.%s", funcname, stmt->as.labeled.label);

      for (int i = 0; i < labels->len; i++) {
        if (strcmp(labels->data[i], label) == 0) {
          for (int i = 0; i < labels->len; i++) {
            free(labels->data[i]);
          }
          vec_free(labels);
          free(label);
          return (struct CollectLabelsResult){
              .is_ok = false, .msg = "Duplicate label", .labels = {0}};
        }
      }

      vec_insert(labels, label);

      r = collect_labels_stmt(stmt->as.labeled.stmt, labels, funcname);
      if (!r.is_ok) {
        return r;
      }
      break;
    }
    case STMT_FN: {
      for (int i = 0; i < stmt->as.fn.body.len; i++) {
        struct CollectLabelsResult r;

        r = collect_labels_stmt(&stmt->as.fn.body.data[i], labels,
                                stmt->as.fn.name);
        if (!r.is_ok) {
          return r;
        }
      }
      break;
    }
    case STMT_BLOCK: {
      for (int i = 0; i < stmt->as.block.stmts.len; i++) {
        struct CollectLabelsResult r;

        r = collect_labels_stmt(&stmt->as.block.stmts.data[i], labels,
                                funcname);
        if (!r.is_ok) {
          return r;
        }
      }
      break;
    }
    case STMT_IF: {
      struct CollectLabelsResult r1, r2;

      r1 = collect_labels_stmt(stmt->as.if_stmt.then_block, labels, funcname);
      if (!r1.is_ok) {
        return r1;
      }

      if (stmt->as.if_stmt.else_block) {
        r2 = collect_labels_stmt(stmt->as.if_stmt.else_block, labels, funcname);
        if (!r2.is_ok) {
          return r2;
        }
      }
      break;
    }
    case STMT_WHILE: {
      struct CollectLabelsResult r;

      r = collect_labels_stmt(stmt->as.while_stmt.body, labels, funcname);
      if (!r.is_ok) {
        return r;
      }

      break;
    }
    default:
      break;
  }
  return (struct CollectLabelsResult){.is_ok = true, .msg = NULL};
}

struct CollectLabelsResult collect_labels(struct AST *ast)
{
  VecCharPtr labels = {0};

  for (int i = 0; i < ast->stmts.len; i++) {
    struct CollectLabelsResult r;

    r = collect_labels_stmt(&ast->stmts.data[i], &labels, NULL);
    if (!r.is_ok) {
      for (int i = 0; i < labels.len; i++) {
        free(labels.data[i]);
      }
      vec_free(&labels);
      return r;
    }
  }

  for (int i = 0; i < labels.len; i++) {
    printf("found label: %s\n", labels.data[i]);
  }

  return (struct CollectLabelsResult){
      .is_ok = true, .msg = NULL, .labels = labels};
}

struct LabelCheckResult {
  bool is_ok;
  char *msg;
};

struct LabelCheckResult check_labels_stmt(struct Stmt *stmt, VecCharPtr *labels,
                                          char *funcname)
{
  switch (stmt->kind) {
    case STMT_FN: {
      for (int i = 0; i < stmt->as.fn.body.len; i++) {
        struct LabelCheckResult r;

        r = check_labels_stmt(&stmt->as.fn.body.data[i], labels,
                              stmt->as.fn.name);
        if (!r.is_ok) {
          return r;
        }
      }
      break;
    }
    case STMT_GOTO: {
      bool is_found;
      char *label;

      is_found = false;
      label = mkstr("%s.%s", funcname, stmt->as.goto_stmt.label);

      printf("labels->len is: %d\n", labels->len);
      for (int i = 0; i < labels->len; i++) {
        printf("label inside is: %s\n", labels->data[i]);
        if (strcmp(labels->data[i], label) == 0) {
          is_found = true;
          break;
        }
      }

      if (!is_found) {
        return (struct LabelCheckResult){
            .is_ok = false, .msg = mkstr("No label %s found", label)};
      }

      free(label);
      return (struct LabelCheckResult){.is_ok = true, .msg = NULL};
    }
    case STMT_LOOP: {
      struct LabelCheckResult body_res;

      body_res = check_labels_stmt(stmt->as.loop.body, labels, funcname);
      if (!body_res.is_ok) {
        return body_res;
      }

      break;
    }
    case STMT_IF: {
      struct LabelCheckResult then_res, else_res;

      then_res =
          check_labels_stmt(stmt->as.if_stmt.then_block, labels, funcname);
      if (!then_res.is_ok) {
        return then_res;
      }

      if (stmt->as.if_stmt.else_block) {
        else_res =
            check_labels_stmt(stmt->as.if_stmt.else_block, labels, funcname);
        if (!else_res.is_ok) {
          return else_res;
        }
      }

      break;
    }
    case STMT_WHILE: {
      struct LabelCheckResult body_res;

      body_res = check_labels_stmt(stmt->as.while_stmt.body, labels, funcname);
      if (!body_res.is_ok) {
        return body_res;
      }

      break;
    }
    case STMT_LABELED: {
      struct LabelCheckResult labeled_res;

      labeled_res = check_labels_stmt(stmt->as.labeled.stmt, labels, funcname);
      if (!labeled_res.is_ok) {
        return labeled_res;
      }

      break;
    }
    default:
      break;
  }
  return (struct LabelCheckResult){.is_ok = true, .msg = NULL};
}

struct LabelCheckResult check_labels(struct AST *ast, VecCharPtr *labels)
{
  for (int i = 0; i < ast->stmts.len; i++) {
    struct LabelCheckResult r;

    r = check_labels_stmt(&ast->stmts.data[i], labels, NULL);
    if (!r.is_ok) {
      return r;
    }
  }
  return (struct LabelCheckResult){.is_ok = true, .msg = NULL};
}

enum IRValueKind {
  IRValue_CONST,
  IRValue_VAR,
};

struct IRValue {
  enum IRValueKind kind;
  Type type;
  union {
    char *var;
    struct Literal konst;
  } as;
};

struct IRValue *clone_irval(struct IRValue *v)
{
  if (!v) {
    return NULL;
  }

  struct IRValue *clone;

  clone = malloc(sizeof(struct IRValue));
  clone->kind = v->kind;
  clone->type = clone_type(v->type);

  if (v->kind == IRValue_VAR) {
    clone->as.var = strdup(v->as.var);
  } else if (v->kind == IRValue_CONST) {
    clone->as.konst = v->as.konst;
  }

  return clone;
}

void print_ir_val(struct IRValue *ir_val, int spaces)
{
  switch (ir_val->kind) {
    case IRValue_CONST: {
      printf("IRValue(\n");

      print_indent(spaces + 2);
      printf("type = CONST,\n");

      print_indent(spaces + 2);

      switch (ir_val->type.kind) {
        case I8_T: {
          printf("v: %d,\n", ir_val->as.konst.as.i8);
          break;
        }
        case U8_T: {
          printf("v: %d,\n", ir_val->as.konst.as.u8);
          break;
        }
        case I16_T: {
          printf("v: %d,\n", ir_val->as.konst.as.i16);
          break;
        }
        case U16_T: {
          printf("v: %d,\n", ir_val->as.konst.as.u16);
          break;
        }
        case I32_T: {
          printf("v: %d,\n", ir_val->as.konst.as.i32);
          break;
        }
        case U32_T: {
          printf("v: %d,\n", ir_val->as.konst.as.u32);
          break;
        }
        case I64_T: {
          printf("v: %lld,\n", ir_val->as.konst.as.i64);
          break;
        }
        case U64_T: {
          printf("v: %llu,\n", ir_val->as.konst.as.u64);
          break;
        }
        case F32_T: {
          printf("v: %f,\n", ir_val->as.konst.as.f32);
          break;
        }
        case F64_T: {
          printf("v: %f,\n", ir_val->as.konst.as.f64);
          break;
        }
        case BOOL_T: {
          printf("v: %s,\n", ir_val->as.konst.as.boolean ? "true" : "false");
          break;
        }
        case STR_T: {
          printf("v: \"%s\",\n", ir_val->as.konst.as.str);
          break;
        }
        case PTR_T: {
          printf("v: <ptr>,\n");
          break;
        }
        default:
          assert(0);
      }

      print_indent(spaces);
      printf(")");
      break;
    }
    case IRValue_VAR: {
      printf("IRValue(\n");

      print_indent(spaces + 2);
      printf("type = VAR,\n");

      print_indent(spaces + 2);
      printf("name = %s,\n", ir_val->as.var);

      print_indent(spaces);
      printf(")");
      break;
    }
  }
}

void free_ir_val(struct IRValue *val)
{
  switch (val->kind) {
    case IRValue_CONST: {
      break;
    }
    case IRValue_VAR: {
      free(val->as.var);
      break;
    }
  }
  free(val);
}

enum IRInstrKind {
  IRInstr_BIN,
  IRInstr_UNARY,
  IRInstr_RET,
  IRInstr_CPY,
  IRInstr_CALL,
  IRInstr_JMP,
  IRInstr_JZ,
  IRInstr_LBL,
  IRInstr_GETADDR,
  IRInstr_LOAD,
  IRInstr_STORE,
  IRInstr_CAST,
  IRInstr_CPY_TO_OFFSET,
  IRInstr_CPY_FROM_OFFSET,
  IRInstr_ADD_PTR,
};

enum IRInstrBinaryKind {
  IRInstrBinary_ADD,
  IRInstrBinary_SUB,
  IRInstrBinary_MUL,
  IRInstrBinary_DIV,
  IRInstrBinary_E,
  IRInstrBinary_NE,
  IRInstrBinary_L,
  IRInstrBinary_LE,
  IRInstrBinary_G,
  IRInstrBinary_GE,
  IRInstrBinary_BIT_AND,
  IRInstrBinary_BIT_XOR,
  IRInstrBinary_BIT_OR,
  IRInstrBinary_SHL,
  IRInstrBinary_SHR,
  IRInstrBinary_SAR,
};

void print_ir_binary_op(enum IRInstrBinaryKind kind)
{
  switch (kind) {
    case IRInstrBinary_ADD:
      printf("ADD");
      break;
    case IRInstrBinary_SUB:
      printf("SUB");
      break;
    case IRInstrBinary_MUL:
      printf("MUL");
      break;
    case IRInstrBinary_DIV:
      printf("DIV");
      break;
    case IRInstrBinary_E:
      printf("EQUAL EQUAL");
      break;
    case IRInstrBinary_NE:
      printf("BANG EQUAL");
      break;
    case IRInstrBinary_L:
      printf("LESS");
      break;
    case IRInstrBinary_LE:
      printf("LESS EQUAL");
      break;
    case IRInstrBinary_G:
      printf("GREATER");
      break;
    case IRInstrBinary_GE:
      printf("GREATER EQUAL");
      break;
    case IRInstrBinary_BIT_AND:
      printf("BITWISE AND");
      break;
    case IRInstrBinary_BIT_XOR:
      printf("BITWISE XOR");
      break;
    case IRInstrBinary_BIT_OR:
      printf("BITWISE OR");
      break;
    case IRInstrBinary_SHL:
      printf("SHIFT LEFT");
      break;
    case IRInstrBinary_SHR:
      printf("SHIFT RIGHT LOGICAL");
      break;
    case IRInstrBinary_SAR:
      printf("SHIFT RIGHT ARITHMETIC");
      break;
    default:
      assert(0);
  }
}
enum IRInstrBinaryKind expr_bin_to_ir_bin(enum ExprBinKind kind, Type type)
{
  switch (kind) {
    case EXPR_BIN_ADD:
      return IRInstrBinary_ADD;
    case EXPR_BIN_SUB:
      return IRInstrBinary_SUB;
    case EXPR_BIN_MUL:
      return IRInstrBinary_MUL;
    case EXPR_BIN_DIV:
      return IRInstrBinary_DIV;
    case EXPR_BIN_EQUAL_EQUAL:
      return IRInstrBinary_E;
    case EXPR_BIN_BANG_EQUAL:
      return IRInstrBinary_NE;
    case EXPR_BIN_LESS:
      return IRInstrBinary_L;
    case EXPR_BIN_LESS_EQUAL:
      return IRInstrBinary_LE;
    case EXPR_BIN_GREATER:
      return IRInstrBinary_G;
    case EXPR_BIN_GREATER_EQUAL:
      return IRInstrBinary_GE;
    case EXPR_BIN_BITWISE_AND:
      return IRInstrBinary_BIT_AND;
    case EXPR_BIN_BITWISE_XOR:
      return IRInstrBinary_BIT_XOR;
    case EXPR_BIN_BITWISE_OR:
      return IRInstrBinary_BIT_OR;
    case EXPR_BIN_SHIFT_LEFT:
      return IRInstrBinary_SHL;
    case EXPR_BIN_SHIFT_RIGHT:
      switch (type.kind) {
        case U8_T:
        case U16_T:
        case U32_T:
        case U64_T:
          return IRInstrBinary_SHR;
        default:
          return IRInstrBinary_SAR;
      }
    default:
      assert(0 && "unhandled ExprBinKind in expr_bin_to_ir_bin");
  }
}

struct IRInstr_Binary {
  enum IRInstrBinaryKind kind;
  struct IRValue *lhs;
  struct IRValue *rhs;
  struct IRValue *dst;
};

enum IRInstrUnaryKind {
  IRInstrUnary_NEG,
  IRInstrUnary_NOT,
  IRInstrUnary_BIT_NOT,
};

struct IRInstr_Unary {
  enum IRInstrUnaryKind kind;
  struct IRValue *src;
  struct IRValue *dst;
};

struct IRInstr_Ret {
  struct IRValue *val;
};

struct IRInstr_Copy {
  struct IRValue *src;
  struct IRValue *dst;
};

typedef Vector(struct IRValue *) VecIRValuePtr;

struct IRInstr_Call {
  struct Expr target;
  VecIRValuePtr args;
  struct IRValue *dst;
};

struct IRInstr_Jump {
  char *target;
};

struct IRInstr_JumpIfZero {
  struct IRValue cond;
  char *target;
};

struct IRInstr_Label {
  char *name;
};

struct IRInstr_GetAddress {
  struct IRValue *src;
  struct IRValue *dst;
};

struct IRInstr_Load {
  struct IRValue *src;
  struct IRValue *dst;
};

struct IRInstr_Store {
  struct IRValue *val;
  struct IRValue *dst;
};

enum IRCastKind {
  IRCast_None,

  IRCast_Truncate,
  IRCast_SignExtend,
  IRCast_ZeroExtend,

  IRCast_FloatPromote,
  IRCast_FloatDemote,

  IRCast_IntToFloat,
  IRCast_IntToDouble,
  IRCast_FloatToInt,
  IRCast_DoubleToInt,

  IRCast_UIntToFloat,
  IRCast_UIntToDouble,
  IRCast_FloatToUInt,
  IRCast_DoubleToUInt,

  IRCast_PtrToInt,
  IRCast_IntToPtr,
  IRCast_Bitcast,
};

struct IRInstr_Cast {
  enum IRCastKind kind;
  struct IRValue *src;
  struct IRValue *dst;
};

struct IRInstr_CopyFromOffset {
  struct IRValue *src;
  int offset;
  struct IRValue *dst;
};

struct IRInstr_CopyToOffset {
  struct IRValue *dst;
  int offset;
  struct IRValue *src;
};

struct IRInstr_AddPtr {
  struct IRValue *ptr;
  struct IRValue *index;
  int scale;
  struct IRValue *dst;
};

struct IRInstr {
  enum IRInstrKind kind;
  union {
    struct IRInstr_Binary binary;
    struct IRInstr_Unary unary;
    struct IRInstr_Ret ret;
    struct IRInstr_Copy copy;
    struct IRInstr_Call call;
    struct IRInstr_Jump jmp;
    struct IRInstr_JumpIfZero jz;
    struct IRInstr_Label label;
    struct IRInstr_GetAddress getaddr;
    struct IRInstr_Load load;
    struct IRInstr_Store store;
    struct IRInstr_Cast cast;
    struct IRInstr_CopyFromOffset cpy_from_offset;
    struct IRInstr_CopyToOffset cpy_to_offset;
    struct IRInstr_AddPtr add_ptr;
  } as;
};

void print_ir_instr(struct IRInstr *instr, int spaces)
{
  print_indent(spaces);

  switch (instr->kind) {
    case IRInstr_LOAD: {
      printf("IRInstr_LOAD(\n");
      print_indent(spaces + 2);
      printf("src = ");
      print_ir_val(instr->as.load.src, spaces + 2);
      printf(",\n");
      print_indent(spaces + 2);
      printf("dst = ");
      print_ir_val(instr->as.load.dst, spaces + 2);
      printf("\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case IRInstr_STORE: {
      printf("IRInstr_STORE(\n");
      print_indent(spaces + 2);
      printf("val = ");
      print_ir_val(instr->as.store.val, spaces + 2);
      printf(",\n");
      print_indent(spaces + 2);
      printf("dst = ");
      print_ir_val(instr->as.store.dst, spaces + 2);
      printf("\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case IRInstr_GETADDR: {
      printf("IRInstr_GETADDR(\n");
      print_indent(spaces + 2);
      printf("src = ");
      print_ir_val(instr->as.getaddr.src, spaces + 2);
      printf(",\n");
      print_indent(spaces + 2);
      printf("dst = ");
      print_ir_val(instr->as.getaddr.dst, spaces + 2);
      printf("\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case IRInstr_CAST: {
      printf("IRInstr_CAST(\n");
      print_indent(spaces + 2);
      printf("src = ");
      print_ir_val(instr->as.cast.src, spaces + 2);
      printf(",\n");
      print_indent(spaces + 2);
      printf("dst = ");
      print_ir_val(instr->as.cast.dst, spaces + 2);
      printf("\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case IRInstr_UNARY: {
      printf("IRInstr_UNARY(\n");
      print_indent(spaces + 2);
      if (instr->as.unary.kind == IRInstrUnary_NOT) {
        printf("kind = NOT,\n");
      } else {
        printf("kind = NEG,\n");
      }
      print_indent(spaces + 2);
      printf("src = ");
      print_ir_val(instr->as.unary.src, spaces + 2);
      printf(",\n");
      print_indent(spaces + 2);
      printf("dst = ");
      print_ir_val(instr->as.unary.dst, spaces + 2);
      printf("\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case IRInstr_JMP: {
      printf("IRInstr_JMP(target = %s)", instr->as.jmp.target);
      break;
    }
    case IRInstr_JZ: {
      printf("IRInstr_JZ(\n");
      print_indent(spaces + 2);
      printf("target = %s,\n", instr->as.jz.target);
      print_indent(spaces + 2);
      printf("cond: ");
      print_ir_val(&instr->as.jz.cond, spaces + 2);
      printf(",\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case IRInstr_LBL: {
      printf("IRInstr_LBL(name = %s)", instr->as.label.name);
      break;
    }
    case IRInstr_CPY: {
      printf("IRInstr_CPY(\n");

      print_indent(spaces + 2);
      printf("src = ");
      print_ir_val(instr->as.copy.src, spaces + 2);
      printf(",\n");

      print_indent(spaces + 2);
      printf("dst = ");
      print_ir_val(instr->as.copy.dst, spaces + 2);
      printf("\n");

      print_indent(spaces);
      printf(")");
      break;
    }
    case IRInstr_BIN: {
      printf("IRInstr_BIN(\n");

      print_indent(spaces + 2);
      printf("type = ");
      print_ir_binary_op(instr->as.binary.kind);
      printf(",\n");

      print_indent(spaces + 2);
      printf("lhs = ");
      print_ir_val(instr->as.binary.lhs, spaces + 2);
      printf(",\n");

      print_indent(spaces + 2);
      printf("rhs = ");
      print_ir_val(instr->as.binary.rhs, spaces + 2);
      printf(",\n");

      print_indent(spaces + 2);
      printf("dst = ");
      print_ir_val(instr->as.binary.dst, spaces + 2);
      printf("\n");

      print_indent(spaces);
      printf(")");
      break;
    }
    case IRInstr_RET: {
      printf("IRInstr_RET(\n");

      print_indent(spaces + 2);
      printf("val = ");
      if (instr->as.ret.val) {
        print_ir_val(instr->as.ret.val, spaces + 2);
      }
      printf("\n");

      print_indent(spaces);
      printf(")");
      break;
    }
    case IRInstr_CALL: {
      printf("IRInstr_CALL(\n");

      print_indent(spaces + 2);
      printf("target = ");
      print_expr(&instr->as.call.target, spaces + 2);
      printf(",\n");

      print_indent(spaces + 2);
      printf("args: [\n");
      for (int k = 0; k < instr->as.call.args.len; k++) {
        print_indent(spaces + 4);
        print_ir_val(instr->as.call.args.data[k], spaces + 4);
        printf(",\n");
      }
      print_indent(spaces + 2);
      printf("],\n");

      print_indent(spaces + 2);
      printf("dst: ");
      if (instr->as.call.dst) {
        print_ir_val(instr->as.call.dst, spaces + 2);
      } else {
        printf("NULL");
      }
      printf("\n");

      print_indent(spaces);
      printf(")");
      break;
    }
    case IRInstr_CPY_FROM_OFFSET: {
      printf("IRInstr_CPY_FROM_OFFSET(\n");
      print_indent(spaces + 2);
      printf("src = ");
      print_ir_val(instr->as.cpy_from_offset.src, spaces + 2);
      printf(",\n");
      print_indent(spaces + 2);
      printf("offset = %d,\n", instr->as.cpy_from_offset.offset);
      print_indent(spaces + 2);
      printf("dst = ");
      print_ir_val(instr->as.cpy_from_offset.dst, spaces + 2);
      printf("\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case IRInstr_CPY_TO_OFFSET: {
      printf("IRInstr_CPY_TO_OFFSET(\n");
      print_indent(spaces + 2);
      printf("dst = ");
      print_ir_val(instr->as.cpy_to_offset.dst, spaces + 2);
      printf(",\n");
      print_indent(spaces + 2);
      printf("offset = %d,\n", instr->as.cpy_to_offset.offset);
      print_indent(spaces + 2);
      printf("src = ");
      print_ir_val(instr->as.cpy_to_offset.src, spaces + 2);
      printf("\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case IRInstr_ADD_PTR: {
      printf("IRInstr_ADD_PTR(\n");
      print_indent(spaces + 2);
      printf("ptr = ");
      print_ir_val(instr->as.add_ptr.ptr, spaces + 2);
      printf(",\n");
      print_indent(spaces + 2);
      printf("index = ");
      print_ir_val(instr->as.add_ptr.index, spaces + 2);
      printf(",\n");
      print_indent(spaces + 2);
      printf("scale = %d,\n", instr->as.add_ptr.scale);
      print_indent(spaces + 2);
      printf("dst = ");
      print_ir_val(instr->as.add_ptr.dst, spaces + 2);
      printf("\n");
      print_indent(spaces);
      printf(")");
      break;
    }
  }
}

void free_ir_instr(struct IRInstr *instr)
{
  switch (instr->kind) {
    case IRInstr_UNARY: {
      free_ir_val(instr->as.unary.src);
      free_ir_val(instr->as.unary.dst);
      break;
    }
    case IRInstr_BIN: {
      free_ir_val(instr->as.binary.lhs);
      free_ir_val(instr->as.binary.rhs);
      free_ir_val(instr->as.binary.dst);
      break;
    }
    case IRInstr_RET: {
      if (instr->as.ret.val) {
        free_ir_val(instr->as.ret.val);
      }
      break;
    }
    case IRInstr_CPY: {
      free_ir_val(instr->as.copy.src);
      free_ir_val(instr->as.copy.dst);
      break;
    }
    case IRInstr_CALL: {
      for (int i = 0; i < instr->as.call.args.len; i++) {
        free_ir_val(instr->as.call.args.data[i]);
      }
      vec_free(&instr->as.call.args);

      if (instr->as.call.dst) {
        free_ir_val(instr->as.call.dst);
      }
      break;
    }
    case IRInstr_JMP: {
      free(instr->as.jmp.target);
      break;
    }
    case IRInstr_JZ: {
      free(instr->as.jz.target);
      if (instr->as.jz.cond.kind == IRValue_VAR) {
        free(instr->as.jz.cond.as.var);
      }
      break;
    }
    case IRInstr_LBL: {
      free(instr->as.label.name);
      break;
    }
    case IRInstr_GETADDR: {
      free_ir_val(instr->as.getaddr.src);
      free_ir_val(instr->as.getaddr.dst);
      break;
    }
    case IRInstr_CAST: {
      free_ir_val(instr->as.cast.src);
      free_ir_val(instr->as.cast.dst);
      break;
    }
    case IRInstr_LOAD: {
      free_ir_val(instr->as.load.src);
      free_ir_val(instr->as.load.dst);
      break;
    }
    case IRInstr_STORE: {
      free_ir_val(instr->as.store.dst);
      free_ir_val(instr->as.store.val);
      break;
    }
    case IRInstr_CPY_FROM_OFFSET: {
      free_ir_val(instr->as.cpy_from_offset.src);
      free_ir_val(instr->as.cpy_from_offset.dst);
      break;
    }
    case IRInstr_CPY_TO_OFFSET: {
      free_ir_val(instr->as.cpy_to_offset.dst);
      free_ir_val(instr->as.cpy_to_offset.src);
      break;
    }
    case IRInstr_ADD_PTR: {
      free_ir_val(instr->as.add_ptr.ptr);
      free_ir_val(instr->as.add_ptr.index);
      free_ir_val(instr->as.add_ptr.dst);
      break;
    }
    default:
      assert(0 && "Unhandled IR instruction in free_ir_instr");
  }
}
typedef Vector(struct IRInstr) VecIRInstr;

struct IRFunction {
  char *name;
  VecParam params;
  Type retval;
  VecIRInstr body;
};

void print_ir_fn(struct IRFunction *func)
{
  printf("IRFunction(\n  name = %s,\n  retval = %d,\n  body = [\n", func->name,
         func->retval.kind);
  for (int i = 0; i < func->body.len; i++) {
    print_ir_instr(&func->body.data[i], 4);
    printf(",\n");
  }
  printf("  ]\n");
  printf(")\n");
}

void free_ir_fn(struct IRFunction *func)
{
  for (int i = 0; i < func->body.len; i++) {
    free_ir_instr(&func->body.data[i]);
  }
  vec_free(&func->body);
  free(func);
}

typedef Vector(struct IRFunction *) VecIRFunctionPtr;

struct IRProgram {
  VecIRFunctionPtr funcs;
};

void print_ir(struct IRProgram *prog)
{
  for (int i = 0; i < prog->funcs.len; i++) {
    print_ir_fn(prog->funcs.data[i]);
  }
}

void free_ir_prog(struct IRProgram *prog)
{
  for (int i = 0; i < prog->funcs.len; i++) {
    free_ir_fn(prog->funcs.data[i]);
  }
  vec_free(&prog->funcs);
}

struct IRValue *mkirvar(void)
{
  struct IRValue *var;
  int i;

  i = mktmp();

  var = malloc(sizeof(struct IRValue));
  var->kind = IRValue_VAR;
  var->as.var = mkstr("tmp.%d", i);

  return var;
}

/*
 * ExpResult describes what an expression refers to during IR lowering.
 *
 * Some expressions already produce a normal IR value:
 *   x, 42, foo()
 * These use EXPRESULT_PLAIN.
 *
 * Some expressions describe a memory location instead of immediately loading
 * it: *ptr        -> EXPRESULT_DEREF point.x     -> EXPRESULT_SUBOBJECT
 *
 * Keeping these as locations is important because the caller may want to:
 *   - read from them,
 *   - assign to them,
 *   - or take their address.
 *
 * For example:
 *   point.x        can become a load from (point + field_offset)
 *   point.x = 10   can become a store to (point + field_offset)
 *   &point.x       can become the address (point + field_offset)
 *
 * SUBOBJECT stores:
 *   - the base aggregate object,
 *   - the byte offset of the field within that object,
 *   - and the base object's type, so later lowering still knows its layout.
 */
enum ExpResultKind {
  EXPRESULT_PLAIN,
  EXPRESULT_DEREF,
  EXPRESULT_SUBOBJECT,
};

struct ExpResult {
  enum ExpResultKind kind;
  union {
    struct IRValue *plain;
    struct IRValue *ptr;
    struct {
      char *base;
      size_t offset;
      Type base_type;
    } subobject;
  } as;
};

struct ExpResult irfy_expr(VecIRInstr *instrs, struct Expr *expr);

struct IRValue *irfy_expr_and_convert(VecIRInstr *instrs, struct Expr *expr)
{
  /* evaluates an expr and forces lvalue2rvalue conversion. */
  struct ExpResult result;

  result = irfy_expr(instrs, expr);
  switch (result.kind) {
    case EXPRESULT_PLAIN:
      /* When we evaluate a purely mathematical AST node, like `1 + 2`, the
       * result of `irfy_expr` is `EXPRESULT_PLAIN`, which means that the result
       * is already a concrete value or a temporary register holding that value.
       *
       * That's it, we do not need to do anything else to use the value in
       * further IR generation pass.  */
      return result.as.plain;
    case EXPRESULT_DEREF: {
      /* When we evaluate e.g. `y = *x + 5`, that is a ptr deref, `irfy_expr`
       * returns `EXPRESULT_DEREF`, which gives us a memory address of that
       * variable, NOT the data inside it.
       *
       * Since the mem address won't do any good and we can't add it to `5` in
       * this case, we need the VALUE sitting at that memory address.  So, we
       * create a new IR variable, `dst`, and do:  `dst = load(src_ptr)`.
       *
       * Then we clone (is all this cloning necessary?) and return `dst`. */
      struct IRValue *dst = mkirvar();
      dst->type = expr->type;

      struct IRInstr instr;
      instr.kind = IRInstr_LOAD;
      instr.as.load.src = result.as.ptr;
      instr.as.load.dst = dst;
      vec_insert(instrs, instr);

      struct IRValue *ret = malloc(sizeof(struct IRValue));
      ret->kind = IRValue_VAR;
      ret->as.var = strdup(dst->as.var);
      ret->type = dst->type;

      return ret;
    }
    case EXPRESULT_SUBOBJECT: {
      struct IRValue *dst = mkirvar();
      dst->type = expr->type;

      struct IRValue *base_var = malloc(sizeof(struct IRValue));
      base_var->kind = IRValue_VAR;
      base_var->as.var = strdup(result.as.subobject.base);
      base_var->type = result.as.subobject.base_type;

      struct IRInstr_CopyFromOffset copy_from = {
          .src = base_var,
          .offset = result.as.subobject.offset,
          .dst = clone_irval(dst)};

      struct IRInstr instr;
      instr.kind = IRInstr_CPY_FROM_OFFSET;
      instr.as.cpy_from_offset = copy_from;
      vec_insert(instrs, instr);

      return dst;
    }
  }

  assert(0);
  return NULL;
}

struct StaticConstant {
  char *name;
  char *value;
};

typedef Vector(struct StaticConstant) VecStaticConstant;
VecStaticConstant global_constants = {0};

void free_global_constants(void)
{
  for (int i = 0; i < global_constants.len; i++) {
    free(global_constants.data[i].name);
    free(global_constants.data[i].value);
  }
  vec_free(&global_constants);
}

static inline int get_type_size(Type t)
{
  switch (t.kind) {
    case I8_T:
    case U8_T:
    case BOOL_T:
      return 1;
    case I16_T:
    case U16_T:
      return 2;
    case I32_T:
    case U32_T:
    case F32_T:
      return 4;
    case I64_T:
    case U64_T:
    case F64_T:
    case STR_T:
    case PTR_T:
      return 8;
    case STRUCT_T: {
      struct StructDef *def = struct_get(struct_table, t.as.struct_name);
      return def->size;
    }
    default:
      return -1;
  }
}

bool is_unsigned(enum TypeKind kind)
{
  switch (kind) {
    case U8_T:
    case U16_T:
    case U32_T:
    case U64_T:
      return true;
    default:
      return false;
  }
}

bool is_integer_type(enum TypeKind kind)
{
  switch (kind) {
    case I8_T:
    case I16_T:
    case I32_T:
    case I64_T:
    case U8_T:
    case U16_T:
    case U32_T:
    case U64_T:
      return true;
    default:
      return false;
  }
}

enum IRCastKind get_cast_kind(Type src, Type dst)
{
  if (types_equal(src, dst)) {
    return IRCast_None;
  }

  bool src_is_float = (src.kind == F32_T || src.kind == F64_T);
  bool dst_is_float = (dst.kind == F32_T || dst.kind == F64_T);

  if (src_is_float && dst_is_float) {
    return src.kind == F32_T ? IRCast_FloatPromote : IRCast_FloatDemote;
  }

  if (src.kind == PTR_T && is_integer_type(dst.kind)) {
    return IRCast_PtrToInt;
  }
  if (is_integer_type(src.kind) && dst.kind == PTR_T) {
    return IRCast_IntToPtr;
  }
  if (src.kind == PTR_T && dst.kind == PTR_T) {
    return IRCast_Bitcast;
  }

  bool src_unsigned = is_unsigned(src.kind) || src.kind == BOOL_T;
  bool dst_unsigned = is_unsigned(dst.kind) || dst.kind == BOOL_T;

  if (src_is_float && !dst_is_float) {
    if (dst_unsigned) {
      return src.kind == F32_T ? IRCast_FloatToUInt : IRCast_DoubleToUInt;
    } else {
      return src.kind == F32_T ? IRCast_FloatToInt : IRCast_DoubleToInt;
    }
  }

  if (!src_is_float && dst_is_float) {
    if (src_unsigned) {
      return dst.kind == F32_T ? IRCast_UIntToFloat : IRCast_UIntToDouble;
    } else {
      return dst.kind == F32_T ? IRCast_IntToFloat : IRCast_IntToDouble;
    }
  }

  int src_sz = get_type_size(src);
  int dst_sz = get_type_size(dst);

  if (src_sz == dst_sz) {
    return IRCast_Bitcast;
  }
  if (src_sz > dst_sz) {
    return IRCast_Truncate;
  }
  return src_unsigned ? IRCast_ZeroExtend : IRCast_SignExtend;
}

struct ExpResult irfy_expr(VecIRInstr *instrs, struct Expr *expr)
{
  switch (expr->kind) {
    case EXPR_LITERAL: {
      if (expr->as.literal.kind == LITERAL_STR) {
        char *name;
        struct IRValue *src, *dst;

        name = mklbl("str", mktmp());

        struct StaticConstant sc;
        sc.name = strdup(name);
        sc.value = strdup(expr->as.literal.as.str);
        vec_insert(&global_constants, sc);

        src = malloc(sizeof(struct IRValue));
        src->kind = IRValue_VAR;
        src->as.var = strdup(name);
        src->type = expr->type;

        dst = mkirvar();
        dst->type = expr->type;

        struct IRInstr instr;
        instr.kind = IRInstr_GETADDR;
        instr.as.getaddr.src = src;
        instr.as.getaddr.dst = clone_irval(dst);
        vec_insert(instrs, instr);

        free(name);

        return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = dst};
      } else {
        struct IRValue *ir_val = malloc(sizeof(struct IRValue));
        ir_val->kind = IRValue_CONST;
        ir_val->as.konst = expr->as.literal;
        ir_val->type = expr->type;

        return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = ir_val};
      }
    }
    case EXPR_VARIABLE: {
      struct IRValue *r = malloc(sizeof(struct IRValue));
      r->kind = IRValue_VAR;
      r->as.var = strdup(expr->as.var.name);
      r->type = expr->type;

      return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = r};
    }
    case EXPR_UNARY: {
      struct IRValue *src, *dst;

      /* Force lvalue-to-rvalue conversion for inner expr.  */
      src = irfy_expr_and_convert(instrs, expr->as.unary.expr);

      dst = mkirvar();
      dst->type = expr->type;

      enum IRInstrUnaryKind kind;
      if (expr->as.unary.op[0] == '!') {
        kind = IRInstrUnary_NOT;
      } else if (expr->as.unary.op[0] == '~') {
        kind = IRInstrUnary_BIT_NOT;
      } else {
        kind = IRInstrUnary_NEG;
      }

      struct IRInstr_Unary iu;
      iu.kind = kind;
      iu.src = src;
      iu.dst = clone_irval(dst);

      struct IRInstr i;
      i.kind = IRInstr_UNARY;
      i.as.unary = iu;
      vec_insert(instrs, i);

      return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = dst};
    }
    case EXPR_BINARY: {
      if (expr->as.binary.kind == EXPR_BIN_LOGICAL_AND) {
        struct IRValue *lhs, *rhs, *dst, *one, *zero, *dst_zero;
        char *lbl_false, *lbl_end;
        int tmp;

        dst = mkirvar();
        dst->type = (Type){.kind = BOOL_T};

        tmp = mktmp();
        lbl_false = mklbl("AndFalse", tmp);
        lbl_end = mklbl("AndEnd", tmp);

        lhs = irfy_expr_and_convert(instrs, expr->as.binary.lhs);
        vec_insert(instrs,
                   ((struct IRInstr){
                       .kind = IRInstr_JZ,
                       .as.jz = {.cond = *lhs, .target = strdup(lbl_false)}}));

        rhs = irfy_expr_and_convert(instrs, expr->as.binary.rhs);
        vec_insert(instrs,
                   ((struct IRInstr){
                       .kind = IRInstr_JZ,
                       .as.jz = {.cond = *rhs, .target = strdup(lbl_false)}}));

        one = malloc(sizeof(struct IRValue));
        one->kind = IRValue_CONST;
        one->type = (Type){.kind = BOOL_T};
        one->as.konst.kind = LITERAL_BOOL;
        one->as.konst.type = (Type){.kind = BOOL_T};
        one->as.konst.as.boolean = true;

        vec_insert(instrs,
                   ((struct IRInstr){
                       .kind = IRInstr_CPY,
                       .as.copy = {.src = one, .dst = clone_irval(dst)}}));
        vec_insert(instrs,
                   ((struct IRInstr){.kind = IRInstr_JMP,
                                     .as.jmp = {.target = strdup(lbl_end)}}));
        vec_insert(instrs,
                   ((struct IRInstr){.kind = IRInstr_LBL,
                                     .as.label = {.name = strdup(lbl_false)}}));

        zero = malloc(sizeof(struct IRValue));
        zero->kind = IRValue_CONST;
        zero->type = (Type){.kind = BOOL_T};
        zero->as.konst.kind = LITERAL_BOOL;
        zero->as.konst.type = (Type){.kind = BOOL_T};
        zero->as.konst.as.boolean = false;

        dst_zero = clone_irval(dst);

        vec_insert(instrs, ((struct IRInstr){
                               .kind = IRInstr_CPY,
                               .as.copy = {.src = zero, .dst = dst_zero}}));
        vec_insert(instrs,
                   ((struct IRInstr){.kind = IRInstr_LBL,
                                     .as.label = {.name = strdup(lbl_end)}}));

        free(lbl_false);
        free(lbl_end);

        return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = dst};
      }

      if (expr->as.binary.kind == EXPR_BIN_LOGICAL_OR) {
        struct IRValue *lhs, *rhs, *dst, *one, *zero, *dst_zero;
        int tmp;
        char *lbl_check_rhs, *lbl_true, *lbl_false, *lbl_end;

        dst = mkirvar();
        dst->type = (Type){.kind = BOOL_T};

        tmp = mktmp();
        lbl_check_rhs = mklbl("OrRhs", tmp);
        lbl_true = mklbl("OrTrue", tmp);
        lbl_false = mklbl("OrFalse", tmp);
        lbl_end = mklbl("OrEnd", tmp);

        lhs = irfy_expr_and_convert(instrs, expr->as.binary.lhs);
        vec_insert(
            instrs,
            ((struct IRInstr){
                .kind = IRInstr_JZ,
                .as.jz = {.cond = *lhs, .target = strdup(lbl_check_rhs)}}));
        vec_insert(instrs,
                   ((struct IRInstr){.kind = IRInstr_JMP,
                                     .as.jmp = {.target = strdup(lbl_true)}}));
        vec_insert(instrs, ((struct IRInstr){
                               .kind = IRInstr_LBL,
                               .as.label = {.name = strdup(lbl_check_rhs)}}));

        rhs = irfy_expr_and_convert(instrs, expr->as.binary.rhs);
        vec_insert(instrs,
                   ((struct IRInstr){
                       .kind = IRInstr_JZ,
                       .as.jz = {.cond = *rhs, .target = strdup(lbl_false)}}));

        vec_insert(instrs,
                   ((struct IRInstr){.kind = IRInstr_LBL,
                                     .as.label = {.name = strdup(lbl_true)}}));

        one = malloc(sizeof(struct IRValue));
        one->kind = IRValue_CONST;
        one->type = (Type){.kind = BOOL_T};
        one->as.konst.kind = LITERAL_BOOL;
        one->as.konst.type = (Type){.kind = BOOL_T};
        one->as.konst.as.boolean = true;

        vec_insert(instrs,
                   ((struct IRInstr){
                       .kind = IRInstr_CPY,
                       .as.copy = {.src = one, .dst = clone_irval(dst)}}));
        vec_insert(instrs,
                   ((struct IRInstr){.kind = IRInstr_JMP,
                                     .as.jmp = {.target = strdup(lbl_end)}}));
        vec_insert(instrs,
                   ((struct IRInstr){.kind = IRInstr_LBL,
                                     .as.label = {.name = strdup(lbl_false)}}));

        zero = malloc(sizeof(struct IRValue));
        zero->kind = IRValue_CONST;
        zero->type = (Type){.kind = BOOL_T};
        zero->as.konst.kind = LITERAL_BOOL;
        zero->as.konst.type = (Type){.kind = BOOL_T};
        zero->as.konst.as.boolean = false;

        dst_zero = clone_irval(dst);

        vec_insert(instrs, ((struct IRInstr){
                               .kind = IRInstr_CPY,
                               .as.copy = {.src = zero, .dst = dst_zero}}));
        vec_insert(instrs,
                   ((struct IRInstr){.kind = IRInstr_LBL,
                                     .as.label = {.name = strdup(lbl_end)}}));

        free(lbl_check_rhs);
        free(lbl_true);
        free(lbl_false);
        free(lbl_end);

        return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = dst};
      }

      struct IRValue *lhs, *rhs, *dst;

      lhs = irfy_expr_and_convert(instrs, expr->as.binary.lhs);
      rhs = irfy_expr_and_convert(instrs, expr->as.binary.rhs);

      dst = mkirvar();
      dst->type = expr->type;

      enum IRInstrBinaryKind kind;
      kind = expr_bin_to_ir_bin(expr->as.binary.kind, expr->type);

      struct IRInstr_Binary bininstr = {
          .lhs = lhs, .rhs = rhs, .dst = clone_irval(dst), .kind = kind};
      struct IRInstr instr;
      instr.kind = IRInstr_BIN;
      instr.as.binary = bininstr;
      vec_insert(instrs, instr);

      return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = dst};
    }
    case EXPR_ASSIGN: {
      struct ExpResult lhs_res;
      struct IRValue *rhs_val;

      /* Keep lhs as lvalue.  */
      lhs_res = irfy_expr(instrs, expr->as.assign.lhs);

      /* Force lvalue-to-rvalue conversion for rhs.  */
      rhs_val = irfy_expr_and_convert(instrs, expr->as.assign.rhs);

      /* If lhs is a plain operand, emit just a cpy.  */
      if (lhs_res.kind == EXPRESULT_PLAIN) {
        struct IRInstr instr = {0};
        instr.kind = IRInstr_CPY;
        instr.as.copy = (struct IRInstr_Copy){
            .src = rhs_val, .dst = clone_irval(lhs_res.as.plain)};
        vec_insert(instrs, instr);

        return (struct ExpResult){.kind = EXPRESULT_PLAIN,
                                  .as.plain = lhs_res.as.plain};
      } else if (lhs_res.kind == EXPRESULT_DEREF) {
        /* ...otherwise, emit a store.  */
        struct IRInstr instr = {0};
        instr.kind = IRInstr_STORE;
        instr.as.store = (struct IRInstr_Store){.val = clone_irval(rhs_val),
                                                .dst = lhs_res.as.ptr};
        vec_insert(instrs, instr);

        return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = rhs_val};
      } else if (lhs_res.kind == EXPRESULT_SUBOBJECT) {
        /* Reconstruct the base variable from the subobject name */
        struct IRValue *base_var = malloc(sizeof(struct IRValue));
        base_var->kind = IRValue_VAR;
        base_var->as.var = strdup(lhs_res.as.subobject.base);
        base_var->type = lhs_res.as.subobject.base_type;

        struct IRInstr_CopyToOffset copy_to = {
            .dst = base_var,
            .offset = lhs_res.as.subobject.offset,
            .src = clone_irval(rhs_val)};

        struct IRInstr instr = {0};
        instr.kind = IRInstr_CPY_TO_OFFSET;
        instr.as.cpy_to_offset = copy_to;
        vec_insert(instrs, instr);

        /* Assignments evaluate to their right-hand side value */
        return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = rhs_val};
      } else {
        assert(0 && "Unhandled left-hand side in assignment");
      }

      break;
    }
    case EXPR_COMPOUND_ASSIGN: {
      struct ExpResult lhs_res;
      struct IRValue *rhs_val;

      /* Keep lhs as lvalue. */
      lhs_res = irfy_expr(instrs, expr->as.compound_assign.lhs);

      /* Force lvalue-to-rvalue conversion for rhs. */
      rhs_val = irfy_expr_and_convert(instrs, expr->as.compound_assign.rhs);

      /* 1. Load the current value of LHS into a temporary */
      struct IRValue *lhs_val = NULL;
      if (lhs_res.kind == EXPRESULT_PLAIN) {
        lhs_val = lhs_res.as.plain;
      } else if (lhs_res.kind == EXPRESULT_DEREF) {
        lhs_val = mkirvar();
        lhs_val->type = expr->type;
        struct IRInstr load = {
            .kind = IRInstr_LOAD,
            .as.load = {.src = lhs_res.as.ptr, .dst = clone_irval(lhs_val)}};
        vec_insert(instrs, load);
      } else if (lhs_res.kind == EXPRESULT_SUBOBJECT) {
        lhs_val = mkirvar();
        lhs_val->type = expr->type;
        struct IRValue *base_var = malloc(sizeof(struct IRValue));
        base_var->kind = IRValue_VAR;
        base_var->as.var = strdup(lhs_res.as.subobject.base);
        base_var->type = lhs_res.as.subobject.base_type;

        struct IRInstr_CopyFromOffset copy_from = {
            .src = base_var,
            .offset = lhs_res.as.subobject.offset,
            .dst = clone_irval(lhs_val)};
        struct IRInstr instr = {.kind = IRInstr_CPY_FROM_OFFSET,
                                .as.cpy_from_offset = copy_from};
        vec_insert(instrs, instr);
      }

      /* 2. Perform the arithmetic operation */
      struct IRValue *bin_res = mkirvar();
      bin_res->type = expr->type;

      struct IRInstr bin_instr = {
          .kind = IRInstr_BIN,
          .as.binary = {.kind = expr_bin_to_ir_bin(
                            expr->as.compound_assign.kind, expr->type),
                        .lhs = clone_irval(lhs_val),
                        .rhs = rhs_val,
                        .dst = clone_irval(bin_res)}};
      vec_insert(instrs, bin_instr);

      /* 3. Store the result back into the LHS location */
      if (lhs_res.kind == EXPRESULT_PLAIN) {
        struct IRInstr cpy = {
            .kind = IRInstr_CPY,
            .as.copy = {.src = clone_irval(bin_res),
                        .dst = clone_irval(lhs_res.as.plain)}};
        vec_insert(instrs, cpy);
      } else if (lhs_res.kind == EXPRESULT_DEREF) {
        struct IRInstr store = {
            .kind = IRInstr_STORE,
            .as.store = {.val = clone_irval(bin_res), .dst = lhs_res.as.ptr}};
        vec_insert(instrs, store);
      } else if (lhs_res.kind == EXPRESULT_SUBOBJECT) {
        struct IRValue *base_var = malloc(sizeof(struct IRValue));
        base_var->kind = IRValue_VAR;
        base_var->as.var = strdup(lhs_res.as.subobject.base);
        base_var->type = lhs_res.as.subobject.base_type;

        struct IRInstr_CopyToOffset copy_to = {
            .dst = base_var,
            .offset = lhs_res.as.subobject.offset,
            .src = clone_irval(bin_res)};
        struct IRInstr instr = {.kind = IRInstr_CPY_TO_OFFSET,
                                .as.cpy_to_offset = copy_to};
        vec_insert(instrs, instr);
      }

      return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = bin_res};
    }
    case EXPR_CALL: {
      /* Force lvalue-to-rvalue conversion upon the arguments.  */
      VecIRValuePtr args = {0};
      for (int i = 0; i < expr->as.call.arguments.len; i++) {
        vec_insert(&args, irfy_expr_and_convert(
                              instrs, &expr->as.call.arguments.data[i]));
      }

      /* If the function returns a value, make sure we capture it in dst.  */
      struct IRValue *dst = NULL;
      if (expr->type.kind != VOID_T) {
        dst = mkirvar();
        dst->type = expr->type;
      }

      struct IRInstr_Call call_instr = {0};
      call_instr.target = *expr->as.call.target;
      call_instr.args = args;
      call_instr.dst = clone_irval(dst);

      struct IRInstr instr = {0};
      instr.kind = IRInstr_CALL;
      instr.as.call = call_instr;
      vec_insert(instrs, instr);

      /* Function calls evaluate to rvalues.  */
      if (dst) {
        return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = dst};
      } else {
        return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = NULL};
      }

      break;
    }
    case EXPR_SIZEOF: {
      struct ExpResult result;
      struct IRValue sz;

      printf("expr type is: \n");
      print_type(&expr->type, 0);

      sz.kind = IRValue_CONST;
      sz.as.konst = (struct Literal){
          .kind = LITERAL_NUM,
          .as.u64 = (unsigned long long) get_type_size(expr->type)};
      sz.type = (Type){.kind = U64_T};

      result.kind = EXPRESULT_PLAIN;
      result.as.plain = ALLOC(sz);

      return result;
    }
    case EXPR_ADDROF: {
      struct ExpResult result;

      result = irfy_expr(instrs, expr->as.addrof.expr);
      switch (result.kind) {
        case EXPRESULT_PLAIN: {
          /* If we take addrof of a plain operand, we will do
           * `dst = getaddr(src)`, and return cloned `dst`.  */
          struct IRValue *dst = mkirvar();
          dst->type = expr->type;

          struct IRInstr instr;
          instr.kind = IRInstr_GETADDR;
          instr.as.getaddr.src = result.as.plain;
          instr.as.getaddr.dst = clone_irval(dst);
          vec_insert(instrs, instr);

          return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = dst};
        }
        case EXPRESULT_DEREF: {
          /* If we take addrof of a deref (like `&*p`), they cancel out.  */
          return (struct ExpResult){.kind = EXPRESULT_PLAIN,
                                    .as.plain = result.as.ptr};
        }
        case EXPRESULT_SUBOBJECT: {
          struct IRValue *base_var = malloc(sizeof(struct IRValue));
          base_var->kind = IRValue_VAR;
          base_var->as.var = strdup(result.as.subobject.base);
          base_var->type = result.as.subobject.base_type;

          struct IRValue *base_ptr = mkirvar();
          base_ptr->type =
              (Type){.kind = PTR_T, .as.base = ALLOC(base_var->type)};

          struct IRInstr_GetAddress getaddr = {.src = base_var,
                                               .dst = clone_irval(base_ptr)};
          vec_insert(instrs, ((struct IRInstr){.kind = IRInstr_GETADDR,
                                               .as.getaddr = getaddr}));
          struct IRValue *field_ptr = mkirvar();
          field_ptr->type = expr->type;

          struct IRValue *index_val = malloc(sizeof(struct IRValue));
          index_val->kind = IRValue_CONST;
          index_val->type = (Type){.kind = I32_T};
          index_val->as.konst.kind = LITERAL_NUM;
          index_val->as.konst.type = (Type){.kind = I32_T};
          index_val->as.konst.as.i32 = result.as.subobject.offset;

          struct IRInstr_AddPtr add_ptr = {.ptr = base_ptr,
                                           .index = index_val,
                                           .scale = 1,
                                           .dst = clone_irval(field_ptr)};
          vec_insert(instrs, ((struct IRInstr){.kind = IRInstr_ADD_PTR,
                                               .as.add_ptr = add_ptr}));
          return (struct ExpResult){.kind = EXPRESULT_PLAIN,
                                    .as.plain = field_ptr};
        }
        default:
          assert(0);
      }

      break;
    }
    case EXPR_DEREF: {
      /* When we encounter a deref expression, like `*p`, we need to
       * evaluate the inner expr, which is `p`, to get the actual mem
       * addr.  But, we do NOT issue a LOAD instruction just yet.  Instead,
       * we defer this to the parent node because a Deref node might be
       * an lvalue (`*x = 5`) or an rvalue (`y = *x + 5`).  The parent node
       * will be the one to intercept this and decide whether a LOAD is needed
       * or just CPY.  */
      struct IRValue *ptr_val;

      ptr_val = irfy_expr_and_convert(instrs, expr->as.deref.expr);
      return (struct ExpResult){.kind = EXPRESULT_DEREF, .as.ptr = ptr_val};
    }
    case EXPR_CAST: {
      struct IRValue *src = irfy_expr_and_convert(instrs, expr->as.cast.expr);
      struct IRValue *dst = mkirvar();
      dst->type = expr->type;

      enum IRCastKind kind =
          get_cast_kind(expr->as.cast.expr->type, expr->type);

      if (kind == IRCast_None || kind == IRCast_Bitcast ||
          kind == IRCast_PtrToInt || kind == IRCast_IntToPtr) {
        struct IRInstr i = {0};
        i.kind = IRInstr_CPY;
        i.as.copy.src = src;
        i.as.copy.dst = clone_irval(dst);
        vec_insert(instrs, i);
      } else {
        struct IRInstr i = {0};
        i.kind = IRInstr_CAST;
        i.as.cast.kind = kind;
        i.as.cast.src = src;
        i.as.cast.dst = clone_irval(dst);
        vec_insert(instrs, i);
      }

      return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = dst};
    }
    case EXPR_MEMBER: {
      Type target_type = expr->as.member.target->type;
      char *struct_name = expr->as.member.is_arrow
                              ? target_type.as.base->as.struct_name
                              : target_type.as.struct_name;

      struct StructDef *def = struct_get(struct_table, struct_name);

      int offset = 0;
      for (int i = 0; i < def->fields.len; i++) {
        if (strcmp(def->fields.data[i].name, expr->as.member.field_name) == 0) {
          offset = def->fields.data[i].offset;
          break;
        }
      }

      if (expr->as.member.is_arrow) {
        struct IRValue *base_ptr =
            irfy_expr_and_convert(instrs, expr->as.member.target);

        if (offset == 0) {
          return (struct ExpResult){.kind = EXPRESULT_DEREF,
                                    .as.ptr = base_ptr};
        }
        struct IRValue *field_ptr = mkirvar();
        field_ptr->type = (Type){.kind = PTR_T, .as.base = ALLOC(expr->type)};

        struct IRValue *index_val = malloc(sizeof(struct IRValue));
        index_val->kind = IRValue_CONST;
        index_val->type = (Type){.kind = I32_T};
        index_val->as.konst.kind = LITERAL_NUM;
        index_val->as.konst.type = (Type){.kind = I32_T};
        index_val->as.konst.as.i32 = offset;

        struct IRInstr_AddPtr add_ptr = {.ptr = base_ptr,
                                         .index = index_val,
                                         .scale = 1,
                                         .dst = clone_irval(field_ptr)};
        vec_insert(instrs, ((struct IRInstr){.kind = IRInstr_ADD_PTR,
                                             .as.add_ptr = add_ptr}));

        return (struct ExpResult){.kind = EXPRESULT_DEREF, .as.ptr = field_ptr};
      } else {
        /* Direct access: foo.bar */
        struct ExpResult target_res = irfy_expr(instrs, expr->as.member.target);
        if (target_res.kind == EXPRESULT_PLAIN) {
          return (struct ExpResult){
              .kind = EXPRESULT_SUBOBJECT,
              .as.subobject = {
                  .base = strdup(target_res.as.plain->as.var),
                  .offset = offset,
                  .base_type = target_res.as.plain->type,
              }};
        } else if (target_res.kind == EXPRESULT_SUBOBJECT) {
          return (struct ExpResult){
              .kind = EXPRESULT_SUBOBJECT,
              .as.subobject = {
                  .base = strdup(target_res.as.subobject.base),
                  .offset = target_res.as.subobject.offset + offset,
                  .base_type = target_res.as.subobject.base_type,
              }};
        } else if (target_res.kind == EXPRESULT_DEREF) {
          if (offset == 0) {
            return target_res;
          }

          struct IRValue *field_ptr = mkirvar();
          field_ptr->type = (Type){.kind = PTR_T, .as.base = ALLOC(expr->type)};

          struct IRValue *index_val = malloc(sizeof(struct IRValue));
          index_val->kind = IRValue_CONST;
          index_val->type = (Type){.kind = I32_T};
          index_val->as.konst.kind = LITERAL_NUM;
          index_val->as.konst.type = (Type){.kind = I32_T};
          index_val->as.konst.as.i32 = offset;

          struct IRInstr_AddPtr add_ptr = {.ptr = target_res.as.ptr,
                                           .index = index_val,
                                           .scale = 1,
                                           .dst = clone_irval(field_ptr)};
          vec_insert(instrs, ((struct IRInstr){.kind = IRInstr_ADD_PTR,
                                               .as.add_ptr = add_ptr}));

          return (struct ExpResult){.kind = EXPRESULT_DEREF,
                                    .as.ptr = field_ptr};
        }
      }
      break;
    }
    case EXPR_STRUCT_INIT: {
      struct IRValue *tmp_struct = mkirvar();
      tmp_struct->type.kind = STRUCT_T;
      tmp_struct->type.as.struct_name = expr->as.struct_init.struct_name;

      for (int i = 0; i < expr->as.struct_init.values.len; i++) {
        struct IRValue *val = irfy_expr_and_convert(
            instrs, expr->as.struct_init.values.data[i].expr);

        int offset = expr->as.struct_init.values.data[i].resolved_offset;

        struct IRInstr_CopyToOffset copy_to = {
            .dst = clone_irval(tmp_struct), .offset = offset, .src = val};

        vec_insert(instrs, ((struct IRInstr){.kind = IRInstr_CPY_TO_OFFSET,
                                             .as.cpy_to_offset = copy_to}));
      }

      return (struct ExpResult){.kind = EXPRESULT_PLAIN,
                                .as.plain = tmp_struct};
    }
    default:
      assert(0);
  }
}

int extract_label_number(const char *label)
{
  if (!label) {
    return -1;
  }

  const char *dot = strrchr(label, '.');

  if (!dot) {
    return -1;
  }

  dot++;

  int n = -1;
  if (sscanf(dot, "%d", &n) == 1) {
    return n;
  }

  return -1;
}

void irfy_stmt(VecIRInstr *instrs, struct Stmt *stmt)
{
  switch (stmt->kind) {
    case STMT_FN:
    case STMT_STRUCT:
      assert(0);
    case STMT_LOOP: {
      int tmp;
      struct IRInstr begin, jmp, end;

      tmp = extract_label_number(stmt->as.loop.label);

      begin.kind = IRInstr_LBL;
      begin.as.label.name = mklbl("Loop", tmp);

      vec_insert(instrs, begin);

      irfy_stmt(instrs, stmt->as.loop.body);

      jmp.kind = IRInstr_JMP;
      jmp.as.jmp.target = mklbl("Loop", tmp);

      vec_insert(instrs, jmp);

      end.kind = IRInstr_LBL;
      end.as.label.name = mklbl("End", tmp);

      vec_insert(instrs, end);

      break;
    }
    case STMT_LET: {
      struct IRValue *res, *dst;
      struct IRInstr cpy = {0};

      res = irfy_expr_and_convert(instrs, stmt->as.let.init);

      dst = malloc(sizeof(struct IRValue));
      dst->kind = IRValue_VAR;
      dst->as.var = strdup(stmt->as.let.name);
      dst->type = stmt->as.let.type;

      cpy.kind = IRInstr_CPY;
      cpy.as.copy = (struct IRInstr_Copy){.src = res, .dst = dst};

      vec_insert(instrs, cpy);
      break;
    }
    case STMT_RET: {
      struct IRInstr i = {0};

      i.kind = IRInstr_RET;
      i.as.ret.val = stmt->as.ret.val
                         ? irfy_expr_and_convert(instrs, stmt->as.ret.val)
                         : NULL;

      vec_insert(instrs, i);
      break;
    }
    case STMT_IF: {
      int tmp;
      struct IRValue *cond;

      tmp = mktmp();
      cond = irfy_expr_and_convert(instrs, &stmt->as.if_stmt.cond);

      if (!stmt->as.if_stmt.else_block) {
        struct IRInstr i1 = {0}, i2 = {0};
        struct IRInstr_JumpIfZero ijz = {0};
        struct IRInstr_Label ilbl = {0};

        ijz.cond = *cond;
        ijz.target = mklbl("End", tmp);

        i1.kind = IRInstr_JZ;
        i1.as.jz = ijz;

        vec_insert(instrs, i1);

        irfy_stmt(instrs, stmt->as.if_stmt.then_block);

        ilbl.name = mklbl("End", tmp);

        i2.kind = IRInstr_LBL;
        i2.as.label = ilbl;

        vec_insert(instrs, i2);
      } else {
        struct IRInstr jz_instr = {0};
        jz_instr.kind = IRInstr_JZ;
        jz_instr.as.jz.cond = *cond;
        jz_instr.as.jz.target = mklbl("Else", tmp);
        vec_insert(instrs, jz_instr);

        irfy_stmt(instrs, stmt->as.if_stmt.then_block);

        struct IRInstr jmp_end = {0};
        jmp_end.kind = IRInstr_JMP;
        jmp_end.as.jmp.target = mklbl("End", tmp);
        vec_insert(instrs, jmp_end);

        struct IRInstr label_else = {0};
        label_else.kind = IRInstr_LBL;
        label_else.as.label.name = mklbl("Else", tmp);
        vec_insert(instrs, label_else);

        irfy_stmt(instrs, stmt->as.if_stmt.else_block);

        struct IRInstr label_end = {0};
        label_end.kind = IRInstr_LBL;
        label_end.as.label.name = mklbl("End", tmp);
        vec_insert(instrs, label_end);
      }

      free(cond);
      break;
    }
    case STMT_WHILE: {
      int tmp;
      struct IRValue *cond;
      struct IRInstr i1 = {0}, i2 = {0}, i3 = {0}, i4 = {0};

      tmp = extract_label_number(stmt->as.while_stmt.label);

      i4.kind = IRInstr_LBL;
      i4.as.label.name = mklbl("While", tmp);

      vec_insert(instrs, i4);

      cond = irfy_expr_and_convert(instrs, &stmt->as.while_stmt.cond);

      i1.kind = IRInstr_JZ;
      i1.as.jz.cond = *cond;
      i1.as.jz.target = mklbl("End", tmp);

      free(cond);

      vec_insert(instrs, i1);

      irfy_stmt(instrs, stmt->as.while_stmt.body);

      i3.kind = IRInstr_JMP;
      i3.as.jmp.target = mklbl("While", tmp);

      vec_insert(instrs, i3);

      i2.kind = IRInstr_LBL;
      i2.as.label.name = mklbl("End", tmp);

      vec_insert(instrs, i2);
      break;
    }
    case STMT_BLOCK: {
      for (int i = 0; i < stmt->as.block.stmts.len; i++) {
        irfy_stmt(instrs, &stmt->as.block.stmts.data[i]);
      }
      break;
    }
    case STMT_BREAK: {
      char *label, *new_label;
      int n;

      label = stmt->as.break_stmt.label;
      n = extract_label_number(label);

      new_label = mklbl("End", n);

      struct IRInstr i;
      i.kind = IRInstr_JMP;

      struct IRInstr_Jump jmp;
      jmp.target = new_label;

      i.as.jmp = jmp;

      vec_insert(instrs, i);

      break;
    }
    case STMT_CONTINUE: {
      char *label;

      label = stmt->as.continue_stmt.label;

      struct IRInstr i;
      i.kind = IRInstr_JMP;
      i.as.jmp.target = strdup(label);

      vec_insert(instrs, i);
      break;
    }
    case STMT_GOTO: {
      char *label;

      label = stmt->as.goto_stmt.label;

      struct IRInstr i;
      i.kind = IRInstr_JMP;
      i.as.jmp.target = strdup(label);

      vec_insert(instrs, i);
      break;
    }
    case STMT_LABELED: {
      char *label;

      label = stmt->as.labeled.label;

      struct IRInstr i;
      i.kind = IRInstr_LBL;
      i.as.label.name = strdup(label);

      vec_insert(instrs, i);

      irfy_stmt(instrs, stmt->as.labeled.stmt);
      break;
    }
    case STMT_EXTERN:
    case STMT_ENUM:
      break;
    case STMT_EXPR: {
      struct IRValue *v;

      v = irfy_expr_and_convert(instrs, &stmt->as.expr_stmt.expr);
      if (v) {
        free_ir_val(v);
      }

      break;
    }
  }
}

struct IRFunction *irfy_fn(struct Stmt *stmt)
{
  if (stmt->kind != STMT_FN) {
    return NULL;
  }

  struct IRFunction f;
  VecIRInstr instrs = {0};

  for (int i = 0; i < stmt->as.fn.body.len; i++) {
    irfy_stmt(&instrs, &stmt->as.fn.body.data[i]);
  }

  f.body = instrs;
  f.name = stmt->as.fn.name;
  f.params = stmt->as.fn.params;
  f.retval = stmt->as.fn.retval;

  return ALLOC(f);
}

struct IrfyResult {
  bool is_ok;
  char *msg;
  struct IRProgram prog;
};

struct IrfyResult irfy_ast(struct AST *ast)
{
  struct IRProgram prog;
  struct IrfyResult result;
  VecIRFunctionPtr funcs = {0};

  result.is_ok = true;
  result.msg = NULL;

  for (int i = 0; i < ast->stmts.len; i++) {
    struct IRFunction *f;

    f = irfy_fn(&ast->stmts.data[i]);
    if (f) {
      vec_insert(&funcs, f);
    }
  }

  prog.funcs = funcs;

  result.prog = prog;

  return result;
}

struct ResolveResult {
  bool is_ok;
  char *msg;
  union {
    struct AST *ast;
    struct Stmt *stmt;
    struct Expr *expr;
    struct Parameter *param;
  } as;
};

struct Variable {
  char *uniq_name;
};

struct VariableMap {
  struct VariableMap *next;
  char *name;
  struct Variable value;
};

void varmap_insert(struct VariableMap **varmap, char *name, char *uniq_name)
{
  struct Variable v;
  struct VariableMap *node;

  v.uniq_name = uniq_name;

  node = malloc(sizeof(struct VariableMap));

  node->name = strdup(name);

  node->value = v;
  node->next = *varmap;

  *varmap = node;
}

char *varmap_get(struct VariableMap *varmap, char *name)
{
  while (varmap) {
    if (strcmp(varmap->name, name) == 0) {
      return varmap->value.uniq_name;
    }
    varmap = varmap->next;
  }
  return NULL;
}

struct ResolveResult resolve_expr(struct VariableMap **varmap,
                                  struct Expr *expr)
{
  switch (expr->kind) {
    case EXPR_STRUCT_INIT: {
      for (int i = 0; i < expr->as.struct_init.values.len; i++) {
        struct ResolveResult r;

        r = resolve_expr(varmap, expr->as.struct_init.values.data[i].expr);
        if (!r.is_ok) {
          return r;
        }
      }
      break;
    }
    case EXPR_MEMBER: {
      struct ResolveResult r;

      r = resolve_expr(varmap, expr->as.member.target);
      if (!r.is_ok) {
        return r;
      }
      break;
    }
    case EXPR_CAST: {
      struct ResolveResult r;

      r = resolve_expr(varmap, expr->as.cast.expr);
      if (!r.is_ok) {
        return r;
      }

      break;
    }
    case EXPR_LITERAL:
      break;
    case EXPR_VARIABLE: {
      char *resolved_name = varmap_get(*varmap, expr->as.var.name);
      if (resolved_name) {
        free(expr->as.var.name);
        expr->as.var.name = strdup(resolved_name);
      } else {
        int val;
        if (enum_variant_get(expr->as.var.name, &val)) {
          free(expr->as.var.name);

          expr->kind = EXPR_LITERAL;
          expr->as.literal.kind = LITERAL_NUM;
          expr->as.literal.type = (Type){.kind = I32_T};
          expr->as.literal.as.i32 = val;
          expr->type = (Type){.kind = I32_T};
        } else {
          return (struct ResolveResult){.is_ok = false,
                                        .msg = "Undefined variable"};
        }
      }
      break;
    }
    case EXPR_BINARY: {
      struct ResolveResult r1, r2;

      r1 = resolve_expr(varmap, expr->as.binary.lhs);
      if (!r1.is_ok) {
        return r1;
      }

      r2 = resolve_expr(varmap, expr->as.binary.rhs);
      if (!r2.is_ok) {
        return r2;
      }

      break;
    }
    case EXPR_CALL: {
      struct ResolveResult r;

      r = resolve_expr(varmap, expr->as.call.target);
      if (!r.is_ok) {
        return r;
      }

      for (int i = 0; i < expr->as.call.arguments.len; i++) {
        r = resolve_expr(varmap, &expr->as.call.arguments.data[i]);
        if (!r.is_ok) {
          return r;
        }
      }
      break;
    }
    case EXPR_ASSIGN: {
      struct ResolveResult rlhs, rrhs;

      rlhs = resolve_expr(varmap, expr->as.assign.lhs);
      if (!rlhs.is_ok) {
        return rlhs;
      }

      rrhs = resolve_expr(varmap, expr->as.assign.rhs);
      if (!rrhs.is_ok) {
        return rrhs;
      }

      break;
    }
    case EXPR_COMPOUND_ASSIGN: {
      struct ResolveResult rlhs, rrhs;

      rlhs = resolve_expr(varmap, expr->as.compound_assign.lhs);
      if (!rlhs.is_ok) {
        return rlhs;
      }

      rrhs = resolve_expr(varmap, expr->as.compound_assign.rhs);
      if (!rrhs.is_ok) {
        return rrhs;
      }

      break;
    }

    case EXPR_UNARY: {
      struct ResolveResult r;

      r = resolve_expr(varmap, expr->as.unary.expr);
      if (!r.is_ok) {
        return r;
      }

      break;
    }
    case EXPR_DEREF: {
      struct ResolveResult r;

      r = resolve_expr(varmap, expr->as.deref.expr);
      if (!r.is_ok) {
        return r;
      }

      break;
    }
    case EXPR_ADDROF: {
      struct ResolveResult r;

      r = resolve_expr(varmap, expr->as.addrof.expr);
      if (!r.is_ok) {
        return r;
      }

      break;
    }
    case EXPR_SIZEOF: {
      struct ResolveResult r;

      r = resolve_expr(varmap, expr->as.sizeof_expr.expr);
      if (!r.is_ok) {
        return r;
      }

      break;
    }
    default:
      assert(0);
  }
  return (struct ResolveResult){.is_ok = true, .msg = NULL, .as.expr = expr};
}

struct ResolveResult resolve_param(struct VariableMap **varmap,
                                   struct Parameter *param)
{
  char *uniq_name;

  uniq_name = mkuniq(param->name);
  varmap_insert(varmap, param->name, uniq_name);
  free(param->name);
  param->name = strdup(uniq_name);

  return (struct ResolveResult){.is_ok = true, .msg = NULL, .as.param = param};
}

struct ResolveResult resolve_stmt(struct VariableMap **varmap,
                                  struct Stmt *stmt)
{
  switch (stmt->kind) {
    case STMT_LOOP: {
      struct ResolveResult r;

      r = resolve_stmt(varmap, stmt->as.loop.body);
      if (!r.is_ok) {
        return r;
      }

      break;
    }
    case STMT_LABELED: {
      struct ResolveResult r;

      r = resolve_stmt(varmap, stmt->as.labeled.stmt);
      if (!r.is_ok) {
        return r;
      }

      break;
    }
    case STMT_EXTERN: {
      char *cpy;

      cpy = strdup(stmt->as.extern_stmt.name);
      varmap_insert(varmap, stmt->as.extern_stmt.name, cpy);
      break;
    }
    case STMT_BREAK:
    case STMT_CONTINUE:
    case STMT_ENUM:
    case STMT_GOTO:
      break;
    case STMT_EXPR: {
      struct ResolveResult r;

      r = resolve_expr(varmap, &stmt->as.expr_stmt.expr);
      if (!r.is_ok) {
        return r;
      }
      break;
    }
    case STMT_WHILE: {
      struct ResolveResult cond_res, body_res;

      cond_res = resolve_expr(varmap, &stmt->as.while_stmt.cond);
      if (!cond_res.is_ok) {
        return cond_res;
      }

      body_res = resolve_stmt(varmap, stmt->as.while_stmt.body);
      if (!body_res.is_ok) {
        return body_res;
      }

      break;
    }
    case STMT_IF: {
      struct ResolveResult cond_res, then_res, else_res;

      cond_res = resolve_expr(varmap, &stmt->as.if_stmt.cond);
      if (!cond_res.is_ok) {
        return cond_res;
      }

      then_res = resolve_stmt(varmap, stmt->as.if_stmt.then_block);
      if (!then_res.is_ok) {
        return then_res;
      }

      if (stmt->as.if_stmt.else_block) {
        else_res = resolve_stmt(varmap, stmt->as.if_stmt.else_block);
        if (!else_res.is_ok) {
          return else_res;
        }
      }

      break;
    }
    case STMT_FN: {
      struct VariableMap *variable_map, *outer_map;
      char *cpy;

      cpy = strdup(stmt->as.fn.name);

      if (varmap) {
        varmap_insert(varmap, stmt->as.fn.name, cpy);
      }

      variable_map = varmap ? *varmap : NULL;
      outer_map = variable_map;

      for (int i = 0; i < stmt->as.fn.params.len; i++) {
        struct ResolveResult r;

        r = resolve_param(&variable_map, &stmt->as.fn.params.data[i]);
        if (!r.is_ok) {
          return r;
        }
      }

      for (int i = 0; i < stmt->as.fn.body.len; i++) {
        struct ResolveResult r;

        r = resolve_stmt(&variable_map, &stmt->as.fn.body.data[i]);
        if (!r.is_ok) {
          return r;
        }
      }

      while (variable_map && variable_map != outer_map) {
        struct VariableMap *tmp;

        tmp = variable_map;
        variable_map = variable_map->next;

        free(tmp->name);
        free(tmp->value.uniq_name);
        free(tmp);
      }

      break;
    }
    case STMT_BLOCK: {
      struct VariableMap *outer_map = *varmap;

      for (int i = 0; i < stmt->as.block.stmts.len; i++) {
        struct ResolveResult r;

        r = resolve_stmt(varmap, &stmt->as.block.stmts.data[i]);
        if (!r.is_ok) {
          return r;
        }
      }

      while (*varmap != outer_map) {
        struct VariableMap *tmp = *varmap;
        *varmap = tmp->next;

        free(tmp->name);
        free(tmp->value.uniq_name);
        free(tmp);
      }
      break;
    }
    case STMT_LET: {
      struct ResolveResult r;

      r = resolve_expr(varmap, stmt->as.let.init);
      if (!r.is_ok) {
        return r;
      }

      char *uniq_name;

      uniq_name = mkuniq(stmt->as.let.name);

      varmap_insert(varmap, stmt->as.let.name, uniq_name);

      free(stmt->as.let.name);
      stmt->as.let.name = strdup(uniq_name);

      break;
    }
    case STMT_RET: {
      struct ResolveResult r;

      r = resolve_expr(varmap, stmt->as.ret.val);
      if (!r.is_ok) {
        return r;
      }

      break;
    }
  }

  return (struct ResolveResult){.is_ok = true, .msg = NULL, .as.stmt = stmt};
}

struct ResolveResult resolve(struct AST *ast)
{
  struct VariableMap *global_map = NULL;
  for (int i = 0; i < ast->stmts.len; i++) {
    struct ResolveResult r;

    r = resolve_stmt(&global_map, &ast->stmts.data[i]);
    if (!r.is_ok) {
      while (global_map) {
        struct VariableMap *tmp;

        tmp = global_map;
        global_map = global_map->next;

        free(tmp->name);
        free(tmp->value.uniq_name);
        free(tmp);
      }
      return r;
    }
  }

  while (global_map) {
    struct VariableMap *tmp = global_map;
    global_map = global_map->next;
    free(tmp->name);
    free(tmp->value.uniq_name);
    free(tmp);
  }

  return (struct ResolveResult){.is_ok = true, .msg = NULL, .as.ast = ast};
}

struct TypecheckResult {
  bool is_ok;
  char *msg;
  struct AST *ast;
};

struct Symbol {
  struct Symbol *next;
  char *name;
  Type type;
  bool is_mut;
};

void free_symbol(struct Symbol *sym)
{
  free_type(&sym->type);
  free(sym);
}

void sym_insert(struct Symbol **sym, char *name, Type type, bool is_mut)
{
  struct Symbol *node;

  node = malloc(sizeof(struct Symbol));

  node->name = name;
  node->type = type;
  node->is_mut = is_mut;
  node->next = *sym;

  *sym = node;
}

struct Symbol *sym_get(struct Symbol *sym, char *name)
{
  while (sym) {
    if (strcmp(sym->name, name) == 0) {
      return sym;
    }
    sym = sym->next;
  }
  return NULL;
}

void print_type(Type *type, int spaces)
{
  switch (type->kind) {
    case STRUCT_T:
      printf("%s", type->as.struct_name);
      break;
    case VOID_T:
      printf("void");
      break;
    case PTR_T:
      printf("*");
      print_type(type->as.base, spaces);
      break;
    case U8_T:
      printf("u8");
      break;
    case U16_T:
      printf("u16");
      break;
    case U32_T:
      printf("u32");
      break;
    case U64_T:
      printf("u64");
      break;
    case I8_T:
      printf("i8");
      break;
    case I16_T:
      printf("i16");
      break;
    case I32_T:
      printf("i32");
      break;
    case I64_T:
      printf("i64");
      break;
    case F32_T:
      printf("f32");
      break;
    case F64_T:
      printf("f64");
      break;
    case BOOL_T:
      printf("bool");
      break;
    case STR_T:
      printf("str");
      break;
    case FN_T: {
      printf("fn(\n");

      print_indent(spaces + 2);
      printf("args: [\n");
      for (int i = 0; i < type->as.func.params.len; i++) {
        print_indent(spaces + 4);
        print_type(&type->as.func.params.data[i], spaces + 4);
        printf(",\n");
      }

      if (type->as.func.is_variadic) {
        print_indent(spaces + 4);
        printf("...\n");
      }

      print_indent(spaces + 2);
      printf("],\n");

      print_indent(spaces + 2);
      printf("retval: ");
      if (type->as.func.retval) {
        print_type(type->as.func.retval, spaces + 2);
      } else {
        printf("void");
      }
      printf("\n");

      print_indent(spaces);
      printf(")");
      break;
    }
    case UNKNOWN_T:
      printf("unknown");
      break;
  }
}

#define IN_RANGE(val, min, max) ((val) >= (min) && (val) <= (max))

bool is_bitwise_binop(enum ExprBinKind kind)
{
  return kind == EXPR_BIN_BITWISE_AND || kind == EXPR_BIN_BITWISE_XOR ||
         kind == EXPR_BIN_BITWISE_OR || kind == EXPR_BIN_SHIFT_LEFT ||
         kind == EXPR_BIN_SHIFT_RIGHT;
}

bool is_shift_binop(enum ExprBinKind kind)
{
  return kind == EXPR_BIN_SHIFT_LEFT || kind == EXPR_BIN_SHIFT_RIGHT;
}

void get_type_size_and_align(Type *type, int *size, int *align)
{
  switch (type->kind) {
    case I8_T:
    case U8_T:
    case BOOL_T:
      *size = 1;
      *align = 1;
      break;
    case I16_T:
    case U16_T:
      *size = 2;
      *align = 2;
      break;
    case I32_T:
    case U32_T:
    case F32_T:
      *size = 4;
      *align = 4;
      break;
    case I64_T:
    case U64_T:
    case F64_T:
    case PTR_T:
    case STR_T:
      *size = 8;
      *align = 8;
      break;
    case STRUCT_T: {
      struct StructDef *def = struct_get(struct_table, type->as.struct_name);
      assert(def && "Tried to get size of incomplete/unknown struct");
      *size = def->size;
      *align = def->alignment;
      break;
    }
    default:
      *size = -1;
      *align = -1;
      break;
  }
}

Type get_common_type(struct Expr *lhs, struct Expr *rhs)
{
  Type t1 = lhs->type;
  Type t2 = rhs->type;

  if (t1.kind == t2.kind) {
    return t1;
  }

  int size1 = get_type_size(t1);
  int size2 = get_type_size(t2);

  if (size1 == -1 || size2 == -1) {
    return (Type){.kind = UNKNOWN_T};
  }

  if (size1 > size2) {
    return t1;
  }
  if (size2 > size1) {
    return t2;
  }

  if (is_unsigned(t1.kind)) {
    return t1;
  }
  if (is_unsigned(t2.kind)) {
    return t2;
  }

  return (Type){.kind = UNKNOWN_T};
}
bool promote_literal(struct Expr *expr, Type target_type)
{
  if (expr->kind == EXPR_UNARY && expr->as.unary.expr->kind == EXPR_LITERAL &&
      expr->as.unary.expr->as.literal.kind == LITERAL_NUM) {
    bool res = promote_literal(expr->as.unary.expr, target_type);
    if (res) {
      expr->type = target_type;
    }
    return res;
  }

  if (expr->kind != EXPR_LITERAL || expr->as.literal.kind != LITERAL_NUM) {
    return false;
  }

  if (expr->kind != EXPR_LITERAL || expr->as.literal.kind != LITERAL_NUM) {
    return false;
  }

  bool src_is_float = (expr->as.literal.type.kind == F32_T ||
                       expr->as.literal.type.kind == F64_T);
  bool tgt_is_float = (target_type.kind == F32_T || target_type.kind == F64_T);

  if (src_is_float && tgt_is_float) {
    double val = (expr->as.literal.type.kind == F32_T)
                     ? (double) expr->as.literal.as.f32
                     : expr->as.literal.as.f64;

    expr->type = target_type;
    expr->as.literal.type = target_type;

    if (target_type.kind == F32_T) {
      expr->as.literal.as.f32 = (float) val;
    } else {
      expr->as.literal.as.f64 = val;
    }
    return true;
  }

  if (src_is_float != tgt_is_float) {
    return false;
  }

  bool src_is_unsigned = is_unsigned(expr->as.literal.type.kind);
  bool tgt_is_unsigned = is_unsigned(target_type.kind);

  unsigned long long uval = 0;
  long long sval = 0;

  if (src_is_unsigned) {
    switch (expr->as.literal.type.kind) {
      case U8_T:
        uval = expr->as.literal.as.u8;
        break;
      case U16_T:
        uval = expr->as.literal.as.u16;
        break;
      case U32_T:
        uval = expr->as.literal.as.u32;
        break;
      case U64_T:
        uval = expr->as.literal.as.u64;
        break;
      default:
        break;
    }
  } else {
    switch (expr->as.literal.type.kind) {
      case I8_T:
        sval = expr->as.literal.as.i8;
        break;
      case I16_T:
        sval = expr->as.literal.as.i16;
        break;
      case I32_T:
        sval = expr->as.literal.as.i32;
        break;
      case I64_T:
        sval = expr->as.literal.as.i64;
        break;
      default:
        break;
    }
  }

  bool fits = false;

  if (src_is_unsigned && tgt_is_unsigned) {
    switch (target_type.kind) {
      case U8_T:
        fits = (uval <= UCHAR_MAX);
        break;
      case U16_T:
        fits = (uval <= USHRT_MAX);
        break;
      case U32_T:
        fits = (uval <= UINT_MAX);
        break;
      case U64_T:
        fits = (uval <= ULLONG_MAX);
        break;
      default:
        break;
    }
  } else if (!src_is_unsigned && !tgt_is_unsigned) {
    switch (target_type.kind) {
      case I8_T:
        fits = IN_RANGE(sval, SCHAR_MIN, SCHAR_MAX);
        break;
      case I16_T:
        fits = IN_RANGE(sval, SHRT_MIN, SHRT_MAX);
        break;
      case I32_T:
        fits = IN_RANGE(sval, INT_MIN, INT_MAX);
        break;
      case I64_T:
        fits = IN_RANGE(sval, LLONG_MIN, LLONG_MAX);
        break;
      default:
        break;
    }
  } else if (!src_is_unsigned && tgt_is_unsigned) {
    if (sval < 0) {
      return false;
    }

    unsigned long long casted = (unsigned long long) sval;
    switch (target_type.kind) {
      case U8_T:
        fits = (casted <= UCHAR_MAX);
        break;
      case U16_T:
        fits = (casted <= USHRT_MAX);
        break;
      case U32_T:
        fits = (casted <= UINT_MAX);
        break;
      case U64_T:
        fits = (casted <= ULLONG_MAX);
        break;
      default:
        break;
    }
  } else if (src_is_unsigned && !tgt_is_unsigned) {
    switch (target_type.kind) {
      case I8_T:
        fits = (uval <= SCHAR_MAX);
        break;
      case I16_T:
        fits = (uval <= SHRT_MAX);
        break;
      case I32_T:
        fits = (uval <= INT_MAX);
        break;
      case I64_T:
        fits = (uval <= LLONG_MAX);
        break;
      default:
        break;
    }
  }

  if (fits) {
    expr->type = target_type;
    expr->as.literal.type = target_type;

    unsigned long long final_uval =
        src_is_unsigned ? uval : (unsigned long long) sval;
    long long final_sval = src_is_unsigned ? (long long) uval : sval;

    switch (target_type.kind) {
      case I8_T:
        expr->as.literal.as.i8 = (char) final_sval;
        break;
      case U8_T:
        expr->as.literal.as.u8 = (unsigned char) final_uval;
        break;
      case I16_T:
        expr->as.literal.as.i16 = (short) final_sval;
        break;
      case U16_T:
        expr->as.literal.as.u16 = (unsigned short) final_uval;
        break;
      case I32_T:
        expr->as.literal.as.i32 = (int) final_sval;
        break;
      case U32_T:
        expr->as.literal.as.u32 = (unsigned int) final_uval;
        break;
      case I64_T:
        expr->as.literal.as.i64 = (long long) final_sval;
        break;
      case U64_T:
        expr->as.literal.as.u64 = final_uval;
        break;
      default:
        break;
    }
    return true;
  }

  return false;
}

struct TypecheckResult coerce_expr_to_type(struct Expr *expr, Type target_type,
                                           char *err_msg)
{
  if (types_equal(expr->type, target_type)) {
    return (struct TypecheckResult){.is_ok = true, .msg = NULL, .ast = NULL};
  }

  bool is_literal =
      (expr->kind == EXPR_LITERAL && expr->as.literal.kind == LITERAL_NUM);
  bool is_unary_literal =
      (expr->kind == EXPR_UNARY && expr->as.unary.expr->kind == EXPR_LITERAL &&
       expr->as.unary.expr->as.literal.kind == LITERAL_NUM);

  if (is_literal || is_unary_literal) {
    if (!promote_literal(expr, target_type)) {
      return (struct TypecheckResult){
          .is_ok = false, .msg = err_msg, .ast = NULL};
    }
  }

  return (struct TypecheckResult){.is_ok = true, .msg = NULL, .ast = NULL};
}

bool is_expr_mutable(struct Expr *expr, struct Symbol *sym_table)
{
  switch (expr->kind) {
    case EXPR_VARIABLE: {
      struct Symbol *sym = sym_get(sym_table, expr->as.var.name);
      return sym ? sym->is_mut : false;
    }
    case EXPR_MEMBER: {
      if (expr->as.member.is_arrow) {
        return true;
      }
      return is_expr_mutable(expr->as.member.target, sym_table);
    }
    case EXPR_DEREF: {
      return true;
    }
    default:
      return false;
  }
}

struct TypecheckResult typecheck_expr(struct Expr *expr,
                                      struct Symbol *sym_table)
{
  struct TypecheckResult res = {.is_ok = true, .msg = NULL, .ast = NULL};

  switch (expr->kind) {
    case EXPR_SIZEOF: {
      struct TypecheckResult r;

      r = typecheck_expr(expr->as.sizeof_expr.expr, sym_table);
      if (!r.is_ok) {
        return r;
      }

      expr->type = expr->as.sizeof_expr.expr->type;

      /* FIXME: is complete? */
      break;
    }
    case EXPR_MEMBER: {
      struct TypecheckResult r;

      r = typecheck_expr(expr->as.member.target, sym_table);
      if (!r.is_ok) {
        return r;
      }

      Type target_type = expr->as.member.target->type;
      char *struct_name = NULL;

      /* 1. Validate the left-hand side */
      if (expr->as.member.is_arrow) {
        if (target_type.kind != PTR_T ||
            target_type.as.base->kind != STRUCT_T) {
          return (struct TypecheckResult){
              .is_ok = false,
              .msg = "Left of '->' must be a pointer to struct",
              .ast = NULL};
        }
        struct_name = target_type.as.base->as.struct_name;
      } else {
        if (target_type.kind != STRUCT_T) {
          return (struct TypecheckResult){.is_ok = false,
                                          .msg = "Left of '.' must be a struct",
                                          .ast = NULL};
        }
        struct_name = target_type.as.struct_name;
      }

      /* 2. Look up the struct definition */
      struct StructDef *def = struct_get(struct_table, struct_name);
      if (!def) {
        return (struct TypecheckResult){
            .is_ok = false,
            .msg = "Accessing member of incomplete/unknown struct",
            .ast = NULL};
      }

      /* 3. Find the field and assign the expression's type */
      bool found = false;
      for (int i = 0; i < def->fields.len; i++) {
        if (strcmp(def->fields.data[i].name, expr->as.member.field_name) == 0) {
          expr->type = clone_type(def->fields.data[i].type);
          found = true;
          break;
        }
      }

      if (!found) {
        return (struct TypecheckResult){
            .is_ok = false, .msg = "Struct has no such field", .ast = NULL};
      }

      break;
    }
    case EXPR_STRUCT_INIT: {
      struct StructDef *def =
          struct_get(struct_table, expr->as.struct_init.struct_name);
      if (!def) {
        return (struct TypecheckResult){
            .is_ok = false, .msg = "Initializing unknown struct", .ast = NULL};
      }

      int positional_idx = 0;
      for (int i = 0; i < expr->as.struct_init.values.len; i++) {
        struct StructInitItem *item = &expr->as.struct_init.values.data[i];

        struct TypecheckResult r = typecheck_expr(item->expr, sym_table);
        if (!r.is_ok) {
          return r;
        }

        Type expected_type;
        int resolved_offset = 0;

        if (item->designator) {
          char *path = strdup(item->designator);
          char *part = strtok(path, ".");
          struct StructDef *curr_def = def;
          Type current_type;

          while (part) {
            bool found = false;
            for (int f = 0; f < curr_def->fields.len; f++) {
              if (strcmp(curr_def->fields.data[f].name, part) == 0) {
                found = true;
                resolved_offset += curr_def->fields.data[f].offset;
                current_type = curr_def->fields.data[f].type;
                break;
              }
            }
            if (!found) {
              free(path);
              return (struct TypecheckResult){.is_ok = false,
                                              .msg = "Field not found"};
            }

            part = strtok(NULL, ".");
            if (part) {
              if (current_type.kind != STRUCT_T) {
                return (struct TypecheckResult){.is_ok = false,
                                                .msg = "Not a struct"};
              }
              curr_def = struct_get(struct_table, current_type.as.struct_name);
            }
          }
          free(path);
          expected_type = current_type;

        } else {
          /* Handle positional fallback */
          expected_type = def->fields.data[positional_idx].type;
          resolved_offset = def->fields.data[positional_idx].offset;
          positional_idx++;
        }

        /* Save the final flat offset for the IR phase! */
        item->resolved_offset = resolved_offset;
      }

      expr->type.kind = STRUCT_T;
      expr->type.as.struct_name = strdup(def->name);
      break;
    }
    case EXPR_ADDROF: {
      struct TypecheckResult r;

      r = typecheck_expr(expr->as.addrof.expr, sym_table);
      if (!r.is_ok) {
        return r;
      }

      Type *base = malloc(sizeof(Type));
      *base = expr->as.addrof.expr->type;
      expr->type = (Type){.kind = PTR_T, .as.base = base};
      break;
    }
    case EXPR_DEREF: {
      struct TypecheckResult r;

      r = typecheck_expr(expr->as.deref.expr, sym_table);
      if (!r.is_ok) {
        return r;
      }

      if (expr->as.deref.expr->type.kind != PTR_T) {
        return (struct TypecheckResult){
            .is_ok = false, .msg = "Cannot dereference a non-pointer"};
      }
      if (expr->as.deref.expr->type.as.base->kind == VOID_T) {
        return (struct TypecheckResult){
            .is_ok = false, .msg = "Cannot dereference a void pointer"};
      }

      expr->type = *expr->as.deref.expr->type.as.base;
      break;
    }
    case EXPR_CAST: {
      struct TypecheckResult r = typecheck_expr(expr->as.cast.expr, sym_table);
      if (!r.is_ok) {
        return r;
      }

      struct TypecheckResult coerce_r = coerce_expr_to_type(
          expr->as.cast.expr, expr->as.cast.target_type, "Invalid cast");
      if (!coerce_r.is_ok) {
        return coerce_r;
      }

      expr->type = clone_type(expr->as.cast.target_type);
      break;
    }
    case EXPR_LITERAL: {
      if (expr->as.literal.kind == LITERAL_STR) {
        expr->type = (Type){.kind = STR_T};
      }
      break;
    }
    case EXPR_UNARY: {
      struct TypecheckResult r;

      r = typecheck_expr(expr->as.unary.expr, sym_table);
      if (!r.is_ok) {
        return r;
      }

      expr->type = expr->as.unary.expr->type;

      if (*expr->as.unary.op == '-') {
        switch (expr->type.kind) {
          case U8_T:
            expr->type.kind = I8_T;
            break;
          case U16_T:
            expr->type.kind = I16_T;
            break;
          case U32_T:
            expr->type.kind = I32_T;
            break;
          case U64_T:
            expr->type.kind = I64_T;
            break;
          default:
            break;
        }
      } else if (*expr->as.unary.op == '!') {
        if (expr->type.kind != BOOL_T) {
          return (struct TypecheckResult){
              .is_ok = false,
              .msg =
                  "Type error: logical NOT (!) requires a boolean expression",
              .ast = NULL};
        }
      } else if (*expr->as.unary.op == '~') {
        if (!is_integer_type(expr->type.kind)) {
          return (struct TypecheckResult){
              .is_ok = false,
              .msg =
                  "Type error: bitwise NOT (~) requires an integer expression",
              .ast = NULL};
        }
      }

      break;
    }
    case EXPR_VARIABLE: {
      struct Symbol *sym = sym_get(sym_table, expr->as.var.name);

      printf("looking up: %s\n", expr->as.var.name);
      if (!sym) {
        return (struct TypecheckResult){
            .is_ok = false,
            .msg = "Referenced an undefined variable or function",
            .ast = NULL};
      }

      if (sym->type.kind == UNKNOWN_T) {
        return (struct TypecheckResult){
            .is_ok = false, .msg = "Referenced an unknown type", .ast = NULL};
      }

      expr->type = clone_type(sym->type);

      break;
    }
    case EXPR_BINARY: {
      struct TypecheckResult lhs_res =
          typecheck_expr(expr->as.binary.lhs, sym_table);
      if (!lhs_res.is_ok) {
        return lhs_res;
      }

      struct TypecheckResult rhs_res =
          typecheck_expr(expr->as.binary.rhs, sym_table);
      if (!rhs_res.is_ok) {
        return rhs_res;
      }

      if (expr->as.binary.kind == EXPR_BIN_LOGICAL_AND ||
          expr->as.binary.kind == EXPR_BIN_LOGICAL_OR) {
        expr->type = (Type){.kind = BOOL_T};
        break;
      }

      if (is_bitwise_binop(expr->as.binary.kind)) {
        if (!is_integer_type(expr->as.binary.lhs->type.kind) ||
            !is_integer_type(expr->as.binary.rhs->type.kind)) {
          return (struct TypecheckResult){
              .is_ok = false,
              .msg = "Type error: bitwise operators require integer operands",
              .ast = NULL};
        }

        if (is_shift_binop(expr->as.binary.kind)) {
          expr->type = expr->as.binary.lhs->type;
        } else {
          Type common_type =
              get_common_type(expr->as.binary.lhs, expr->as.binary.rhs);
          if (common_type.kind == UNKNOWN_T) {
            return (struct TypecheckResult){
                .is_ok = false,
                .msg = "Unable to compute common type",
                .ast = NULL};
          }
          struct TypecheckResult lhs_coerce = coerce_expr_to_type(
              expr->as.binary.lhs, common_type,
              "Type error: bitwise lhs does not fit in the common type");
          if (!lhs_coerce.is_ok) {
            return lhs_coerce;
          }

          struct TypecheckResult rhs_coerce = coerce_expr_to_type(
              expr->as.binary.rhs, common_type,
              "Type error: bitwise rhs does not fit in the common type");
          if (!rhs_coerce.is_ok) {
            return rhs_coerce;
          }

          expr->type = common_type;
        }
        break;
      }

      Type common_type;

      common_type = get_common_type(expr->as.binary.lhs, expr->as.binary.rhs);
      if (common_type.kind == UNKNOWN_T) {
        return (struct TypecheckResult){.is_ok = false,
                                        .msg = "Unable to compute common type",
                                        .ast = NULL};
      }

      expr->type = common_type;

      break;
    }
    case EXPR_ASSIGN: {
      struct TypecheckResult lhs_res =
          typecheck_expr(expr->as.assign.lhs, sym_table);
      if (!lhs_res.is_ok) {
        return lhs_res;
      }

      struct TypecheckResult rhs_res =
          typecheck_expr(expr->as.assign.rhs, sym_table);
      if (!rhs_res.is_ok) {
        return rhs_res;
      }

      if (expr->as.assign.lhs->kind != EXPR_VARIABLE &&
          expr->as.assign.lhs->kind != EXPR_DEREF &&
          expr->as.assign.lhs->kind != EXPR_MEMBER) {
        return (struct TypecheckResult){
            .is_ok = false,
            .msg = "Invalid assignment target: left side must be a variable",
            .ast = NULL};
      }

      if (!is_expr_mutable(expr->as.assign.lhs, sym_table)) {
        return (struct TypecheckResult){
            .is_ok = false,
            .msg = "Type error: cannot assign to an immutable variable",
            .ast = NULL};
      }

      Type actual_type = expr->as.assign.rhs->type;
      Type expected_type = expr->as.assign.lhs->type;

      if (!types_equal(actual_type, expected_type)) {
        bool is_literal = (expr->as.assign.rhs->kind == EXPR_LITERAL &&
                           expr->as.assign.rhs->as.literal.kind == LITERAL_NUM);
        bool is_unary_literal =
            (expr->as.assign.rhs->kind == EXPR_UNARY &&
             expr->as.assign.rhs->as.unary.expr->kind == EXPR_LITERAL &&
             expr->as.assign.rhs->as.unary.expr->as.literal.kind ==
                 LITERAL_NUM);

        if (is_literal || is_unary_literal) {
          if (!promote_literal(expr->as.assign.rhs, expected_type)) {
            return (struct TypecheckResult){
                .is_ok = false,
                .msg =
                    "Type error: assignment does not fit in the expected type",
                .ast = NULL,
            };
          }
        } else {
          struct Expr *inner = malloc(sizeof(struct Expr));
          *inner = *expr->as.assign.rhs;

          expr->as.assign.rhs->kind = EXPR_CAST;
          expr->as.assign.rhs->type = expected_type;
          expr->as.assign.rhs->as.cast.expr = inner;
        }
      }

      expr->type = expr->as.assign.lhs->type;
      break;
    }
    case EXPR_COMPOUND_ASSIGN: {
      struct TypecheckResult lhs_res =
          typecheck_expr(expr->as.compound_assign.lhs, sym_table);
      if (!lhs_res.is_ok) {
        return lhs_res;
      }

      struct TypecheckResult rhs_res =
          typecheck_expr(expr->as.compound_assign.rhs, sym_table);
      if (!rhs_res.is_ok) {
        return rhs_res;
      }

      if (expr->as.compound_assign.lhs->kind != EXPR_VARIABLE &&
          expr->as.compound_assign.lhs->kind != EXPR_DEREF &&
          expr->as.compound_assign.lhs->kind != EXPR_MEMBER) {
        return (struct TypecheckResult){
            .is_ok = false,
            .msg =
                "Invalid compound assignment target: left side must be a "
                "variable",
            .ast = NULL};
      }

      if (!is_expr_mutable(expr->as.compound_assign.lhs, sym_table)) {
        return (struct TypecheckResult){
            .is_ok = false,
            .msg = "Type error: cannot assign to an immutable variable",
            .ast = NULL};
      }

      if (is_bitwise_binop(expr->as.compound_assign.kind) &&
          (!is_integer_type(expr->as.compound_assign.lhs->type.kind) ||
           !is_integer_type(expr->as.compound_assign.rhs->type.kind))) {
        return (struct TypecheckResult){
            .is_ok = false,
            .msg =
                "Type error: compound bitwise assignment requires integer "
                "operands",
            .ast = NULL};
      }

      Type actual_type = expr->as.compound_assign.rhs->type;
      Type expected_type = expr->as.compound_assign.lhs->type;

      if (!types_equal(actual_type, expected_type)) {
        bool is_literal =
            (expr->as.compound_assign.rhs->kind == EXPR_LITERAL &&
             expr->as.compound_assign.rhs->as.literal.kind == LITERAL_NUM);
        bool is_unary_literal =
            (expr->as.compound_assign.rhs->kind == EXPR_UNARY &&
             expr->as.compound_assign.rhs->as.unary.expr->kind ==
                 EXPR_LITERAL &&
             expr->as.compound_assign.rhs->as.unary.expr->as.literal.kind ==
                 LITERAL_NUM);

        if (is_literal || is_unary_literal) {
          if (!promote_literal(expr->as.compound_assign.rhs, expected_type)) {
            return (struct TypecheckResult){
                .is_ok = false,
                .msg =
                    "Type error: compound assignment does not fit in the "
                    "expected type",
                .ast = NULL,
            };
          }
        } else {
          struct Expr *inner = malloc(sizeof(struct Expr));
          *inner = *expr->as.compound_assign.rhs;

          expr->as.compound_assign.rhs->kind = EXPR_CAST;
          expr->as.compound_assign.rhs->type = expected_type;
          expr->as.compound_assign.rhs->as.cast.expr = inner;
        }
      }

      expr->type = expr->as.compound_assign.lhs->type;
      break;
    }
    case EXPR_CALL: {
      struct Symbol *callee_sym =
          sym_get(sym_table, expr->as.call.target->as.var.name);

      if (!callee_sym) {
        return (struct TypecheckResult){
            .is_ok = false, .msg = "Called an undefined function", .ast = NULL};
      }

      bool is_variadic;
      int expected_args, provided_args;

      is_variadic = callee_sym->type.as.func.is_variadic;
      expected_args = callee_sym->type.as.func.params.len;
      provided_args = expr->as.call.arguments.len;

      if (is_variadic ? (provided_args < expected_args)
                      : (provided_args != expected_args)) {
        return (struct TypecheckResult){
            .is_ok = false,
            .msg = "Called with a wrong number of args",
            .ast = NULL};
      }

      for (int i = 0; i < expr->as.call.arguments.len; i++) {
        struct TypecheckResult arg_res =
            typecheck_expr(&expr->as.call.arguments.data[i], sym_table);

        if (!arg_res.is_ok) {
          return arg_res;
        }

        Type actual_type = expr->as.call.arguments.data[i].type;

        if (i < expected_args) {
          Type expected_type = callee_sym->type.as.func.params.data[i];

          if (!types_equal(actual_type, expected_type)) {
            bool is_literal =
                expr->as.call.arguments.data[i].kind == EXPR_LITERAL &&
                expr->as.call.arguments.data[i].as.literal.kind == LITERAL_NUM;

            bool is_unary_literal =
                expr->as.call.arguments.data[i].kind == EXPR_UNARY &&
                expr->as.call.arguments.data[i].as.unary.expr->kind ==
                    EXPR_LITERAL &&
                expr->as.call.arguments.data[i]
                        .as.unary.expr->as.literal.kind == LITERAL_NUM;

            if (is_literal || is_unary_literal) {
              if (!promote_literal(&expr->as.call.arguments.data[i],
                                   expected_type)) {
                return (struct TypecheckResult){
                    .is_ok = false,
                    .msg =
                        "Type error: argument literal value does not fit in "
                        "the expected type",
                    .ast = NULL,
                };
              }
            } else {
              struct Expr *inner = malloc(sizeof(struct Expr));
              *inner = expr->as.call.arguments.data[i];

              struct Expr cast_expr;
              cast_expr.kind = EXPR_CAST;
              cast_expr.type = clone_type(expected_type);
              cast_expr.as.cast.expr = inner;

              expr->as.call.arguments.data[i] = cast_expr;
            }
          }
        } else {
          if (actual_type.kind == I8_T || actual_type.kind == I16_T ||
              actual_type.kind == BOOL_T) {
            struct Expr *inner = malloc(sizeof(struct Expr));
            *inner = expr->as.call.arguments.data[i];

            struct Expr cast_expr;
            cast_expr.kind = EXPR_CAST;
            cast_expr.type = (Type){.kind = I32_T};
            cast_expr.as.cast.expr = inner;

            expr->as.call.arguments.data[i] = cast_expr;
          } else if (actual_type.kind == U8_T || actual_type.kind == U16_T) {
            struct Expr *inner = malloc(sizeof(struct Expr));
            *inner = expr->as.call.arguments.data[i];

            struct Expr cast_expr;
            cast_expr.kind = EXPR_CAST;
            cast_expr.type = (Type){.kind = U32_T};
            cast_expr.as.cast.expr = inner;

            expr->as.call.arguments.data[i] = cast_expr;
          } else if (actual_type.kind == F32_T) {
            struct Expr *inner = malloc(sizeof(struct Expr));
            *inner = expr->as.call.arguments.data[i];

            struct Expr cast_expr;
            cast_expr.kind = EXPR_CAST;
            cast_expr.type = (Type){.kind = F64_T};
            cast_expr.as.cast.expr = inner;

            expr->as.call.arguments.data[i] = cast_expr;
          }
        }
      }

      expr->type = clone_type(*callee_sym->type.as.func.retval);

      break;
    }
  }
  return res;
}

struct TypecheckResult typecheck_stmt(struct Stmt *stmt,
                                      struct Symbol **sym_table)
{
  struct TypecheckResult res = {.is_ok = true, .msg = NULL, .ast = NULL};

  switch (stmt->kind) {
    case STMT_LOOP: {
      struct TypecheckResult r;

      r = typecheck_stmt(stmt->as.loop.body, sym_table);
      if (!r.is_ok) {
        return r;
      }

      break;
    }
    case STMT_STRUCT: {
      struct StructDef def;
      def.name = stmt->as.struct_stmt.name;
      def.fields = stmt->as.struct_stmt.fields; /* sharing the pointer */
      def.size = 0;
      def.alignment = 1;
      def.is_union = stmt->as.struct_stmt.is_union;

      /* Calculate offsets with x86_64 ABI padding */
      for (int i = 0; i < def.fields.len; i++) {
        int field_size, field_align;
        get_type_size_and_align(&def.fields.data[i].type, &field_size,
                                &field_align);

        if (field_size == -1) {
          return (struct TypecheckResult){
              .is_ok = false, .msg = "Struct/Union field has invalid type"};
        }

        if (def.is_union) {
          /* Union: All fields start at offset 0 */
          stmt->as.struct_stmt.fields.data[i].offset = 0;
          def.fields.data[i].offset = 0;

          /* Size and alignment are simply the maximum of all fields */
          if (field_size > def.size) {
            def.size = field_size;
          }
          if (field_align > def.alignment) {
            def.alignment = field_align;
          }
        } else {
          /* Pad the current size to match the field's required alignment */
          if (def.size % field_align != 0) {
            def.size += field_align - (def.size % field_align);
          }

          /* Write back the calculated offset to the AST */
          stmt->as.struct_stmt.fields.data[i].offset = def.size;
          def.fields.data[i].offset = def.size; /* Update our table copy */

          def.size += field_size;

          /* Struct alignment is the largest alignment of its fields */
          if (field_align > def.alignment) {
            def.alignment = field_align;
          }
        }
      }

      /* Final padding so arrays of this struct align properly */
      if (def.size % def.alignment != 0) {
        def.size += def.alignment - (def.size % def.alignment);
      }

      struct_insert(&struct_table, def);
      break;
    }
    case STMT_EXTERN: {
      Type t = {0};
      VecType param_types = {0};
      for (int i = 0; i < stmt->as.extern_stmt.params.len; i++) {
        vec_insert(&param_types,
                   clone_type(stmt->as.extern_stmt.params.data[i].type));
      }

      t.kind = FN_T;
      t.as.func.retval = malloc(sizeof(Type));
      *t.as.func.retval = clone_type(stmt->as.extern_stmt.retval);
      t.as.func.params = param_types;
      t.as.func.is_variadic = stmt->as.extern_stmt.is_variadic;

      sym_insert(sym_table, stmt->as.extern_stmt.name, t, false);
      break;
    }
    case STMT_FN: {
      if (sym_table) {
        Type t;

        VecType types = {0};
        for (int i = 0; i < stmt->as.fn.params.len; i++) {
          vec_insert(&types, clone_type(stmt->as.fn.params.data[i].type));
        }

        t.kind = FN_T;
        t.as.func.retval = malloc(sizeof(Type));
        *t.as.func.retval = clone_type(stmt->as.fn.retval);
        t.as.func.params = types;

        sym_insert(sym_table, stmt->as.fn.name, clone_type(t), false);
      }

      struct Symbol *fn_sym_table = sym_table ? *sym_table : NULL;
      struct Symbol *outer_sym = fn_sym_table;

      for (int i = 0; i < stmt->as.fn.params.len; i++) {
        sym_insert(&fn_sym_table, stmt->as.fn.params.data[i].name,
                   clone_type(stmt->as.fn.params.data[i].type),
                   stmt->as.fn.params.data[i].is_mut);
      }

      for (int i = 0; i < stmt->as.fn.body.len; i++) {
        res = typecheck_stmt(&stmt->as.fn.body.data[i], &fn_sym_table);
        if (!res.is_ok) {
          return res;
        }
      }

      while (fn_sym_table && fn_sym_table != outer_sym) {
        struct Symbol *tmp = fn_sym_table;
        fn_sym_table = fn_sym_table->next;
        free_symbol(tmp);
      }

      break;
    }
    case STMT_BLOCK: {
      for (int i = 0; i < stmt->as.block.stmts.len; i++) {
        res = typecheck_stmt(&stmt->as.block.stmts.data[i], sym_table);
        if (!res.is_ok) {
          return res;
        }
      }
      break;
    }
    case STMT_LET: {
      res = typecheck_expr(stmt->as.let.init, *sym_table);
      if (!res.is_ok) {
        return res;
      }

      Type actual_type = stmt->as.let.init->type;
      Type expected_type = stmt->as.let.type;

      if (!types_equal(actual_type, expected_type)) {
        bool is_literal = (stmt->as.let.init->kind == EXPR_LITERAL &&
                           stmt->as.let.init->as.literal.kind == LITERAL_NUM);
        bool is_unary_literal =
            (stmt->as.let.init->kind == EXPR_UNARY &&
             stmt->as.let.init->as.unary.expr->kind == EXPR_LITERAL &&
             stmt->as.let.init->as.unary.expr->as.literal.kind == LITERAL_NUM);

        if (is_literal || is_unary_literal) {
          if (!promote_literal(stmt->as.let.init, expected_type)) {
            return (struct TypecheckResult){
                .is_ok = false,
                .msg = "Type error: let init does not fit in the expected type",
                .ast = NULL,
            };
          }
        } else {
          struct Expr *inner = malloc(sizeof(struct Expr));
          *inner = *stmt->as.let.init;

          stmt->as.let.init->kind = EXPR_CAST;
          stmt->as.let.init->type = expected_type;
          stmt->as.let.init->as.cast.expr = inner;
        }
      }
      sym_insert(sym_table, stmt->as.let.name, clone_type(stmt->as.let.type),
                 stmt->as.let.is_mut);
      break;
    }

    case STMT_RET: {
      if (stmt->as.ret.val) {
        res = typecheck_expr(stmt->as.ret.val, *sym_table);
        if (!res.is_ok) {
          return res;
        }

        Type actual_type = stmt->as.ret.val->type;
        Type expected_type = stmt->as.ret.expected_retval;

        if (!types_equal(actual_type, expected_type)) {
          bool is_literal = (stmt->as.ret.val->kind == EXPR_LITERAL &&
                             stmt->as.ret.val->as.literal.kind == LITERAL_NUM);
          bool is_unary_literal =
              (stmt->as.ret.val->kind == EXPR_UNARY &&
               stmt->as.ret.val->as.unary.expr->kind == EXPR_LITERAL &&
               stmt->as.ret.val->as.unary.expr->as.literal.kind == LITERAL_NUM);

          if (is_literal || is_unary_literal) {
            if (!promote_literal(stmt->as.ret.val, expected_type)) {
              return (struct TypecheckResult){
                  .is_ok = false,
                  .msg =
                      "Type error: returned literal value does not fit in the "
                      "expected return type",
                  .ast = NULL,
              };
            }
          }
        }
      }
      break;
    }
    case STMT_IF: {
      struct TypecheckResult cond_res, then_res, else_res;

      cond_res = typecheck_expr(&stmt->as.if_stmt.cond, *sym_table);
      if (!cond_res.is_ok) {
        return cond_res;
      }

      then_res = typecheck_stmt(stmt->as.if_stmt.then_block, sym_table);
      if (!then_res.is_ok) {
        return then_res;
      }

      if (stmt->as.if_stmt.else_block) {
        else_res = typecheck_stmt(stmt->as.if_stmt.else_block, sym_table);
        if (!else_res.is_ok) {
          return else_res;
        }
      }

      break;
    }
    case STMT_EXPR: {
      struct TypecheckResult r;

      r = typecheck_expr(&stmt->as.expr_stmt.expr, *sym_table);
      if (!r.is_ok) {
        return r;
      }

      break;
    }
    case STMT_WHILE: {
      struct TypecheckResult cond_res, body_res;

      cond_res = typecheck_expr(&stmt->as.while_stmt.cond, *sym_table);
      if (!cond_res.is_ok) {
        return cond_res;
      }

      body_res = typecheck_stmt(stmt->as.while_stmt.body, sym_table);
      if (!body_res.is_ok) {
        return body_res;
      }

      break;
    }
    case STMT_LABELED: {
      struct TypecheckResult r;

      r = typecheck_stmt(stmt->as.labeled.stmt, sym_table);
      if (!r.is_ok) {
        return r;
      }

      break;
    }
    case STMT_BREAK:
    case STMT_CONTINUE:
    case STMT_ENUM:
    case STMT_GOTO:
      break;
    default:
      assert(0);
  }

  return res;
}

struct TypecheckResult typecheck(struct AST *ast)
{
  struct Symbol *global_sym = NULL;

  for (int i = 0; i < ast->stmts.len; i++) {
    struct TypecheckResult r;
    r = typecheck_stmt(&ast->stmts.data[i], &global_sym);
    if (!r.is_ok) {
      while (global_sym) {
        struct Symbol *tmp = global_sym;
        global_sym = global_sym->next;
        free_symbol(tmp);
      }
      return r;
    }
  }

  while (global_sym) {
    struct Symbol *tmp = global_sym;
    global_sym = global_sym->next;
    free_symbol(tmp);
  }

  return (struct TypecheckResult){.is_ok = true, .msg = NULL, .ast = ast};
}

enum AsmOperandKind {
  AsmOperand_IMM,
  AsmOperand_PSEUDO,
  AsmOperand_REG,
  AsmOperand_STACK,
  AsmOperand_DATA,
  AsmOperand_MEMORY,
  AsmOperand_INDEXED,
};

enum AsmRegister {
  AX,
  BX,
  CX,
  DX,
  SI,
  DI,
  R8,
  R9,
  R10,
  R11,
  R12,
  R13,
  R14,
  R15,
  BP,
  SP,
  XMM0,
  XMM1,
  XMM2,
  XMM3,
  XMM4,
  XMM5,
  XMM6,
  XMM7,
  XMM8,
  XMM9,
  XMM10,
  XMM11,
  XMM12,
  XMM13,
  XMM14,
  XMM15,
};

enum AsmTypeKind {
  AsmType_BYTE,
  AsmType_WORD,
  AsmType_LONGWORD,
  AsmType_QUADWORD,
  AsmType_FLOAT,
  AsmType_DOUBLE,
  AsmType_BYTE_ARRAY,
};

struct AsmType {
  enum AsmTypeKind kind;
  union {
    struct {
      int size;
      int alignment;
    } bytearray;
  } as;
};

static bool asm_type_can_live_in_int_reg(struct AsmType type)
{
  switch (type.kind) {
    case AsmType_BYTE:
    case AsmType_WORD:
    case AsmType_LONGWORD:
    case AsmType_QUADWORD:
      return true;

    case AsmType_FLOAT:
    case AsmType_DOUBLE:
    case AsmType_BYTE_ARRAY:
      return false;
  }

  assert(0 && "unhandled asm type");
}

void print_asm_type(struct AsmType type)
{
  switch (type.kind) {
    case AsmType_BYTE:
      printf("AsmType_BYTE");
      break;
    case AsmType_WORD:
      printf("AsmType_WORD");
      break;
    case AsmType_LONGWORD:
      printf("AsmType_LONGWORD");
      break;
    case AsmType_QUADWORD:
      printf("AsmType_QUADWORD");
      break;
    default:
      assert(0);
  }
}

struct AsmType type_to_asm_type(Type type)
{
  switch (type.kind) {
    case U8_T:
    case I8_T:
    case BOOL_T:
      return (struct AsmType){.kind = AsmType_BYTE};
    case U16_T:
    case I16_T:
      return (struct AsmType){.kind = AsmType_WORD};
    case U32_T:
    case I32_T:
      return (struct AsmType){.kind = AsmType_LONGWORD};
    case U64_T:
    case I64_T:
    case PTR_T:
      return (struct AsmType){.kind = AsmType_QUADWORD};
    case F32_T:
      return (struct AsmType){.kind = AsmType_FLOAT};
    case F64_T:
      return (struct AsmType){.kind = AsmType_DOUBLE};
    case STRUCT_T: {
      int size, alignment;
      get_type_size_and_align(&type, &size, &alignment);
      return (struct AsmType){
          .kind = AsmType_BYTE_ARRAY,
          .as.bytearray = {.size = size, .alignment = alignment}};
    }
    default:
      return (struct AsmType){.kind = AsmType_QUADWORD};
  }
}

static inline int asm_type_stack_size(struct AsmType t)
{
  switch (t.kind) {
    case AsmType_BYTE:
      return 1;
    case AsmType_WORD:
      return 2;
    case AsmType_LONGWORD:
    case AsmType_FLOAT:
      return 4;
    case AsmType_QUADWORD:
    case AsmType_DOUBLE:
      return 8;
    case AsmType_BYTE_ARRAY:
      return ((t.as.bytearray.size + 7) / 8) * 8;
  }

  assert(0);
}

static inline int asm_type_stack_align(struct AsmType t)
{
  switch (t.kind) {
    case AsmType_BYTE:
      return 1;
    case AsmType_WORD:
      return 2;
    case AsmType_LONGWORD:
    case AsmType_FLOAT:
      return 4;
    case AsmType_QUADWORD:
    case AsmType_DOUBLE:
      return 8;
    case AsmType_BYTE_ARRAY:
      return t.as.bytearray.alignment;
  }

  assert(0);
}

static inline int align_up_int(int n, int align)
{
  assert(align > 0);
  return ((n + align - 1) / align) * align;
}

struct AsmOperand {
  enum AsmOperandKind kind;
  struct AsmType asm_type;
  union {
    long long imm;
    char *pseudo;
    enum AsmRegister reg;
    int stack_offset;
    char *data;
    struct {
      enum AsmRegister base;
      int offset;
    } mem;
    struct {
      enum AsmRegister base;
      enum AsmRegister index;
      int scale;
    } indexed;
  } as;
};

static const char *reg_to_str_64(enum AsmRegister reg)
{
  switch (reg) {
    case AX:
      return "%rax";
    case DI:
      return "%rdi";
    case SI:
      return "%rsi";
    case DX:
      return "%rdx";
    case CX:
      return "%rcx";
    case R8:
      return "%r8";
    case R9:
      return "%r9";
    case R10:
      return "%r10";
    case BP:
      return "%rbp";
    case SP:
      return "%rsp";
    default:
      return "";
  }
}

void print_asm_operand(struct AsmOperand *op)
{
  switch (op->kind) {
    case AsmOperand_INDEXED: {
      printf("AsmOperand_INDEXED((%s, %s, %d)",
             reg_to_str_64(op->as.indexed.base),
             reg_to_str_64(op->as.indexed.index), op->as.indexed.scale);
      break;
    }
    case AsmOperand_MEMORY: {
      printf("AsmOperand_MEMORY(offset = %d, base = ", op->as.mem.offset);
      switch (op->as.mem.base) {
        case AX:
          printf("%%rax");
          break;
        case BX:
          printf("%%rbx");
          break;
        case DI:
          printf("%%rdi");
          break;
        case SI:
          printf("%%rsi");
          break;
        case DX:
          printf("%%rdx");
          break;
        case CX:
          printf("%%rcx");
          break;
        case R8:
          printf("%%r8");
          break;
        case R9:
          printf("%%r9");
          break;
        case R10:
          printf("%%r10");
          break;
        case R11:
          printf("%%r11");
          break;
        case R12:
          printf("%%r12");
          break;
        case R13:
          printf("%%r13");
          break;
        case R14:
          printf("%%r14");
          break;
        case R15:
          printf("%%r15");
          break;
        case BP:
          printf("%%rbp");
          break;
        case SP:
          printf("%%rsp");
          break;
        default:
          assert(0);
      }
      printf(")");
      break;
    }
    case AsmOperand_IMM: {
      printf("AsmOperand_IMM(%lld)", op->as.imm);
      break;
    }
    case AsmOperand_PSEUDO: {
      printf("AsmOperand_PSEUDO(%s)", op->as.pseudo);
      break;
    }
    case AsmOperand_REG: {
      printf("AsmOperand_REG(");

      switch (op->as.reg) {
        case XMM0: {
          printf("%%xmm0");
          break;
        }
        case XMM1: {
          printf("%%xmm1");
          break;
        }
        case XMM2: {
          printf("%%xmm2");
          break;
        }
        case XMM3: {
          printf("%%xmm3");
          break;
        }
        case XMM4: {
          printf("%%xmm4");
          break;
        }
        case XMM5: {
          printf("%%xmm5");
          break;
        }
        case XMM6: {
          printf("%%xmm6");
          break;
        }
        case XMM7: {
          printf("%%xmm7");
          break;
        }
        case XMM8: {
          printf("%%xmm8");
          break;
        }
        case XMM9: {
          printf("%%xmm9");
          break;
        }
        case XMM10: {
          printf("%%xmm10");
          break;
        }
        case XMM11: {
          printf("%%xmm11");
          break;
        }
        case XMM12: {
          printf("%%xmm12");
          break;
        }
        case XMM13: {
          printf("%%xmm13");
          break;
        }
        case XMM14: {
          printf("%%xmm14");
          break;
        }
        case XMM15: {
          printf("%%xmm15");
          break;
        }
        case AX: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%al");
              break;
            }
            case AsmType_WORD: {
              printf("%%ax");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%eax");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%rax");
              break;
            }
            default:
              assert(0);
          }

          break;
        }
        case BX: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%bl");
              break;
            }
            case AsmType_WORD: {
              printf("%%bx");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%ebx");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%rbx");
              break;
            }
            default:
              assert(0);
          }

          break;
        }
        case DI: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%dil");
              break;
            }
            case AsmType_WORD: {
              printf("%%di");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%edi");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%rdi");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case SI: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%sil");
              break;
            }
            case AsmType_WORD: {
              printf("%%si");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%esi");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%rsi");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case DX: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%dl");
              break;
            }
            case AsmType_WORD: {
              printf("%%dx");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%edx");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%rdx");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case CX: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%cl");
              break;
            }
            case AsmType_WORD: {
              printf("%%cx");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%ecx");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%rcx");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case R8: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%r8b");
              break;
            }
            case AsmType_WORD: {
              printf("%%r8w");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%r8d");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%r8");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case R9: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%r9b");
              break;
            }
            case AsmType_WORD: {
              printf("%%r9w");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%r9d");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%r9");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case R10: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%r10b");
              break;
            }
            case AsmType_WORD: {
              printf("%%r10w");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%r10d");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%r10");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case R11: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%r11b");
              break;
            }
            case AsmType_WORD: {
              printf("%%r11w");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%r11d");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%r11");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case R12: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%r12b");
              break;
            }
            case AsmType_WORD: {
              printf("%%r12w");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%r12d");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%r12");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case R13: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%r13b");
              break;
            }
            case AsmType_WORD: {
              printf("%%r13w");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%r13d");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%r13");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case R14: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%r14b");
              break;
            }
            case AsmType_WORD: {
              printf("%%r14w");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%r14d");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%r14");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case R15: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%r15b");
              break;
            }
            case AsmType_WORD: {
              printf("%%r15w");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%r15d");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%r15");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case BP: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%bpl");
              break;
            }
            case AsmType_WORD: {
              printf("%%bp");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%ebp");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%rbp");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case SP: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%spl");
              break;
            }
            case AsmType_WORD: {
              printf("%%sp");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%esp");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%rsp");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
      }

      printf(")");

      break;
    }
    case AsmOperand_STACK: {
      printf("AsmOperand_STACK(offset = %d)", op->as.stack_offset);
      break;
    }
    case AsmOperand_DATA: {
      printf("AsmOperand_DATA(name = %s)", op->as.data);
      break;
    }
    default:
      assert(0);
  }
}

enum AsmInstrKind {
  AsmInstr_PUSH,
  AsmInstr_POP,
  AsmInstr_MOV,
  AsmInstr_BIN,
  AsmInstr_RET,
  AsmInstr_CALL,
  AsmInstr_JMP,
  AsmInstr_LBL,
  AsmInstr_CMP,
  AsmInstr_JmpCC,
  AsmInstr_SetCC,
  AsmInstr_LEA,
  AsmInstr_UNARY,
  AsmInstr_CVT,
  AsmInstr_REP_MOVSB,
};

struct AsmInstrMov {
  struct AsmOperand src;
  struct AsmOperand dst;
};

enum AsmInstrBinaryKind {
  AsmInstrBinary_ADD,
  AsmInstrBinary_SUB,
  AsmInstrBinary_MUL,
  AsmInstrBinary_DIV,
  AsmInstrBinary_LESS,
  AsmInstrBinary_LESS_EQUAL,
  AsmInstrBinary_GREATER,
  AsmInstrBinary_GREATER_EQUAL,
  AsmInstrBinary_EQUAL_EQUAL,
  AsmInstrBinary_BANG_EQUAL,
  AsmInstrBinary_BIT_AND,
  AsmInstrBinary_BIT_XOR,
  AsmInstrBinary_BIT_OR,
  AsmInstrBinary_SHL,
  AsmInstrBinary_SHR,
  AsmInstrBinary_SAR,
};

struct AsmInstrBinary {
  enum AsmInstrBinaryKind kind;
  struct AsmOperand lhs;
  struct AsmOperand rhs;
};

struct AsmInstrRet {
  short __dummy;
};

struct AsmInstrPush {
  struct AsmOperand op;
};

struct AsmInstrPop {
  struct AsmOperand op;
};

struct AsmInstrCall {
  char *target;
};

struct AsmInstrJmp {
  char *target;
};

enum ConditionCode {
  /* signed */
  CC_E,  /* equal */
  CC_NE, /* not equal */
  CC_L,  /* less, */
  CC_LE, /* less or equal */
  CC_G,  /* greater */
  CC_GE, /* greater or equal */

  /* unsigned */
  CC_A,  /* above */
  CC_AE, /* above or equal */
  CC_B,  /* below */
  CC_BE, /* below or equal */
};

struct AsmInstrJmpCC {
  enum ConditionCode cc;
  char *target;
};

struct AsmInstrSetCC {
  enum ConditionCode cc;
  struct AsmOperand op;
};

struct AsmInstrCmp {
  struct AsmType asm_type;
  struct AsmOperand lhs;
  struct AsmOperand rhs;
};

struct AsmInstrLabel {
  char *name;
};

struct AsmInstrLea {
  struct AsmOperand src;
  struct AsmOperand dst;
};

enum AsmInstrUnaryKind {
  AsmInstrUnary_NEG,
  AsmInstrUnary_BIT_NOT,
};

struct AsmInstrUnary {
  enum AsmInstrUnaryKind kind;
  struct AsmOperand op;
};

enum AsmCastKind {
  AsmCast_SignExtend,
  AsmCast_ZeroExtend,
  AsmCast_FloatPromote,
  AsmCast_FloatDemote,
  AsmCast_IntToFloat,
  AsmCast_IntToDouble,
  AsmCast_FloatToInt,
  AsmCast_DoubleToInt,
};

struct AsmInstrCvt {
  enum AsmCastKind kind;
  struct AsmOperand src;
  struct AsmOperand dst;
};

struct AsmInstr {
  enum AsmInstrKind kind;
  struct AsmType asm_type;
  union {
    struct AsmInstrMov mov;
    struct AsmInstrBinary binary;
    struct AsmInstrRet ret;
    struct AsmInstrPush push;
    struct AsmInstrPop pop;
    struct AsmInstrCall call;
    struct AsmInstrJmp jmp;
    struct AsmInstrJmpCC jmpcc;
    struct AsmInstrSetCC setcc;
    struct AsmInstrCmp cmp;
    struct AsmInstrLabel lbl;
    struct AsmInstrLea lea;
    struct AsmInstrUnary unary;
    struct AsmInstrCvt cvt;
  } as;
};

typedef Vector(struct AsmInstr) VecAsmInstr;

struct AsmFunction {
  char *name;
  VecAsmInstr body;
};

typedef Vector(struct AsmFunction) VecAsmFunction;

struct AsmProgram {
  VecAsmFunction funcs;
};

void print_condition_code(enum ConditionCode cc)
{
  switch (cc) {
    case CC_E:
      printf("E");
      break;
    case CC_NE:
      printf("NE");
      break;
    case CC_L:
      printf("L");
      break;
    case CC_LE:
      printf("LE");
      break;
    case CC_G:
      printf("G");
      break;
    case CC_GE:
      printf("GE");
      break;
    case CC_A:
      printf("A");
      break;
    case CC_AE:
      printf("AE");
      break;
    case CC_B:
      printf("B");
      break;
    case CC_BE:
      printf("BE");
      break;
    default:
      assert(0);
  }
}

void print_asm_binary_op(enum AsmInstrBinaryKind kind)
{
  switch (kind) {
    case AsmInstrBinary_ADD:
      printf("ADD");
      break;
    case AsmInstrBinary_SUB:
      printf("SUB");
      break;
    case AsmInstrBinary_MUL:
      printf("MUL");
      break;
    case AsmInstrBinary_DIV:
      printf("DIV");
      break;
    case AsmInstrBinary_LESS:
      printf("LESS");
      break;
    case AsmInstrBinary_LESS_EQUAL:
      printf("LESS EQUAL");
      break;
    case AsmInstrBinary_GREATER:
      printf("GREATER");
      break;
    case AsmInstrBinary_GREATER_EQUAL:
      printf("GREATER EQUAL");
      break;
    case AsmInstrBinary_EQUAL_EQUAL:
      printf("EQUAL EQUAL");
      break;
    case AsmInstrBinary_BANG_EQUAL:
      printf("BANG EQUAL");
      break;
    case AsmInstrBinary_BIT_AND:
      printf("BITWISE AND");
      break;
    case AsmInstrBinary_BIT_XOR:
      printf("BITWISE XOR");
      break;
    case AsmInstrBinary_BIT_OR:
      printf("BITWISE OR");
      break;
    case AsmInstrBinary_SHL:
      printf("SHIFT LEFT");
      break;
    case AsmInstrBinary_SHR:
      printf("SHIFT RIGHT LOGICAL");
      break;
    case AsmInstrBinary_SAR:
      printf("SHIFT RIGHT ARITHMETIC");
      break;
    default:
      assert(0 && "Unhandled AsmInstrBinaryKind");
  }
}

void print_asm_instr(struct AsmInstr *instr, int spaces)
{
  print_indent(spaces);

  switch (instr->kind) {
    case AsmInstr_PUSH: {
      printf("AsmInstr_PUSH(\n");
      print_indent(spaces + 2);
      printf("op = ");
      print_asm_operand(&instr->as.push.op);
      printf(",\n");
      print_indent(spaces);
      printf("),\n");
      break;
    }
    case AsmInstr_POP: {
      printf("AsmInstr_POP(\n");
      print_indent(spaces + 2);
      printf("op = ");
      print_asm_operand(&instr->as.pop.op);
      printf(",\n");
      print_indent(spaces);
      printf("),\n");
      break;
    }
    case AsmInstr_MOV: {
      printf("AsmInstr_MOV(\n");
      print_indent(spaces + 2);
      printf("src = ");
      print_asm_operand(&instr->as.mov.src);
      printf(",\n");
      print_indent(spaces + 2);
      printf("dst = ");
      print_asm_operand(&instr->as.mov.dst);
      printf(",\n");
      print_indent(spaces);
      printf("),\n");
      break;
    }
    case AsmInstr_BIN: {
      printf("AsmInstr_BIN(\n");
      print_indent(spaces + 2);
      printf("kind = ");
      print_asm_binary_op(instr->as.binary.kind);
      printf(",\n");
      print_indent(spaces + 2);
      printf("lhs = ");
      print_asm_operand(&instr->as.binary.lhs);
      printf(",\n");
      print_indent(spaces + 2);
      printf("rhs = ");
      print_asm_operand(&instr->as.binary.rhs);
      printf(",\n");
      print_indent(spaces);
      printf("),\n");
      break;
    }
    case AsmInstr_RET: {
      printf("AsmInstr_RET,\n");
      break;
    }
    case AsmInstr_CALL: {
      printf("AsmInstr_CALL(target = %s),\n", instr->as.call.target);
      break;
    }
    case AsmInstr_JMP: {
      printf("AsmInstr_JMP(target = %s),\n", instr->as.jmp.target);
      break;
    }
    case AsmInstr_LBL: {
      printf("AsmInstr_LBL(name = %s),\n", instr->as.lbl.name);
      break;
    }
    case AsmInstr_CMP: {
      printf("AsmInstr_CMP(\n");
      print_indent(spaces + 2);
      printf("asm_type = ");
      print_asm_type(instr->as.cmp.asm_type);
      printf(",\n");
      print_indent(spaces + 2);
      printf("lhs = ");
      print_asm_operand(&instr->as.cmp.lhs);
      printf(",\n");
      print_indent(spaces + 2);
      printf("rhs = ");
      print_asm_operand(&instr->as.cmp.rhs);
      printf(",\n");
      print_indent(spaces);
      printf(",)\n");
      break;
    }
    case AsmInstr_JmpCC: {
      printf("AsmInstr_JmpCC(\n");
      print_indent(spaces + 2);
      printf("cc = ");
      print_condition_code(instr->as.jmpcc.cc);
      printf(",\n");
      print_indent(spaces + 2);
      printf("target = %s,\n", instr->as.jmpcc.target);
      print_indent(spaces);
      printf("),\n");
      break;
    }
    case AsmInstr_SetCC: {
      printf("AsmInstr_SetCC(\n");
      print_indent(spaces + 2);
      printf("cc = ");
      print_condition_code(instr->as.setcc.cc);
      printf(",\n");
      print_indent(spaces + 2);
      printf("op = ");
      print_asm_operand(&instr->as.setcc.op);
      printf(",\n");
      print_indent(spaces);
      printf("),\n");
      break;
    }

    case AsmInstr_LEA: {
      printf("AsmInstr_LEA(\n");
      print_indent(spaces + 2);
      printf("src = ");
      print_asm_operand(&instr->as.lea.src);
      printf(",\n");
      print_indent(spaces + 2);
      printf("dst = ");
      print_asm_operand(&instr->as.lea.dst);
      printf(",\n");
      print_indent(spaces);
      printf("),\n");
      break;
    }
    case AsmInstr_UNARY: {
      printf("AsmInstr_UNARY(\n");
      print_indent(spaces + 2);
      printf("op = ");
      print_asm_operand(&instr->as.unary.op);
      printf(",\n");
      print_indent(spaces);
      printf("),\n");
      break;
    }
    case AsmInstr_CVT: {
      printf("AsmInstr_CVT(\n");
      print_indent(spaces + 2);
      printf("src = ");
      print_asm_operand(&instr->as.cvt.src);
      printf(",\n");
      print_indent(spaces + 2);
      printf("dst = ");
      print_asm_operand(&instr->as.cvt.dst);
      printf(",\n");
      print_indent(spaces);
      printf("),\n");
      break;
    }
    case AsmInstr_REP_MOVSB: {
      printf("AsmInstr_REP_MOVSB,\n");
      break;
    }
  }
}

void free_asm_instr(struct AsmInstr *instr)
{
  if (instr->kind == AsmInstr_LEA) {
    if (instr->as.lea.src.kind == AsmOperand_DATA) {
      free(instr->as.lea.src.as.data);
    }
    if (instr->as.lea.dst.kind == AsmOperand_DATA) {
      free(instr->as.lea.dst.as.data);
    }
  }
}

void print_asm_fn(struct AsmFunction *fn)
{
  printf("AsmFunction(\n");
  print_indent(2);
  printf("name = %s,\n", fn->name);
  print_indent(2);
  printf("body: [\n");

  for (int i = 0; i < fn->body.len; i++) {
    print_asm_instr(&fn->body.data[i], 4);
  }

  print_indent(2);
  printf("]\n)\n");
}

void free_asm_fn(struct AsmFunction *fn)
{
  for (int i = 0; i < fn->body.len; i++) {
    free_asm_instr(&fn->body.data[i]);
  }
  vec_free(&fn->body);
}

void print_asm(struct AsmProgram *prog)
{
  for (int i = 0; i < prog->funcs.len; i++) {
    print_asm_fn(&prog->funcs.data[i]);
  }
}

void free_asm(struct AsmProgram *prog)
{
  for (int i = 0; i < prog->funcs.len; i++) {
    free_asm_fn(&prog->funcs.data[i]);
  }
  vec_free(&prog->funcs);
}

struct AsmOperand codegen_irvalue(struct IRValue *val)
{
  switch (val->kind) {
    case IRValue_CONST: {
      struct AsmOperand operand;
      operand.kind = AsmOperand_IMM;
      switch (val->type.kind) {
        case I8_T:
          operand.as.imm = val->as.konst.as.i8;
          break;
        case U8_T:
          operand.as.imm = val->as.konst.as.u8;
          break;
        case I16_T:
          operand.as.imm = val->as.konst.as.i16;
          break;
        case U16_T:
          operand.as.imm = val->as.konst.as.u16;
          break;
        case I32_T:
          operand.as.imm = val->as.konst.as.i32;
          break;
        case U32_T:
          operand.as.imm = val->as.konst.as.u32;
          break;
        case I64_T:
          operand.as.imm = val->as.konst.as.i64;
          break;
        case U64_T:
          operand.as.imm = val->as.konst.as.u64;
          break;
        case F32_T:
        case F64_T: {
          char *lbl = mklbl("LC", mktmp());
          struct StaticConstant sc;
          sc.name = strdup(lbl);

          char buf[64];
          if (val->type.kind == F32_T) {
            unsigned int bits;
            memcpy(&bits, &val->as.konst.as.f32, sizeof(float));
            snprintf(buf, sizeof(buf), ".long %u", bits);
          } else {
            unsigned long long bits;
            memcpy(&bits, &val->as.konst.as.f64, sizeof(double));
            snprintf(buf, sizeof(buf), ".quad %llu", bits);
          }

          sc.value = strdup(buf);
          vec_insert(&global_constants, sc);

          operand.kind = AsmOperand_DATA;
          operand.as.data = strdup(lbl);
          break;
        }
        case BOOL_T:
          operand.as.imm = val->as.konst.as.boolean ? 1 : 0;
          break;
        default:
          assert(0);
      }
      operand.asm_type = type_to_asm_type(val->type);
      return operand;
    }
    case IRValue_VAR: {
      struct AsmOperand operand;
      operand.kind = AsmOperand_PSEUDO;
      operand.as.pseudo = val->as.var;
      operand.asm_type = type_to_asm_type(val->type);
      return operand;
    }
    default:
      assert(0);
  }
}

bool is_comparison(enum IRInstrBinaryKind kind)
{
  return kind == IRInstrBinary_E || kind == IRInstrBinary_NE ||
         kind == IRInstrBinary_L || kind == IRInstrBinary_LE ||
         kind == IRInstrBinary_G || kind == IRInstrBinary_GE;
}

enum ABIClass { ABI_NO_CLASS, ABI_INTEGER, ABI_SSE, ABI_MEMORY };

struct ABIClassification {
  bool is_memory;
  enum ABIClass eightbytes[2];
};

struct ABIClassification classify_type(Type *type)
{
  struct ABIClassification result = {
      .is_memory = false, .eightbytes = {ABI_NO_CLASS, ABI_NO_CLASS}};

  int size, align;
  get_type_size_and_align(type, &size, &align);

  /* System V ABI: If size is > 16 (2 eightbytes), it goes to memory. */
  if (size > 16) {
    result.is_memory = true;
    return result;
  }

  /* Base case: primitives and pointers */
  if (type->kind != STRUCT_T) {
    if (type->kind == F32_T || type->kind == F64_T) {
      result.eightbytes[0] = ABI_SSE;
    } else {
      result.eightbytes[0] = ABI_INTEGER;
    }
    return result;
  }

  /* Recursive case: Structs */
  struct StructDef *def = struct_get(struct_table, type->as.struct_name);
  assert(def && "Tried to classify unknown struct");

  for (int i = 0; i < def->fields.len; i++) {
    struct StructField *field = &def->fields.data[i];
    int offset = field->offset;

    struct ABIClassification field_class = classify_type(&field->type);
    if (field_class.is_memory) {
      result.is_memory = true;
      return result;
    }

    int field_size, field_align;
    get_type_size_and_align(&field->type, &field_size, &field_align);

    int start_eightbyte = offset / 8;
    int end_eightbyte = (offset + field_size - 1) / 8;

    for (int eb = start_eightbyte; eb <= end_eightbyte; eb++) {
      int field_eb = eb - start_eightbyte;
      enum ABIClass c = field_class.eightbytes[field_eb];

      if (c == ABI_INTEGER) {
        result.eightbytes[eb] = ABI_INTEGER;
      } else if (c == ABI_SSE && result.eightbytes[eb] == ABI_NO_CLASS) {
        result.eightbytes[eb] = ABI_SSE;
      }
    }
  }

  return result;
}

bool is_sret(Type *retval)
{
  if (retval->kind != STRUCT_T) {
    return false;
  }

  int size, align;
  get_type_size_and_align(retval, &size, &align);
  return size > 16;
}

bool is_shift_asm_binary(enum AsmInstrBinaryKind kind)
{
  return kind == AsmInstrBinary_SHL || kind == AsmInstrBinary_SHR ||
         kind == AsmInstrBinary_SAR;
}

void codegen_instr(struct IRInstr *ir_instr, VecAsmInstr *instrs,
                   Type *fn_retval)
{
  switch (ir_instr->kind) {
    case IRInstr_BIN: {
      if (is_comparison(ir_instr->as.binary.kind)) {
        Type common = ir_instr->as.binary.dst->type;
        bool is_signed = !is_unsigned(common.kind);

        enum ConditionCode cc;

        if (is_signed) {
          switch (ir_instr->as.binary.kind) {
            case IRInstrBinary_E:
              cc = CC_E;
              break;
            case IRInstrBinary_NE:
              cc = CC_NE;
              break;
            case IRInstrBinary_L:
              cc = CC_L;
              break;
            case IRInstrBinary_G:
              cc = CC_G;
              break;
            case IRInstrBinary_LE:
              cc = CC_LE;
              break;
            case IRInstrBinary_GE:
              cc = CC_GE;
              break;
            default:
              assert(0 && "Unreachable or unhandled signed condition");
          }
        } else {
          switch (ir_instr->as.binary.kind) {
            case IRInstrBinary_E:
              cc = CC_E;
              break;
            case IRInstrBinary_NE:
              cc = CC_NE;
              break;
            case IRInstrBinary_L:
              cc = CC_B;
              break;
            case IRInstrBinary_G:
              cc = CC_A;
              break;
            case IRInstrBinary_LE:
              cc = CC_BE;
              break;
            case IRInstrBinary_GE:
              cc = CC_AE;
              break;
            default:
              assert(0 && "Unreachable or unhandled unsigned condition");
          }
        }

        struct AsmInstr cmp_instr = {0};
        cmp_instr.kind = AsmInstr_CMP;

        struct AsmOperand lhs;
        lhs = codegen_irvalue(ir_instr->as.binary.lhs);

        cmp_instr.as.cmp.asm_type = lhs.asm_type;
        cmp_instr.as.cmp.lhs = codegen_irvalue(ir_instr->as.binary.rhs);
        cmp_instr.as.cmp.rhs = lhs;

        struct AsmInstr mov_instr = {0};
        mov_instr.kind = AsmInstr_MOV;
        mov_instr.asm_type = type_to_asm_type(ir_instr->as.binary.dst->type);
        mov_instr.as.mov.src =
            (struct AsmOperand){.kind = AsmOperand_IMM, .as.imm = 0};
        mov_instr.as.mov.dst = codegen_irvalue(ir_instr->as.binary.dst);

        struct AsmInstr setcc = {0};
        setcc.kind = AsmInstr_SetCC;
        setcc.as.setcc.cc = cc;
        setcc.as.setcc.op = codegen_irvalue(ir_instr->as.binary.dst);

        vec_insert(instrs, cmp_instr);
        vec_insert(instrs, mov_instr);
        vec_insert(instrs, setcc);

        break;
      }

      enum AsmInstrBinaryKind kind;
      struct AsmOperand lhs, rhs, dst;

      switch (ir_instr->as.binary.kind) {
        case IRInstrBinary_ADD:
          kind = AsmInstrBinary_ADD;
          break;
        case IRInstrBinary_SUB:
          kind = AsmInstrBinary_SUB;
          break;
        case IRInstrBinary_MUL:
          kind = AsmInstrBinary_MUL;
          break;
        case IRInstrBinary_DIV:
          kind = AsmInstrBinary_DIV;
          break;
        case IRInstrBinary_BIT_AND:
          kind = AsmInstrBinary_BIT_AND;
          break;
        case IRInstrBinary_BIT_XOR:
          kind = AsmInstrBinary_BIT_XOR;
          break;
        case IRInstrBinary_BIT_OR:
          kind = AsmInstrBinary_BIT_OR;
          break;
        case IRInstrBinary_SHL:
          kind = AsmInstrBinary_SHL;
          break;
        case IRInstrBinary_SHR:
          kind = AsmInstrBinary_SHR;
          break;
        case IRInstrBinary_SAR:
          kind = AsmInstrBinary_SAR;
          break;
        default:
          assert(0);
      }

      lhs = codegen_irvalue(ir_instr->as.binary.lhs);
      rhs = codegen_irvalue(ir_instr->as.binary.rhs);
      dst = codegen_irvalue(ir_instr->as.binary.dst);

      struct AsmInstr i1 = {0}, i2 = {0};

      i1.kind = AsmInstr_MOV;
      i1.as.mov.src = lhs;
      i1.as.mov.dst = dst;
      i1.asm_type = dst.asm_type;

      i2.kind = AsmInstr_BIN;
      i2.as.binary.kind = kind;
      i2.as.binary.lhs = rhs;
      i2.as.binary.rhs = dst;
      i2.asm_type = dst.asm_type;

      vec_insert(instrs, i1);
      vec_insert(instrs, i2);

      break;
    }
    case IRInstr_UNARY: {
      struct AsmOperand src, dst;

      src = codegen_irvalue(ir_instr->as.unary.src);
      dst = codegen_irvalue(ir_instr->as.unary.dst);

      if (ir_instr->as.unary.kind == IRInstrUnary_NOT) {
        struct AsmInstr i1 = {0}, i2 = {0}, i3 = {0};

        i1.kind = AsmInstr_MOV;
        i1.as.mov.src = src;
        i1.as.mov.dst = dst;
        i1.asm_type = dst.asm_type;

        i2.kind = AsmInstr_CMP;
        i2.as.cmp.lhs =
            (struct AsmOperand){.kind = AsmOperand_IMM, .as.imm = 0};
        i2.as.cmp.rhs = dst;
        i2.asm_type = dst.asm_type;

        i3.kind = AsmInstr_SetCC;
        i3.as.setcc.cc = CC_E;
        i3.as.setcc.op = dst;

        vec_insert(instrs, i1);
        vec_insert(instrs, i2);
        vec_insert(instrs, i3);
      } else {
        struct AsmInstr i1 = {0}, i2 = {0};

        i1.kind = AsmInstr_MOV;
        i1.as.mov.src = src;
        i1.as.mov.dst = dst;
        i1.asm_type = dst.asm_type;

        i2.kind = AsmInstr_UNARY;
        i2.as.unary.kind = (ir_instr->as.unary.kind == IRInstrUnary_BIT_NOT)
                               ? AsmInstrUnary_BIT_NOT
                               : AsmInstrUnary_NEG;
        i2.as.unary.op = dst;
        i2.asm_type = dst.asm_type;

        vec_insert(instrs, i1);
        vec_insert(instrs, i2);
      }

      break;
    }
    case IRInstr_RET: {
      struct AsmInstrPop pop;
      struct AsmInstrMov mov;
      struct AsmInstrRet ret;
      struct AsmInstr e1 = {0}, e2 = {0}, i2 = {0};

      ret.__dummy = 0;

      if (ir_instr->as.ret.val) {
        struct AsmOperand retval = codegen_irvalue(ir_instr->as.ret.val);

        if (fn_retval && fn_retval->kind == STRUCT_T) {
          struct ABIClassification cls = classify_type(fn_retval);
          int size, align;
          get_type_size_and_align(fn_retval, &size, &align);

          if (is_sret(fn_retval)) {
            struct AsmOperand sret_slot = {
                .kind = AsmOperand_PSEUDO,
                .as.pseudo = "$__sret_ptr",
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

            struct AsmOperand rsi = {
                .kind = AsmOperand_REG,
                .as.reg = SI,
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

            struct AsmOperand rdi = {
                .kind = AsmOperand_REG,
                .as.reg = DI,
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

            struct AsmOperand rcx = {
                .kind = AsmOperand_REG,
                .as.reg = CX,
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

            struct AsmOperand rax = {
                .kind = AsmOperand_REG,
                .as.reg = AX,
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

            vec_insert(instrs, ((struct AsmInstr){
                                   .kind = AsmInstr_LEA,
                                   .as.lea = {.src = retval, .dst = rsi}}));

            vec_insert(
                instrs,
                ((struct AsmInstr){
                    .kind = AsmInstr_MOV,
                    .asm_type = (struct AsmType){.kind = AsmType_QUADWORD},
                    .as.mov = {.src = sret_slot, .dst = rdi}}));

            vec_insert(
                instrs,
                ((struct AsmInstr){
                    .kind = AsmInstr_MOV,
                    .asm_type = (struct AsmType){.kind = AsmType_QUADWORD},
                    .as.mov = {.src = {.kind = AsmOperand_IMM, .as.imm = size},
                               .dst = rcx}}));

            vec_insert(instrs, ((struct AsmInstr){.kind = AsmInstr_REP_MOVSB}));

            vec_insert(
                instrs,
                ((struct AsmInstr){
                    .kind = AsmInstr_MOV,
                    .asm_type = (struct AsmType){.kind = AsmType_QUADWORD},
                    .as.mov = {.src = sret_slot, .dst = rax}}));
          } else {
            enum AsmRegister int_ret_regs[] = {AX, DX};
            enum AsmRegister xmm_ret_regs[] = {XMM0, XMM1};

            int int_ret_idx = 0;
            int xmm_ret_idx = 0;
            int num_eb = (size + 7) / 8;

            struct AsmOperand r10 = {
                .kind = AsmOperand_REG,
                .as.reg = R10,
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

            vec_insert(instrs, ((struct AsmInstr){
                                   .kind = AsmInstr_LEA,
                                   .as.lea = {.src = retval, .dst = r10}}));

            for (int eb = 0; eb < num_eb; eb++) {
              bool is_sse = cls.eightbytes[eb] == ABI_SSE;

              struct AsmType eb_type =
                  is_sse ? (struct AsmType){.kind = AsmType_DOUBLE}
                         : (struct AsmType){.kind = AsmType_QUADWORD};

              enum AsmRegister reg = is_sse ? xmm_ret_regs[xmm_ret_idx++]
                                            : int_ret_regs[int_ret_idx++];

              struct AsmOperand mem_src = {
                  .kind = AsmOperand_MEMORY,
                  .as.mem = {.base = R10, .offset = eb * 8},
                  .asm_type = eb_type};

              struct AsmOperand dst_reg = {
                  .kind = AsmOperand_REG, .as.reg = reg, .asm_type = eb_type};

              vec_insert(instrs,
                         ((struct AsmInstr){
                             .kind = AsmInstr_MOV,
                             .asm_type = eb_type,
                             .as.mov = {.src = mem_src, .dst = dst_reg}}));
            }
          }
        } else {
          bool is_ret_float = retval.asm_type.kind == AsmType_FLOAT ||
                              retval.asm_type.kind == AsmType_DOUBLE;

          struct AsmOperand dst_reg = {.kind = AsmOperand_REG,
                                       .as.reg = is_ret_float ? XMM0 : AX,
                                       .asm_type = retval.asm_type};

          vec_insert(instrs, ((struct AsmInstr){
                                 .kind = AsmInstr_MOV,
                                 .asm_type = retval.asm_type,
                                 .as.mov = {.src = retval, .dst = dst_reg}}));
        }
      }

      mov.src = (struct AsmOperand){
          .kind = AsmOperand_REG,
          .as.reg = BP,
          .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

      mov.dst = (struct AsmOperand){
          .kind = AsmOperand_REG,
          .as.reg = SP,
          .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

      pop.op = (struct AsmOperand){
          .kind = AsmOperand_REG,
          .as.reg = BP,
          .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

      e1.kind = AsmInstr_MOV;
      e1.as.mov = mov;
      e1.asm_type = (struct AsmType){.kind = AsmType_QUADWORD};

      e2.kind = AsmInstr_POP;
      e2.as.pop = pop;

      i2.kind = AsmInstr_RET;
      i2.as.ret = ret;

      vec_insert(instrs, e1);
      vec_insert(instrs, e2);
      vec_insert(instrs, i2);

      break;
    }
    case IRInstr_CPY: {
      struct AsmOperand src = codegen_irvalue(ir_instr->as.copy.src);
      struct AsmOperand dst = codegen_irvalue(ir_instr->as.copy.dst);

      int size, align;
      get_type_size_and_align(&ir_instr->as.copy.src->type, &size, &align);

      if (ir_instr->as.copy.src->type.kind != STRUCT_T && size <= 8) {
        vec_insert(instrs,
                   ((struct AsmInstr){.kind = AsmInstr_MOV,
                                      .asm_type = src.asm_type,
                                      .as.mov = {.src = src, .dst = dst}}));
      } else {
        struct AsmOperand rsi = {
            .kind = AsmOperand_REG,
            .as.reg = SI,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        struct AsmOperand rdi = {
            .kind = AsmOperand_REG,
            .as.reg = DI,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        struct AsmOperand rcx = {
            .kind = AsmOperand_REG,
            .as.reg = CX,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        vec_insert(instrs,
                   ((struct AsmInstr){.kind = AsmInstr_LEA,
                                      .as.lea = {.src = src, .dst = rsi}}));

        vec_insert(instrs,
                   ((struct AsmInstr){.kind = AsmInstr_LEA,
                                      .as.lea = {.src = dst, .dst = rdi}}));

        vec_insert(
            instrs,
            ((struct AsmInstr){
                .kind = AsmInstr_MOV,
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD},
                .as.mov = {.src = {.kind = AsmOperand_IMM, .as.imm = size},
                           .dst = rcx}}));

        vec_insert(instrs, ((struct AsmInstr){.kind = AsmInstr_REP_MOVSB}));
      }

      break;
    }
    case IRInstr_CALL: {
      /* According to the SystemV ABI, the first six int arguments and pointers
       * are passed via the int regs, and the first eight floating point
       * arguments are passed via the SSE registers. */
      enum AsmRegister int_arg_regs[] = {DI, SI, DX, CX, R8, R9};
      enum AsmRegister xmm_arg_regs[] = {XMM0, XMM1, XMM2, XMM3,
                                         XMM4, XMM5, XMM6, XMM7};

      int num_args = ir_instr->as.call.args.len;

      /* If the function we are calling returns a large struct by value,
       * we must pass a hidden pointer to allocated memory as the first
       * argument. */
      bool call_has_sret =
          ir_instr->as.call.dst && is_sret(&ir_instr->as.call.dst->type);

      /* If there is an sret, it uses up %rdi, so normal integer arguments
       * start at index 1 instead of 0. */
      int int_reg_idx = call_has_sret ? 1 : 0;
      int xmm_reg_idx = 0;
      int num_stack_bytes = 0;

      struct ArgLocation {
        bool is_memory;
        int num_eightbytes;
        enum AsmRegister regs[2];
        struct AsmType asm_types[2];
      } *arg_locs = malloc(sizeof(struct ArgLocation) * num_args);

      /* First pass: Loop through every argument to classify it.
       * We need to figure out which arguments go into registers, which fall
       * to the stack, and how much total stack space we need to allocate. */
      for (int i = 0; i < num_args; i++) {
        struct IRValue *arg_val = ir_instr->as.call.args.data[i];
        struct ABIClassification cls = classify_type(&arg_val->type);

        int type_size, type_align;
        get_type_size_and_align(&arg_val->type, &type_size, &type_align);

        int num_eb = (type_size + 7) / 8;
        bool falls_to_memory = cls.is_memory;
        int needed_int = 0;
        int needed_xmm = 0;

        if (!falls_to_memory) {
          for (int eb = 0; eb < num_eb; eb++) {
            if (cls.eightbytes[eb] == ABI_INTEGER) {
              needed_int++;
            }

            if (cls.eightbytes[eb] == ABI_SSE) {
              needed_xmm++;
            }
          }

          if (int_reg_idx + needed_int > 6 || xmm_reg_idx + needed_xmm > 8) {
            falls_to_memory = true;
          }
        }

        arg_locs[i].is_memory = falls_to_memory;
        arg_locs[i].num_eightbytes = num_eb;

        if (falls_to_memory) {
          num_stack_bytes += align_up_int(type_size, 8);
        } else {
          for (int eb = 0; eb < num_eb; eb++) {
            if (cls.eightbytes[eb] == ABI_INTEGER) {
              arg_locs[i].regs[eb] = int_arg_regs[int_reg_idx++];
              arg_locs[i].asm_types[eb] =
                  (struct AsmType){.kind = AsmType_QUADWORD};
            } else if (cls.eightbytes[eb] == ABI_SSE) {
              arg_locs[i].regs[eb] = xmm_arg_regs[xmm_reg_idx++];
              arg_locs[i].asm_types[eb] =
                  (struct AsmType){.kind = AsmType_DOUBLE};
            }
          }
        }
      }

      /* The SystemV ABI requires the stack pointer (%rsp) to be 16-byte aligned
       * right before the `call` instruction is executed.
       * We calculate the padding needed to satisfy this requirement. */
      int stack_padding =
          num_stack_bytes % 16 != 0 ? 16 - (num_stack_bytes % 16) : 0;
      int total_stack_adjustment = num_stack_bytes + stack_padding;

      /* Allocate space on the stack for memory arguments and padding. */
      if (total_stack_adjustment != 0) {
        struct AsmInstr padding_instr = {0};

        padding_instr.kind = AsmInstr_BIN;
        padding_instr.asm_type = (struct AsmType){.kind = AsmType_QUADWORD};
        padding_instr.as.binary.kind = AsmInstrBinary_SUB;
        padding_instr.as.binary.lhs = (struct AsmOperand){
            .kind = AsmOperand_IMM, .as.imm = total_stack_adjustment};
        padding_instr.as.binary.rhs = (struct AsmOperand){
            .kind = AsmOperand_REG,
            .as.reg = SP,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        vec_insert(instrs, padding_instr);
      }

      int current_stack_offset = 0;

      /* Second pass: Push the stack-bound arguments into the space we just
       * allocated. We do this before loading register arguments to avoid
       * accidentally clobbering the argument registers if evaluating these
       * expressions is complex. */
      for (int i = 0; i < num_args; i++) {
        if (!arg_locs[i].is_memory) {
          continue;
        }

        struct AsmOperand src_op =
            codegen_irvalue(ir_instr->as.call.args.data[i]);

        int size, align;
        get_type_size_and_align(&ir_instr->as.call.args.data[i]->type, &size,
                                &align);

        struct AsmOperand dst_op = {
            .kind = AsmOperand_MEMORY,
            .as.mem = {.base = SP, .offset = current_stack_offset},
            .asm_type = src_op.asm_type};

        /* If it is a primitive or small struct (<= 8 bytes), we can just
         * use a mov via a scratch register. */
        if (ir_instr->as.call.args.data[i]->type.kind != STRUCT_T &&
            size <= 8) {
          struct AsmOperand scratch_reg = {.kind = AsmOperand_REG,
                                           .as.reg = R10,
                                           .asm_type = src_op.asm_type};

          vec_insert(instrs, ((struct AsmInstr){
                                 .kind = AsmInstr_MOV,
                                 .as.mov = {.src = src_op, .dst = scratch_reg},
                                 .asm_type = src_op.asm_type}));

          vec_insert(instrs, ((struct AsmInstr){
                                 .kind = AsmInstr_MOV,
                                 .as.mov = {.src = scratch_reg, .dst = dst_op},
                                 .asm_type = dst_op.asm_type}));
        } else {
          /* If it is a large struct, we need rep movsb to copy it to the stack.
           */
          struct AsmOperand rsi = {
              .kind = AsmOperand_REG,
              .as.reg = SI,
              .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

          struct AsmOperand rdi = {
              .kind = AsmOperand_REG,
              .as.reg = DI,
              .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

          struct AsmOperand rcx = {
              .kind = AsmOperand_REG,
              .as.reg = CX,
              .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

          vec_insert(instrs, ((struct AsmInstr){
                                 .kind = AsmInstr_LEA,
                                 .as.lea = {.src = src_op, .dst = rsi}}));

          vec_insert(instrs, ((struct AsmInstr){
                                 .kind = AsmInstr_LEA,
                                 .as.lea = {.src = dst_op, .dst = rdi}}));

          vec_insert(
              instrs,
              ((struct AsmInstr){
                  .kind = AsmInstr_MOV,
                  .asm_type = (struct AsmType){.kind = AsmType_QUADWORD},
                  .as.mov = {.src = {.kind = AsmOperand_IMM, .as.imm = size},
                             .dst = rcx}}));

          vec_insert(instrs, ((struct AsmInstr){.kind = AsmInstr_REP_MOVSB}));
        }

        current_stack_offset += align_up_int(size, 8);
      }

      /* If the function has an sret, load the address of our local memory
       * destination into %rdi (the hidden first argument). */
      if (call_has_sret) {
        struct AsmOperand dst_op = codegen_irvalue(ir_instr->as.call.dst);

        struct AsmOperand rdi = {
            .kind = AsmOperand_REG,
            .as.reg = DI,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        vec_insert(instrs,
                   ((struct AsmInstr){.kind = AsmInstr_LEA,
                                      .as.lea = {.src = dst_op, .dst = rdi}}));
      }

      /* Third pass: Load the arguments that fit into hardware registers. */
      for (int i = 0; i < num_args; i++) {
        if (arg_locs[i].is_memory) {
          continue;
        }

        struct AsmOperand src_op =
            codegen_irvalue(ir_instr->as.call.args.data[i]);

        if (arg_locs[i].num_eightbytes == 1) {
          if (ir_instr->as.call.args.data[i]->type.kind == STRUCT_T) {
            /* Small struct passed in a single register: move it from memory. */
            struct AsmOperand r10 = {
                .kind = AsmOperand_REG,
                .as.reg = R10,
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

            struct AsmOperand mem0 = {.kind = AsmOperand_MEMORY,
                                      .as.mem = {.base = R10, .offset = 0},
                                      .asm_type = arg_locs[i].asm_types[0]};

            struct AsmOperand dst_reg = {.kind = AsmOperand_REG,
                                         .as.reg = arg_locs[i].regs[0],
                                         .asm_type = arg_locs[i].asm_types[0]};

            vec_insert(instrs, ((struct AsmInstr){
                                   .kind = AsmInstr_LEA,
                                   .as.lea = {.src = src_op, .dst = r10}}));

            vec_insert(instrs, ((struct AsmInstr){
                                   .kind = AsmInstr_MOV,
                                   .asm_type = mem0.asm_type,
                                   .as.mov = {.src = mem0, .dst = dst_reg}}));
          } else {
            /* Normal primitive: move directly into the target register. */
            struct AsmOperand dst_reg = {.kind = AsmOperand_REG,
                                         .as.reg = arg_locs[i].regs[0],
                                         .asm_type = src_op.asm_type};

            vec_insert(instrs, ((struct AsmInstr){
                                   .kind = AsmInstr_MOV,
                                   .as.mov = {.src = src_op, .dst = dst_reg},
                                   .asm_type = src_op.asm_type}));
          }
        } else {
          /* Medium struct (9-16 bytes) passed split across two registers:
           * Load the first 8 bytes into the first reg, and the rest into the
           * second. */
          struct AsmOperand scratch_ptr = {
              .kind = AsmOperand_REG,
              .as.reg = R10,
              .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

          struct AsmOperand mem0 = {.kind = AsmOperand_MEMORY,
                                    .as.mem = {.base = R10, .offset = 0},
                                    .asm_type = arg_locs[i].asm_types[0]};

          struct AsmOperand dst_reg0 = {.kind = AsmOperand_REG,
                                        .as.reg = arg_locs[i].regs[0],
                                        .asm_type = arg_locs[i].asm_types[0]};

          struct AsmOperand mem1 = {.kind = AsmOperand_MEMORY,
                                    .as.mem = {.base = R10, .offset = 8},
                                    .asm_type = arg_locs[i].asm_types[1]};

          struct AsmOperand dst_reg1 = {.kind = AsmOperand_REG,
                                        .as.reg = arg_locs[i].regs[1],
                                        .asm_type = arg_locs[i].asm_types[1]};

          vec_insert(instrs,
                     ((struct AsmInstr){
                         .kind = AsmInstr_LEA,
                         .as.lea = {.src = src_op, .dst = scratch_ptr}}));

          vec_insert(instrs, ((struct AsmInstr){
                                 .kind = AsmInstr_MOV,
                                 .as.mov = {.src = mem0, .dst = dst_reg0},
                                 .asm_type = mem0.asm_type}));

          vec_insert(instrs, ((struct AsmInstr){
                                 .kind = AsmInstr_MOV,
                                 .as.mov = {.src = mem1, .dst = dst_reg1},
                                 .asm_type = mem1.asm_type}));
        }
      }

      /* For functions with variable arguments (varargs), the System V ABI
       * requires that the %al register contains the number of vector (SSE)
       * registers used. */
      struct AsmInstr eax_instr = {0};

      eax_instr.kind = AsmInstr_MOV;
      eax_instr.asm_type = (struct AsmType){.kind = AsmType_LONGWORD};
      eax_instr.as.mov.src =
          (struct AsmOperand){.kind = AsmOperand_IMM, .as.imm = xmm_reg_idx};
      eax_instr.as.mov.dst = (struct AsmOperand){
          .kind = AsmOperand_REG,
          .as.reg = AX,
          .asm_type = (struct AsmType){.kind = AsmType_LONGWORD}};

      vec_insert(instrs, eax_instr);

      /* Emit the actual CALL instruction. */
      struct AsmInstr call_instr = {0};
      call_instr.kind = AsmInstr_CALL;
      call_instr.as.call.target = ir_instr->as.call.target.as.var.name;

      vec_insert(instrs, call_instr);

      /* After the function returns, clean up the stack space we allocated
       * for the stack arguments and alignment padding. */
      if (total_stack_adjustment != 0) {
        struct AsmInstr cleanup_instr = {0};

        cleanup_instr.kind = AsmInstr_BIN;
        cleanup_instr.asm_type = (struct AsmType){.kind = AsmType_QUADWORD};
        cleanup_instr.as.binary.kind = AsmInstrBinary_ADD;
        cleanup_instr.as.binary.lhs = (struct AsmOperand){
            .kind = AsmOperand_IMM, .as.imm = total_stack_adjustment};
        cleanup_instr.as.binary.rhs = (struct AsmOperand){
            .kind = AsmOperand_REG,
            .as.reg = SP,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        vec_insert(instrs, cleanup_instr);
      }

      /* Handle the return value (if there is one, and it wasn't an sret). */
      if (ir_instr->as.call.dst && !call_has_sret) {
        struct AsmOperand dst_op = codegen_irvalue(ir_instr->as.call.dst);

        if (ir_instr->as.call.dst->type.kind == STRUCT_T) {
          /* If a struct is returned in registers, the ABI says it will be split
           * across %rax and %rdx (or %xmm0 and %xmm1). We must piece it back
           * together into our local memory destination. */
          struct ABIClassification cls =
              classify_type(&ir_instr->as.call.dst->type);

          int size, align;
          get_type_size_and_align(&ir_instr->as.call.dst->type, &size, &align);

          int num_eb = (size + 7) / 8;

          enum AsmRegister int_ret_regs[] = {AX, DX};
          enum AsmRegister xmm_ret_regs[] = {XMM0, XMM1};

          int int_ret_idx = 0;
          int xmm_ret_idx = 0;

          struct AsmOperand r10 = {
              .kind = AsmOperand_REG,
              .as.reg = R10,
              .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

          vec_insert(instrs, ((struct AsmInstr){
                                 .kind = AsmInstr_LEA,
                                 .as.lea = {.src = dst_op, .dst = r10}}));

          for (int eb = 0; eb < num_eb; eb++) {
            bool is_sse = cls.eightbytes[eb] == ABI_SSE;

            struct AsmType eb_type =
                is_sse ? (struct AsmType){.kind = AsmType_DOUBLE}
                       : (struct AsmType){.kind = AsmType_QUADWORD};

            enum AsmRegister reg = is_sse ? xmm_ret_regs[xmm_ret_idx++]
                                          : int_ret_regs[int_ret_idx++];

            struct AsmOperand src_reg = {
                .kind = AsmOperand_REG, .as.reg = reg, .asm_type = eb_type};

            struct AsmOperand mem_dst = {
                .kind = AsmOperand_MEMORY,
                .as.mem = {.base = R10, .offset = eb * 8},
                .asm_type = eb_type};

            vec_insert(instrs, ((struct AsmInstr){.kind = AsmInstr_MOV,
                                                  .asm_type = eb_type,
                                                  .as.mov = {.src = src_reg,
                                                             .dst = mem_dst}}));
          }
        } else {
          /* Standard primitive return: grab it from %rax (int) or %xmm0
           * (float). */
          struct AsmInstr mov_instr = {0};

          bool is_dst_float = dst_op.asm_type.kind == AsmType_FLOAT ||
                              dst_op.asm_type.kind == AsmType_DOUBLE;

          mov_instr.kind = AsmInstr_MOV;
          mov_instr.as.mov.src =
              (struct AsmOperand){.kind = AsmOperand_REG,
                                  .as.reg = is_dst_float ? XMM0 : AX,
                                  .asm_type = dst_op.asm_type};
          mov_instr.as.mov.dst = dst_op;
          mov_instr.asm_type = dst_op.asm_type;

          vec_insert(instrs, mov_instr);
        }
      }

      break;
    }
    case IRInstr_JMP: {
      struct AsmInstr i = {0};
      struct AsmInstrJmp jmp;

      jmp.target = ir_instr->as.jmp.target;

      i.kind = AsmInstr_JMP;
      i.as.jmp = jmp;

      vec_insert(instrs, i);
      break;
    }
    case IRInstr_JZ: {
      struct AsmInstr i1 = {0}, i2 = {0};
      struct AsmInstrCmp cmp;
      struct AsmOperand cond;

      cond = codegen_irvalue(&ir_instr->as.jz.cond);

      i1.kind = AsmInstr_CMP;
      cmp.asm_type = cond.asm_type;
      cmp.lhs = (struct AsmOperand){.kind = AsmOperand_IMM, .as.imm = 0};
      cmp.rhs = cond;
      i1.as.cmp = cmp;

      i2.kind = AsmInstr_JmpCC;
      i2.as.jmpcc.cc = CC_E;
      i2.as.jmpcc.target = ir_instr->as.jz.target;

      vec_insert(instrs, i1);
      vec_insert(instrs, i2);

      break;
    }
    case IRInstr_LBL: {
      struct AsmInstr i = {0};
      struct AsmInstrLabel lbl;

      lbl.name = ir_instr->as.label.name;

      i.kind = AsmInstr_LBL;
      i.as.lbl = lbl;

      vec_insert(instrs, i);
      break;
    }
    case IRInstr_GETADDR: {
      struct AsmInstr i = {0};

      i.kind = AsmInstr_LEA;
      i.asm_type = (struct AsmType){.kind = AsmType_QUADWORD};
      i.as.lea.src = codegen_irvalue(ir_instr->as.getaddr.src);
      i.as.lea.dst = codegen_irvalue(ir_instr->as.getaddr.dst);

      vec_insert(instrs, i);
      break;
    }
    case IRInstr_LOAD: {
      struct AsmOperand src_ptr = codegen_irvalue(ir_instr->as.load.src);
      struct AsmOperand dst_val = codegen_irvalue(ir_instr->as.load.dst);

      int size, align;
      get_type_size_and_align(&ir_instr->as.load.dst->type, &size, &align);

      if (ir_instr->as.load.dst->type.kind != STRUCT_T && size <= 8) {
        struct AsmOperand scratch_ptr = {
            .kind = AsmOperand_REG,
            .as.reg = R10,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        vec_insert(
            instrs,
            ((struct AsmInstr){
                .kind = AsmInstr_MOV,
                .as.mov = {.src = src_ptr, .dst = scratch_ptr},
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}}));

        struct AsmOperand mem_op = {.kind = AsmOperand_MEMORY,
                                    .as.mem = {.base = R10, .offset = 0},
                                    .asm_type = dst_val.asm_type};

        vec_insert(instrs,
                   ((struct AsmInstr){.kind = AsmInstr_MOV,
                                      .as.mov = {.src = mem_op, .dst = dst_val},
                                      .asm_type = dst_val.asm_type}));
      } else {
        struct AsmOperand rsi = {
            .kind = AsmOperand_REG,
            .as.reg = SI,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        struct AsmOperand rdi = {
            .kind = AsmOperand_REG,
            .as.reg = DI,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        struct AsmOperand rcx = {
            .kind = AsmOperand_REG,
            .as.reg = CX,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        vec_insert(instrs,
                   ((struct AsmInstr){.kind = AsmInstr_MOV,
                                      .as.mov = {.src = src_ptr, .dst = rsi},
                                      .asm_type = (struct AsmType){
                                          .kind = AsmType_QUADWORD}}));

        vec_insert(instrs,
                   ((struct AsmInstr){.kind = AsmInstr_LEA,
                                      .as.lea = {.src = dst_val, .dst = rdi}}));

        vec_insert(
            instrs,
            ((struct AsmInstr){
                .kind = AsmInstr_MOV,
                .as.mov = {.src = {.kind = AsmOperand_IMM, .as.imm = size},
                           .dst = rcx},
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}}));

        vec_insert(instrs, ((struct AsmInstr){.kind = AsmInstr_REP_MOVSB}));
      }

      break;
    }
    case IRInstr_STORE: {
      struct AsmOperand src_val = codegen_irvalue(ir_instr->as.store.val);
      struct AsmOperand dst_ptr = codegen_irvalue(ir_instr->as.store.dst);

      int size, align;
      get_type_size_and_align(&ir_instr->as.store.val->type, &size, &align);

      if (ir_instr->as.store.val->type.kind != STRUCT_T && size <= 8) {
        struct AsmOperand scratch_val = {
            .kind = AsmOperand_REG, .as.reg = R9, .asm_type = src_val.asm_type};

        vec_insert(instrs, ((struct AsmInstr){
                               .kind = AsmInstr_MOV,
                               .as.mov = {.src = src_val, .dst = scratch_val},
                               .asm_type = src_val.asm_type}));

        struct AsmOperand scratch_ptr = {
            .kind = AsmOperand_REG,
            .as.reg = R10,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        vec_insert(
            instrs,
            ((struct AsmInstr){
                .kind = AsmInstr_MOV,
                .as.mov = {.src = dst_ptr, .dst = scratch_ptr},
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}}));

        struct AsmOperand mem_op = {.kind = AsmOperand_MEMORY,
                                    .as.mem = {.base = R10, .offset = 0},
                                    .asm_type = src_val.asm_type};

        vec_insert(instrs, ((struct AsmInstr){
                               .kind = AsmInstr_MOV,
                               .as.mov = {.src = scratch_val, .dst = mem_op},
                               .asm_type = src_val.asm_type}));
      } else {
        struct AsmOperand rsi = {
            .kind = AsmOperand_REG,
            .as.reg = SI,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        struct AsmOperand rdi = {
            .kind = AsmOperand_REG,
            .as.reg = DI,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        struct AsmOperand rcx = {
            .kind = AsmOperand_REG,
            .as.reg = CX,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        vec_insert(instrs,
                   ((struct AsmInstr){.kind = AsmInstr_LEA,
                                      .as.lea = {.src = src_val, .dst = rsi}}));

        vec_insert(instrs,
                   ((struct AsmInstr){.kind = AsmInstr_MOV,
                                      .as.mov = {.src = dst_ptr, .dst = rdi},
                                      .asm_type = (struct AsmType){
                                          .kind = AsmType_QUADWORD}}));

        vec_insert(
            instrs,
            ((struct AsmInstr){
                .kind = AsmInstr_MOV,
                .as.mov = {.src = {.kind = AsmOperand_IMM, .as.imm = size},
                           .dst = rcx},
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}}));

        vec_insert(instrs, ((struct AsmInstr){.kind = AsmInstr_REP_MOVSB}));
      }

      break;
    }
    case IRInstr_CAST: {
      struct AsmOperand src = codegen_irvalue(ir_instr->as.cast.src);
      struct AsmOperand dst = codegen_irvalue(ir_instr->as.cast.dst);

      if (src.kind == AsmOperand_IMM) {
        struct AsmInstr i = {0};
        i.kind = AsmInstr_MOV;
        i.as.mov.src = src;
        i.as.mov.dst = dst;
        i.asm_type = dst.asm_type;
        vec_insert(instrs, i);
      } else if (ir_instr->as.cast.kind == IRCast_Truncate) {
        struct AsmOperand narrowed_src = src;
        narrowed_src.asm_type = dst.asm_type;
        struct AsmInstr i = {0};
        i.kind = AsmInstr_MOV;
        i.as.mov.src = narrowed_src;
        i.as.mov.dst = dst;
        i.asm_type = dst.asm_type;
        vec_insert(instrs, i);
      } else {
        enum IRCastKind ir_k = ir_instr->as.cast.kind;

        if (ir_k == IRCast_FloatToUInt) {
          ir_k = IRCast_FloatToInt;
        }
        if (ir_k == IRCast_DoubleToUInt) {
          ir_k = IRCast_DoubleToInt;
        }

        if (ir_k == IRCast_UIntToFloat || ir_k == IRCast_UIntToDouble) {
          int src_sz = asm_type_stack_size(src.asm_type);
          if (src_sz < 8) {
            struct AsmOperand r10 = {
                .kind = AsmOperand_REG,
                .as.reg = R10,
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};
            struct AsmInstr zext = {0};
            zext.kind = AsmInstr_CVT;
            zext.as.cvt.kind = AsmCast_ZeroExtend;
            zext.as.cvt.src = src;
            zext.as.cvt.dst = r10;
            vec_insert(instrs, zext);
            src = r10;
          }
          ir_k = (ir_k == IRCast_UIntToFloat) ? IRCast_IntToFloat
                                              : IRCast_IntToDouble;
        }

        enum AsmCastKind asm_k;
        switch (ir_k) {
          case IRCast_SignExtend:
            asm_k = AsmCast_SignExtend;
            break;
          case IRCast_ZeroExtend:
            asm_k = AsmCast_ZeroExtend;
            break;
          case IRCast_FloatPromote:
            asm_k = AsmCast_FloatPromote;
            break;
          case IRCast_FloatDemote:
            asm_k = AsmCast_FloatDemote;
            break;
          case IRCast_IntToFloat:
            asm_k = AsmCast_IntToFloat;
            break;
          case IRCast_IntToDouble:
            asm_k = AsmCast_IntToDouble;
            break;
          case IRCast_FloatToInt:
            asm_k = AsmCast_FloatToInt;
            break;
          case IRCast_DoubleToInt:
            asm_k = AsmCast_DoubleToInt;
            break;
          default:
            assert(0 && "Unhandled mapped cast variant");
        }

        struct AsmInstr i = {0};
        i.kind = AsmInstr_CVT;
        i.as.cvt.kind = asm_k;
        i.as.cvt.src = src;
        i.as.cvt.dst = dst;
        i.asm_type = dst.asm_type;
        vec_insert(instrs, i);
      }
      break;
    }
    case IRInstr_ADD_PTR: {
      struct AsmInstr mov1 = {0};
      mov1.kind = AsmInstr_MOV;
      mov1.asm_type = (struct AsmType){.kind = AsmType_QUADWORD};
      mov1.as.mov.src = codegen_irvalue(ir_instr->as.add_ptr.ptr);
      mov1.as.mov.dst = (struct AsmOperand){
          .kind = AsmOperand_REG,
          .as.reg = AX,
          .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};
      vec_insert(instrs, mov1);

      struct AsmInstr mov2 = {0};
      mov2.kind = AsmInstr_MOV;
      mov2.asm_type = (struct AsmType){.kind = AsmType_QUADWORD};
      mov2.as.mov.src = codegen_irvalue(ir_instr->as.add_ptr.index);
      mov2.as.mov.dst = (struct AsmOperand){
          .kind = AsmOperand_REG,
          .as.reg = DX,
          .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};
      vec_insert(instrs, mov2);

      int scale = ir_instr->as.add_ptr.scale;
      if (scale == 1 || scale == 2 || scale == 4 || scale == 8) {
        struct AsmInstr lea_instr = {0};
        lea_instr.kind = AsmInstr_LEA;
        lea_instr.asm_type = (struct AsmType){.kind = AsmType_QUADWORD};
        lea_instr.as.lea.src = (struct AsmOperand){
            .kind = AsmOperand_INDEXED,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD},
            .as.indexed = {.base = AX, .index = DX, .scale = scale}};
        lea_instr.as.lea.dst = codegen_irvalue(ir_instr->as.add_ptr.dst);
        vec_insert(instrs, lea_instr);
      } else {
        struct AsmInstr imul_instr = {0};
        imul_instr.kind = AsmInstr_BIN;
        imul_instr.asm_type = (struct AsmType){.kind = AsmType_QUADWORD};
        imul_instr.as.binary.kind = AsmInstrBinary_MUL;
        imul_instr.as.binary.lhs = (struct AsmOperand){
            .kind = AsmOperand_IMM,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD},
            .as.imm = scale};
        imul_instr.as.binary.rhs = (struct AsmOperand){
            .kind = AsmOperand_REG,
            .as.reg = DX,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};
        vec_insert(instrs, imul_instr);

        struct AsmInstr lea_fallback = {0};
        lea_fallback.kind = AsmInstr_LEA;
        lea_fallback.asm_type = (struct AsmType){.kind = AsmType_QUADWORD};
        lea_fallback.as.lea.src = (struct AsmOperand){
            .kind = AsmOperand_INDEXED,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD},
            .as.indexed = {.base = AX, .index = DX, .scale = 1}};
        lea_fallback.as.lea.dst = codegen_irvalue(ir_instr->as.add_ptr.dst);
        vec_insert(instrs, lea_fallback);
      }
      break;
    }
    case IRInstr_CPY_FROM_OFFSET: {
      struct AsmOperand src = codegen_irvalue(ir_instr->as.cpy_from_offset.src);
      struct AsmOperand dst = codegen_irvalue(ir_instr->as.cpy_from_offset.dst);

      int size, align;
      get_type_size_and_align(&ir_instr->as.cpy_from_offset.dst->type, &size,
                              &align);

      if (ir_instr->as.cpy_from_offset.dst->type.kind != STRUCT_T &&
          size <= 8) {
        struct AsmOperand scratch_reg = {
            .kind = AsmOperand_REG,
            .as.reg = R10,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        struct AsmInstr i1 = {0};
        i1.kind = AsmInstr_LEA;
        i1.as.lea.src = src;
        i1.as.lea.dst = scratch_reg;

        struct AsmOperand mem_op = {
            .kind = AsmOperand_MEMORY,
            .as.mem = {.base = R10,
                       .offset = ir_instr->as.cpy_from_offset.offset},
            .asm_type = dst.asm_type};

        struct AsmInstr i2 = {0};
        i2.kind = AsmInstr_MOV;
        i2.as.mov.src = mem_op;
        i2.as.mov.dst = dst;
        i2.asm_type = dst.asm_type;

        vec_insert(instrs, i1);
        vec_insert(instrs, i2);
      } else {
        struct AsmOperand r10 = {
            .kind = AsmOperand_REG,
            .as.reg = R10,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        struct AsmOperand rsi = {
            .kind = AsmOperand_REG,
            .as.reg = SI,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        struct AsmOperand rdi = {
            .kind = AsmOperand_REG,
            .as.reg = DI,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        struct AsmOperand rcx = {
            .kind = AsmOperand_REG,
            .as.reg = CX,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        vec_insert(instrs,
                   ((struct AsmInstr){.kind = AsmInstr_LEA,
                                      .as.lea = {.src = src, .dst = r10}}));

        struct AsmOperand mem_offset = {
            .kind = AsmOperand_MEMORY,
            .as.mem = {.base = R10,
                       .offset = ir_instr->as.cpy_from_offset.offset}};

        vec_insert(instrs, ((struct AsmInstr){
                               .kind = AsmInstr_LEA,
                               .as.lea = {.src = mem_offset, .dst = rsi}}));

        vec_insert(instrs,
                   ((struct AsmInstr){.kind = AsmInstr_LEA,
                                      .as.lea = {.src = dst, .dst = rdi}}));

        vec_insert(
            instrs,
            ((struct AsmInstr){
                .kind = AsmInstr_MOV,
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD},
                .as.mov = {.src = {.kind = AsmOperand_IMM, .as.imm = size},
                           .dst = rcx}}));

        vec_insert(instrs, ((struct AsmInstr){.kind = AsmInstr_REP_MOVSB}));
      }

      break;
    }
    case IRInstr_CPY_TO_OFFSET: {
      struct AsmOperand dst = codegen_irvalue(ir_instr->as.cpy_to_offset.dst);
      struct AsmOperand src = codegen_irvalue(ir_instr->as.cpy_to_offset.src);

      int size, align;
      get_type_size_and_align(&ir_instr->as.cpy_to_offset.src->type, &size,
                              &align);

      if (ir_instr->as.cpy_to_offset.src->type.kind != STRUCT_T && size <= 8) {
        struct AsmOperand scratch_reg = {
            .kind = AsmOperand_REG,
            .as.reg = R10,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        vec_insert(instrs, ((struct AsmInstr){
                               .kind = AsmInstr_LEA,
                               .as.lea = {.src = dst, .dst = scratch_reg}}));

        struct AsmOperand mem_op = {
            .kind = AsmOperand_MEMORY,
            .as.mem = {.base = R10,
                       .offset = ir_instr->as.cpy_to_offset.offset},
            .asm_type = src.asm_type};

        vec_insert(instrs,
                   ((struct AsmInstr){.kind = AsmInstr_MOV,
                                      .asm_type = src.asm_type,
                                      .as.mov = {.src = src, .dst = mem_op}}));
      } else {
        struct AsmOperand r10 = {
            .kind = AsmOperand_REG,
            .as.reg = R10,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        struct AsmOperand rsi = {
            .kind = AsmOperand_REG,
            .as.reg = SI,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        struct AsmOperand rdi = {
            .kind = AsmOperand_REG,
            .as.reg = DI,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        struct AsmOperand rcx = {
            .kind = AsmOperand_REG,
            .as.reg = CX,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        vec_insert(instrs,
                   ((struct AsmInstr){.kind = AsmInstr_LEA,
                                      .as.lea = {.src = src, .dst = rsi}}));

        vec_insert(instrs,
                   ((struct AsmInstr){.kind = AsmInstr_LEA,
                                      .as.lea = {.src = dst, .dst = r10}}));

        struct AsmOperand mem_offset = {
            .kind = AsmOperand_MEMORY,
            .as.mem = {.base = R10,
                       .offset = ir_instr->as.cpy_to_offset.offset}};

        vec_insert(instrs, ((struct AsmInstr){
                               .kind = AsmInstr_LEA,
                               .as.lea = {.src = mem_offset, .dst = rdi}}));

        vec_insert(
            instrs,
            ((struct AsmInstr){
                .kind = AsmInstr_MOV,
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD},
                .as.mov = {.src = {.kind = AsmOperand_IMM, .as.imm = size},
                           .dst = rcx}}));

        vec_insert(instrs, ((struct AsmInstr){.kind = AsmInstr_REP_MOVSB}));
      }

      break;
    }
    default:
      break;
  }
}

struct AsmFunction codegen_fn(struct IRFunction *ir_func)
{
  struct AsmFunction func = {0};
  struct AsmInstr p1, p2, p3;
  struct AsmInstrPush push;
  struct AsmInstrMov mov;
  struct AsmInstrBinary sub;

  func.name = ir_func->name;

  /*  In the prologue, we save the caller's BP.  */
  push.op = (struct AsmOperand){
      .kind = AsmOperand_REG,
      .as.reg = BP,
      .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

  p1.kind = AsmInstr_PUSH;
  p1.as.push = push;

  /*  ...and place our SP into BP.  */
  mov.src = (struct AsmOperand){
      .kind = AsmOperand_REG,
      .as.reg = SP,
      .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};
  mov.dst = (struct AsmOperand){
      .kind = AsmOperand_REG,
      .as.reg = BP,
      .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

  p2.kind = AsmInstr_MOV;
  p2.as.mov = mov;
  p2.asm_type = (struct AsmType){.kind = AsmType_QUADWORD};

  sub.kind = AsmInstrBinary_SUB;
  sub.lhs = (struct AsmOperand){.kind = AsmOperand_IMM, .as.imm = 0};
  sub.rhs = (struct AsmOperand){
      .kind = AsmOperand_REG,
      .as.reg = SP,
      .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

  p3.kind = AsmInstr_BIN;
  p3.as.binary = sub;
  p3.asm_type = (struct AsmType){.kind = AsmType_QUADWORD};

  vec_insert(&func.body, p1);
  vec_insert(&func.body, p2);
  vec_insert(&func.body, p3);

  /* When a function needs to return a large struct by value,
   * the struct wouldn't fit in the two regiters (%rax and %rdi),
   * which means it needs to be returned via the stack.
   *
   * This means that the caller will allocate space on its stack,
   * and pass the pointer to that space to the callee in %rdi.
   *
   * As the callee, we want to hold onto that address */
  bool fn_has_sret;

  fn_has_sret = is_sret(&ir_func->retval);
  if (fn_has_sret) {
    struct AsmOperand sret_slot = {
        .kind = AsmOperand_PSEUDO,
        .as.pseudo = "$__sret_ptr",
        .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

    struct AsmOperand rdi = {
        .kind = AsmOperand_REG,
        .as.reg = DI,
        .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

    vec_insert(&func.body,
               ((struct AsmInstr){
                   .kind = AsmInstr_MOV,
                   .asm_type = (struct AsmType){.kind = AsmType_QUADWORD},
                   .as.mov = {.src = rdi, .dst = sret_slot}}));
  }

  /* According to the SystemV ABI, the first six int arguments and pointers
   * are passed via the int regs, and the first eight floating point arguments
   * are passed via the SSE registers.  */
  enum AsmRegister int_arg_regs[] = {DI, SI, DX, CX, R8, R9};
  enum AsmRegister xmm_arg_regs[] = {XMM0, XMM1, XMM2, XMM3,
                                     XMM4, XMM5, XMM6, XMM7};

  int num_params = ir_func->params.len;

  /* If the fn has sret, this means that %rdi will have already been used up,
   * so we start at index 1.  */
  int int_reg_idx = fn_has_sret ? 1 : 0;
  int xmm_reg_idx = 0;

  /* In the SystemV ABI, during the call instruction, the CPU
   * will push the return address on the stack, which means that
   * this will decrement the stack pointer by 8.  Then an instruction
   * like `pushq %rbp`, will decrement the stack pointer by another 8.
   * Then we have `movq %rsp, %rbp`, which means that the return address
   * is at 8(%rbp), and first stack-passed argument will have been at 16(%rbp).
   * The locals are starting off at -8(%rbp).  */
  int stack_offset = 16;

  /* Loop through every parameter and classify it.
   * - How many 8-byte chunks ("eightbytes") the parameter takes?
   * - How many Integer vs. SSE registers this specific parameter needs?
   * - If passing this parameter would exceed the available registers (6 int,
   *   8 SSE), it forces falls_to_memory = true, meaning this argument must
   *   be fetched from the stack. */
  for (int i = 0; i < num_params; i++) {
    struct Parameter *param = &ir_func->params.data[i];
    struct ABIClassification cls = classify_type(&param->type);

    int size, align;
    get_type_size_and_align(&param->type, &size, &align);

    /* The math for the ceiling division below works out as follows:
     *
     * If size is 1 to 8 bytes: (size + 7) is 8 to 15. Integer division by 8
     * results in 1 eightbyte.
     *
     * If size is 9 to 16 bytes: (size + 7) is 16 to 23. Integer division by 8
     * results in 2 eightbytes.
     *
     * If size is 17 to 24 bytes: (size + 7) is 24 to 31. Integer division by 8
     * results in 3 eightbytes. */
    int num_eb = (size + 7) / 8;

    bool falls_to_memory = cls.is_memory;
    int needed_int = 0;
    int needed_xmm = 0;

    if (!falls_to_memory) {
      for (int eb = 0; eb < num_eb; eb++) {
        if (cls.eightbytes[eb] == ABI_INTEGER) {
          needed_int++;
        }

        if (cls.eightbytes[eb] == ABI_SSE) {
          needed_xmm++;
        }
      }

      if (int_reg_idx + needed_int > 6 || xmm_reg_idx + needed_xmm > 8) {
        falls_to_memory = true;
      }
    }

    struct AsmType param_asm_type = type_to_asm_type(param->type);

    struct AsmOperand dst;
    dst.kind = AsmOperand_PSEUDO;
    dst.as.pseudo = param->name;
    dst.asm_type = param_asm_type;

    if (falls_to_memory) {
      struct AsmOperand src_stack = {.kind = AsmOperand_STACK,
                                     .as.stack_offset = stack_offset,
                                     .asm_type = param_asm_type};

      /* If it is a struct, but smaller or equal to 8 bytes, then we can just
       * use mov. */
      if (param->type.kind != STRUCT_T && size <= 8) {
        struct AsmOperand scratch_reg = {
            .kind = AsmOperand_REG, .as.reg = R10, .asm_type = param_asm_type};

        vec_insert(
            &func.body,
            ((struct AsmInstr){.kind = AsmInstr_MOV,
                               .as.mov = {.src = src_stack, .dst = scratch_reg},
                               .asm_type = param_asm_type}));

        vec_insert(&func.body, ((struct AsmInstr){
                                   .kind = AsmInstr_MOV,
                                   .as.mov = {.src = scratch_reg, .dst = dst},
                                   .asm_type = param_asm_type}));
      } else {
        /* If it is a struct, but larger than 8 bytes, we need rep movsb. */
        struct AsmOperand rsi = {
            .kind = AsmOperand_REG,
            .as.reg = SI,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        struct AsmOperand rdi = {
            .kind = AsmOperand_REG,
            .as.reg = DI,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        struct AsmOperand rcx = {
            .kind = AsmOperand_REG,
            .as.reg = CX,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        vec_insert(&func.body, ((struct AsmInstr){
                                   .kind = AsmInstr_LEA,
                                   .as.lea = {.src = src_stack, .dst = rsi}}));

        vec_insert(&func.body,
                   ((struct AsmInstr){.kind = AsmInstr_LEA,
                                      .as.lea = {.src = dst, .dst = rdi}}));

        vec_insert(
            &func.body,
            ((struct AsmInstr){
                .kind = AsmInstr_MOV,
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD},
                .as.mov = {.src = {.kind = AsmOperand_IMM, .as.imm = size},
                           .dst = rcx}}));

        vec_insert(&func.body, ((struct AsmInstr){.kind = AsmInstr_REP_MOVSB}));
      }

      stack_offset += align_up_int(size, 8);
    } else {
      /* If it does NOT fall to memory */
      if (num_eb <= 1) {
        /* ...and there is at most a single eightbyte, */
        if (param->type.kind == STRUCT_T) {
          /* ...but it's still a struct. */
          bool is_sse = cls.eightbytes[0] == ABI_SSE;

          enum AsmRegister reg = is_sse ? xmm_arg_regs[xmm_reg_idx++]
                                        : int_arg_regs[int_reg_idx++];

          struct AsmType eb_type =
              is_sse ? (struct AsmType){.kind = AsmType_DOUBLE}
                     : (struct AsmType){.kind = AsmType_QUADWORD};

          struct AsmOperand r10 = {
              .kind = AsmOperand_REG,
              .as.reg = R10,
              .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

          struct AsmOperand src_reg = {
              .kind = AsmOperand_REG, .as.reg = reg, .asm_type = eb_type};

          struct AsmOperand mem_dst = {.kind = AsmOperand_MEMORY,
                                       .as.mem = {.base = R10, .offset = 0},
                                       .asm_type = eb_type};

          vec_insert(&func.body,
                     ((struct AsmInstr){.kind = AsmInstr_LEA,
                                        .as.lea = {.src = dst, .dst = r10}}));

          vec_insert(
              &func.body,
              ((struct AsmInstr){.kind = AsmInstr_MOV,
                                 .asm_type = eb_type,
                                 .as.mov = {.src = src_reg, .dst = mem_dst}}));
        } else {
          /* not a struct */
          enum AsmRegister reg = cls.eightbytes[0] == ABI_SSE
                                     ? xmm_arg_regs[xmm_reg_idx++]
                                     : int_arg_regs[int_reg_idx++];

          struct AsmOperand src_reg = {.kind = AsmOperand_REG,
                                       .as.reg = reg,
                                       .asm_type = param_asm_type};

          vec_insert(&func.body,
                     ((struct AsmInstr){.kind = AsmInstr_MOV,
                                        .as.mov = {.src = src_reg, .dst = dst},
                                        .asm_type = param_asm_type}));
        }
      } else {
        /* there is more than 1 eightbyte */
        struct AsmOperand r10 = {
            .kind = AsmOperand_REG,
            .as.reg = R10,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        vec_insert(&func.body,
                   ((struct AsmInstr){.kind = AsmInstr_LEA,
                                      .as.lea = {.src = dst, .dst = r10}}));

        for (int eb = 0; eb < num_eb; eb++) {
          enum AsmRegister reg = cls.eightbytes[eb] == ABI_SSE
                                     ? xmm_arg_regs[xmm_reg_idx++]
                                     : int_arg_regs[int_reg_idx++];

          struct AsmType eb_type =
              cls.eightbytes[eb] == ABI_SSE
                  ? (struct AsmType){.kind = AsmType_DOUBLE}
                  : (struct AsmType){.kind = AsmType_QUADWORD};

          struct AsmOperand src_reg = {
              .kind = AsmOperand_REG, .as.reg = reg, .asm_type = eb_type};

          struct AsmOperand mem_dst = {
              .kind = AsmOperand_MEMORY,
              .as.mem = {.base = R10, .offset = eb * 8},
              .asm_type = eb_type};

          vec_insert(&func.body, ((struct AsmInstr){
                                     .kind = AsmInstr_MOV,
                                     .as.mov = {.src = src_reg, .dst = mem_dst},
                                     .asm_type = eb_type}));
        }
      }
    }
  }

  for (int i = 0; i < ir_func->body.len; i++) {
    codegen_instr(&ir_func->body.data[i], &func.body, &ir_func->retval);
  }

  return func;
}

struct AsmResult {
  bool is_ok;
  char *msg;
  struct AsmProgram prog;
};

struct AsmResult codegen(struct IRProgram *ir_prog)
{
  struct AsmProgram prog = {0};
  struct AsmResult result;

  result.is_ok = true;
  result.msg = NULL;

  for (int i = 0; i < ir_prog->funcs.len; i++) {
    vec_insert(&prog.funcs, codegen_fn(ir_prog->funcs.data[i]));
  }

  result.prog = prog;

  return result;
}

struct Map {
  struct Map *next;
  char *name;
  int offset;
  struct AsmType asm_type;
};

static bool map_get_offset(struct Map *map, char *name, int *out_offset)
{
  for (struct Map *curr = map; curr; curr = curr->next) {
    if (curr->name && strcmp(curr->name, name) == 0) {
      *out_offset = curr->offset;
      return true;
    }
  }

  return false;
}

int get_offset(struct Map *map, char *name, struct AsmType asm_type,
               int *used_stack_bytes)
{
  struct Map *curr = map;

  while (curr) {
    if (curr->name && strcmp(curr->name, name) == 0) {
      return curr->offset;
    }

    if (!curr->next) {
      break;
    }

    curr = curr->next;
  }

  int size = asm_type_stack_size(asm_type);
  int align = asm_type_stack_align(asm_type);

  int aligned_used = align_up_int(*used_stack_bytes, align);
  int new_used = aligned_used + size;
  int new_offset = -new_used;

  struct Map *new_entry = malloc(sizeof(struct Map));
  assert(new_entry);

  new_entry->next = NULL;
  new_entry->name = name;
  new_entry->offset = new_offset;
  new_entry->asm_type = asm_type;

  curr->next = new_entry;
  *used_stack_bytes = new_used;

  return new_offset;
}

static inline void replace_operand_pseudo(struct AsmOperand *op,
                                          struct Map *map,
                                          int *used_stack_bytes)
{
  if (op->kind == AsmOperand_PSEUDO) {
    for (int k = 0; k < global_constants.len; k++) {
      if (strcmp(op->as.pseudo, global_constants.data[k].name) == 0) {
        op->kind = AsmOperand_DATA;
        op->as.data = strdup(op->as.pseudo);
        return;
      }
    }

    op->kind = AsmOperand_STACK;
    op->as.stack_offset =
        get_offset(map, op->as.pseudo, op->asm_type, used_stack_bytes);
  }
}

bool is_tmp(char *name)
{
  return strncmp(name, "tmp.", 4) == 0;
}

struct AllocatedReg {
  struct AllocatedReg *next;
  char *pseudo;
  enum AsmRegister reg;
};

static enum AsmRegister allocatable_int_regs[] = {
    AX, BX, DX, CX, SI, DI, R8, R9, R12, R13, R14, R15,
};

#define NUM_ALLOCATABLE_INT_REGS \
  ((int) (sizeof(allocatable_int_regs) / sizeof(allocatable_int_regs[0])))

struct RegAllocState {
  struct AllocatedReg *allocated_regs;
  bool reg_used[NUM_ALLOCATABLE_INT_REGS];
};

struct LiveInterval {
  char *pseudo;
  int start;
  int end;
  struct AsmType asm_type;
};

struct PseudoType {
  char *pseudo;
  struct AsmType asm_type;
};

typedef Vector(struct PseudoType) VecPseudoType;

static struct AsmType *pseudo_type_get(VecPseudoType *types, char *pseudo)
{
  for (int i = 0; i < types->len; i++) {
    if (strcmp(types->data[i].pseudo, pseudo) == 0) {
      return &types->data[i].asm_type;
    }
  }

  return NULL;
}

static void pseudo_type_add(VecPseudoType *types, char *pseudo,
                            struct AsmType asm_type)
{
  if (pseudo_type_get(types, pseudo)) {
    return;
  }

  struct PseudoType item;

  item.pseudo = strdup(pseudo);
  if (!item.pseudo) {
    perror("strdup");
    exit(1);
  }

  item.asm_type = asm_type;

  vec_insert(types, item);
}

static void remember_operand_type(VecPseudoType *types, struct AsmOperand *op)
{
  if (op->kind == AsmOperand_PSEUDO) {
    pseudo_type_add(types, op->as.pseudo, op->asm_type);
  }
}

static VecPseudoType collect_pseudo_types(struct AsmFunction *fn)
{
  VecPseudoType types = {0};

  for (int i = 0; i < fn->body.len; i++) {
    struct AsmInstr *instr = &fn->body.data[i];

    switch (instr->kind) {
      case AsmInstr_MOV:
        remember_operand_type(&types, &instr->as.mov.src);
        remember_operand_type(&types, &instr->as.mov.dst);
        break;

      case AsmInstr_BIN:
        remember_operand_type(&types, &instr->as.binary.lhs);
        remember_operand_type(&types, &instr->as.binary.rhs);
        break;

      case AsmInstr_CMP:
        remember_operand_type(&types, &instr->as.cmp.lhs);
        remember_operand_type(&types, &instr->as.cmp.rhs);
        break;

      case AsmInstr_CVT:
        remember_operand_type(&types, &instr->as.cvt.src);
        remember_operand_type(&types, &instr->as.cvt.dst);
        break;

      case AsmInstr_SetCC:
        remember_operand_type(&types, &instr->as.setcc.op);
        break;

      case AsmInstr_UNARY:
        remember_operand_type(&types, &instr->as.unary.op);
        break;

      case AsmInstr_PUSH:
        remember_operand_type(&types, &instr->as.push.op);
        break;

      case AsmInstr_POP:
        remember_operand_type(&types, &instr->as.pop.op);
        break;

      case AsmInstr_LEA:
        remember_operand_type(&types, &instr->as.lea.dst);
        break;

      case AsmInstr_CALL:
      case AsmInstr_RET:
      case AsmInstr_JMP:
      case AsmInstr_JmpCC:
      case AsmInstr_LBL:
      case AsmInstr_REP_MOVSB:
        break;

      default:
        assert(0 && "Unhandled instruction in collect_pseudo_types");
    }
  }

  return types;
}

static void free_pseudo_types(VecPseudoType *types)
{
  for (int i = 0; i < types->len; i++) {
    free(types->data[i].pseudo);
  }

  vec_free(types);

  types->capacity = 0;
  types->len = 0;
  types->data = NULL;
}

typedef Vector(struct LiveInterval) VecLiveInterval;
typedef Vector(char *) VecPseudo;

static int find_live_interval(VecLiveInterval *intervals, char *pseudo)
{
  for (int i = 0; i < intervals->len; i++) {
    if (strcmp(intervals->data[i].pseudo, pseudo) == 0) {
      return i;
    }
  }

  return -1;
}

static void touch_live_interval(VecLiveInterval *intervals, char *pseudo,
                                struct AsmType asm_type, int instr_idx)
{
  int idx = find_live_interval(intervals, pseudo);

  if (idx >= 0) {
    if (instr_idx < intervals->data[idx].start) {
      intervals->data[idx].start = instr_idx;
    }

    if (instr_idx > intervals->data[idx].end) {
      intervals->data[idx].end = instr_idx;
    }

    return;
  }

  struct LiveInterval interval;

  interval.pseudo = strdup(pseudo);
  if (!interval.pseudo) {
    perror("strdup");
    exit(1);
  }

  interval.start = instr_idx;
  interval.end = instr_idx;
  interval.asm_type = asm_type;

  vec_insert(intervals, interval);
}

static void touch_pseudo_set(VecLiveInterval *intervals, VecPseudoType *types,
                             VecPseudo *set, int instr_idx)
{
  for (int i = 0; i < set->len; i++) {
    struct AsmType *asm_type = pseudo_type_get(types, set->data[i]);

    assert(asm_type && "live pseudo has no known AsmType");

    touch_live_interval(intervals, set->data[i], *asm_type, instr_idx);
  }
}

struct InstrLiveness {
  VecPseudo use;
  VecPseudo def;
  VecPseudo live_before;
  VecPseudo live_after;
};

static VecLiveInterval build_live_intervals(struct InstrLiveness *lv, int n,
                                            VecPseudoType *types)
{
  VecLiveInterval intervals = {0};

  for (int i = 0; i < n; i++) {
    touch_pseudo_set(&intervals, types, &lv[i].use, i);
    touch_pseudo_set(&intervals, types, &lv[i].def, i);
    touch_pseudo_set(&intervals, types, &lv[i].live_before, i);
    touch_pseudo_set(&intervals, types, &lv[i].live_after, i);
  }

  return intervals;
}

static int compare_live_intervals(const void *a, const void *b)
{
  const struct LiveInterval *ia = a;
  const struct LiveInterval *ib = b;

  if (ia->start != ib->start) {
    return ia->start - ib->start;
  }

  return ia->end - ib->end;
}

static void sort_live_intervals(VecLiveInterval *intervals)
{
  qsort(intervals->data, intervals->len, sizeof(intervals->data[0]),
        compare_live_intervals);
}

static void print_live_intervals(VecLiveInterval *intervals)
{
  printf("Live intervals:\n");

  for (int i = 0; i < intervals->len; i++) {
    printf("  %-20s [%d, %d]\n", intervals->data[i].pseudo,
           intervals->data[i].start, intervals->data[i].end);
  }
}

static void free_live_intervals(VecLiveInterval *intervals)
{
  for (int i = 0; i < intervals->len; i++) {
    free(intervals->data[i].pseudo);
  }

  vec_free(intervals);

  intervals->capacity = 0;
  intervals->len = 0;
  intervals->data = NULL;
}

void init_regalloc_state(struct RegAllocState *state)
{
  memset(state, 0, sizeof(*state));
}

void free_regalloc_state(struct RegAllocState *state)
{
  struct AllocatedReg *curr = state->allocated_regs;

  while (curr) {
    struct AllocatedReg *next = curr->next;
    free(curr->pseudo);
    free(curr);
    curr = next;
  }

  state->allocated_regs = NULL;
}

static int allocatable_reg_index(enum AsmRegister reg)
{
  for (int i = 0; i < NUM_ALLOCATABLE_INT_REGS; i++) {
    if (allocatable_int_regs[i] == reg) {
      return i;
    }
  }

  return -1;
}

static void free_reg(struct RegAllocState *state, enum AsmRegister reg)
{
  int idx = allocatable_reg_index(reg);

  assert(idx >= 0 && "tried to free a non-allocatable register");

  state->reg_used[idx] = false;
}

typedef Vector(int) VecInt;

struct BasicBlock {
  int start; /* inclusive instruction index */
  int end;   /* inclusive instruction index */

  VecInt succs;

  VecPseudo use;
  VecPseudo def;

  VecPseudo live_in;
  VecPseudo live_out;
};

struct CFG {
  struct BasicBlock *blocks;
  int block_count;

  /*
   * instr_to_block[i] tells us which block owns instruction i.
   */
  int *instr_to_block;
};

static void int_set_add(VecInt *set, int value)
{
  for (int i = 0; i < set->len; i++) {
    if (set->data[i] == value) {
      return;
    }
  }

  vec_insert(set, value);
}

static int find_label_instr(struct AsmFunction *fn, char *label)
{
  for (int i = 0; i < fn->body.len; i++) {
    struct AsmInstr *instr = &fn->body.data[i];

    if (instr->kind == AsmInstr_LBL && strcmp(instr->as.lbl.name, label) == 0) {
      return i;
    }
  }

  return -1;
}

static int block_after(struct CFG *cfg, int block_idx)
{
  int next = block_idx + 1;

  if (next >= cfg->block_count) {
    return -1;
  }

  return next;
}

static bool pseudo_set_contains(VecPseudo *set, char *name)
{
  for (int i = 0; i < set->len; i++) {
    if (strcmp(set->data[i], name) == 0) {
      return true;
    }
  }

  return false;
}

static bool pseudo_set_equal(VecPseudo *a, VecPseudo *b)
{
  if (a->len != b->len) {
    return false;
  }

  for (int i = 0; i < a->len; i++) {
    if (!pseudo_set_contains(b, a->data[i])) {
      return false;
    }
  }

  return true;
}

static void pseudo_set_add(VecPseudo *set, char *name)
{
  if (!name) {
    return;
  }

  if (pseudo_set_contains(set, name)) {
    return;
  }

  vec_insert(set, name);
}

static void pseudo_set_copy(VecPseudo *dst, VecPseudo *src)
{
  dst->len = 0;

  for (int i = 0; i < src->len; i++) {
    pseudo_set_add(dst, src->data[i]);
  }
}

static void pseudo_set_union_into(VecPseudo *dst, VecPseudo *src)
{
  for (int i = 0; i < src->len; i++) {
    pseudo_set_add(dst, src->data[i]);
  }
}

static void pseudo_set_remove(VecPseudo *set, char *name)
{
  for (int i = 0; i < set->len; i++) {
    if (strcmp(set->data[i], name) == 0) {
      set->data[i] = set->data[set->len - 1];
      set->len--;
      return;
    }
  }
}

static void pseudo_set_subtract(VecPseudo *dst, VecPseudo *to_remove)
{
  for (int i = 0; i < to_remove->len; i++) {
    pseudo_set_remove(dst, to_remove->data[i]);
  }
}

static void pseudo_set_free(VecPseudo *set)
{
  vec_free(set);
  set->capacity = 0;
  set->len = 0;
  set->data = NULL;
}

static void pseudo_set_print(VecPseudo *set)
{
  printf("{");

  for (int i = 0; i < set->len; i++) {
    if (i > 0) {
      printf(", ");
    }

    printf("%s", set->data[i]);
  }

  printf("}");
}
static int block_for_label(struct AsmFunction *fn, struct CFG *cfg, char *label)
{
  int instr_idx = find_label_instr(fn, label);

  assert(instr_idx >= 0 && "jump target label not found");

  return cfg->instr_to_block[instr_idx];
}

static void dot_escape(FILE *out, const char *s)
{
  for (; *s; s++) {
    switch (*s) {
      case '\\':
        fputs("\\\\", out);
        break;
      case '"':
        fputs("\\\"", out);
        break;
      case '\n':
        fputs("\\n", out);
        break;
      default:
        fputc(*s, out);
        break;
    }
  }
}

static const char *asm_instr_kind_name(enum AsmInstrKind kind)
{
  switch (kind) {
    case AsmInstr_PUSH:
      return "PUSH";
    case AsmInstr_POP:
      return "POP";
    case AsmInstr_MOV:
      return "MOV";
    case AsmInstr_BIN:
      return "BIN";
    case AsmInstr_RET:
      return "RET";
    case AsmInstr_CALL:
      return "CALL";
    case AsmInstr_JMP:
      return "JMP";
    case AsmInstr_LBL:
      return "LBL";
    case AsmInstr_CMP:
      return "CMP";
    case AsmInstr_JmpCC:
      return "JmpCC";
    case AsmInstr_SetCC:
      return "SetCC";
    case AsmInstr_LEA:
      return "LEA";
    case AsmInstr_UNARY:
      return "UNARY";
    case AsmInstr_CVT:
      return "CVT";
    case AsmInstr_REP_MOVSB:
      return "REP_MOVSB";
  }

  assert(0 && "unhandled AsmInstrKind");
}

static void dot_print_pseudo_set(FILE *out, VecPseudo *set)
{
  fputc('{', out);

  for (int i = 0; i < set->len; i++) {
    if (i > 0) {
      fputs(", ", out);
    }

    dot_escape(out, set->data[i]);
  }

  fputc('}', out);
}

static struct CFG build_cfg(struct AsmFunction *fn)
{
  int n = fn->body.len;
  bool *leader = calloc(n, sizeof(*leader));

  if (!leader) {
    perror("calloc");
    exit(1);
  }

  if (n > 0) {
    leader[0] = true;
  }

  /*
   * First pass: mark obvious leaders.
   */
  for (int i = 0; i < n; i++) {
    struct AsmInstr *instr = &fn->body.data[i];

    if (instr->kind == AsmInstr_LBL) {
      leader[i] = true;
    }

    if (instr->kind == AsmInstr_JMP || instr->kind == AsmInstr_JmpCC ||
        instr->kind == AsmInstr_RET) {
      if (i + 1 < n) {
        leader[i + 1] = true;
      }
    }
  }

  /*
   * Second pass: jump targets are leaders too.
   */
  for (int i = 0; i < n; i++) {
    struct AsmInstr *instr = &fn->body.data[i];

    if (instr->kind == AsmInstr_JMP) {
      int target_idx = find_label_instr(fn, instr->as.jmp.target);
      assert(target_idx >= 0 && "JMP target not found");
      leader[target_idx] = true;
    } else if (instr->kind == AsmInstr_JmpCC) {
      int target_idx = find_label_instr(fn, instr->as.jmpcc.target);
      assert(target_idx >= 0 && "JmpCC target not found");
      leader[target_idx] = true;
    }
  }

  int block_count = 0;
  for (int i = 0; i < n; i++) {
    if (leader[i]) {
      block_count++;
    }
  }

  struct CFG cfg;
  cfg.blocks = calloc(block_count, sizeof(*cfg.blocks));
  cfg.instr_to_block = calloc(n, sizeof(*cfg.instr_to_block));
  cfg.block_count = block_count;

  if (!cfg.blocks || !cfg.instr_to_block) {
    perror("calloc");
    exit(1);
  }

  /*
   * Create blocks.
   */
  int b = 0;
  for (int i = 0; i < n; i++) {
    if (!leader[i]) {
      continue;
    }

    int start = i;
    int end = i;

    while (end + 1 < n && !leader[end + 1]) {
      end++;
    }

    cfg.blocks[b].start = start;
    cfg.blocks[b].end = end;

    for (int j = start; j <= end; j++) {
      cfg.instr_to_block[j] = b;
    }

    b++;
  }

  /*
   * Add successor edges.
   */
  for (int bi = 0; bi < cfg.block_count; bi++) {
    struct BasicBlock *block = &cfg.blocks[bi];
    struct AsmInstr *last = &fn->body.data[block->end];

    if (last->kind == AsmInstr_JMP) {
      int_set_add(&block->succs,
                  block_for_label(fn, &cfg, last->as.jmp.target));
    } else if (last->kind == AsmInstr_JmpCC) {
      int target = block_for_label(fn, &cfg, last->as.jmpcc.target);
      int fallthrough = block_after(&cfg, bi);

      int_set_add(&block->succs, target);

      if (fallthrough >= 0) {
        int_set_add(&block->succs, fallthrough);
      }
    } else if (last->kind == AsmInstr_RET) {
      /*
       * No successors.
       */
    } else {
      int fallthrough = block_after(&cfg, bi);

      if (fallthrough >= 0) {
        int_set_add(&block->succs, fallthrough);
      }
    }
  }

  free(leader);

  return cfg;
}

static void compute_block_use_def(struct AsmFunction *fn,
                                  struct BasicBlock *block,
                                  struct InstrLiveness *lv)
{
  for (int i = block->start; i <= block->end; i++) {
    /*
     * use[B] contains values used before being defined inside B.
     */
    for (int j = 0; j < lv[i].use.len; j++) {
      char *name = lv[i].use.data[j];

      if (!pseudo_set_contains(&block->def, name)) {
        pseudo_set_add(&block->use, name);
      }
    }

    /*
     * def[B] contains values defined inside B.
     */
    for (int j = 0; j < lv[i].def.len; j++) {
      pseudo_set_add(&block->def, lv[i].def.data[j]);
    }
  }
}

static void solve_block_liveness(struct CFG *cfg)
{
  bool changed = true;

  while (changed) {
    changed = false;

    /*
     * Backward order usually converges faster.
     */
    for (int bi = cfg->block_count - 1; bi >= 0; bi--) {
      struct BasicBlock *block = &cfg->blocks[bi];

      VecPseudo new_live_out = {0};
      VecPseudo new_live_in = {0};

      /*
       * live_out[B] = union(live_in[S]) for each successor S.
       */
      for (int i = 0; i < block->succs.len; i++) {
        int succ_idx = block->succs.data[i];
        pseudo_set_union_into(&new_live_out, &cfg->blocks[succ_idx].live_in);
      }

      /*
       * live_in[B] = use[B] union (live_out[B] - def[B])
       */
      pseudo_set_copy(&new_live_in, &new_live_out);
      pseudo_set_subtract(&new_live_in, &block->def);
      pseudo_set_union_into(&new_live_in, &block->use);

      if (!pseudo_set_equal(&block->live_out, &new_live_out) ||
          !pseudo_set_equal(&block->live_in, &new_live_in)) {
        changed = true;
      }

      pseudo_set_free(&block->live_out);
      pseudo_set_free(&block->live_in);

      block->live_out = new_live_out;
      block->live_in = new_live_in;
    }
  }
}

static void dot_print_instr_summary(FILE *out, int idx, struct AsmInstr *instr)
{
  fprintf(out, "%04d: %s", idx, asm_instr_kind_name(instr->kind));

  switch (instr->kind) {
    case AsmInstr_LBL:
      fputs(" ", out);
      dot_escape(out, instr->as.lbl.name);
      break;

    case AsmInstr_JMP:
      fputs(" -> ", out);
      dot_escape(out, instr->as.jmp.target);
      break;

    case AsmInstr_JmpCC:
      fputs(" -> ", out);
      dot_escape(out, instr->as.jmpcc.target);
      break;

    case AsmInstr_CALL:
      fputs(" ", out);
      dot_escape(out, instr->as.call.target);
      break;

    default:
      break;
  }
}

static void dump_cfg_dot(FILE *out, struct AsmFunction *fn, struct CFG *cfg,
                         struct InstrLiveness *lv)
{
  fprintf(out, "digraph \"cfg_%s\" {\n", fn->name);
  fprintf(out, "  graph [fontname=\"monospace\"];\n");
  fprintf(out, "  node [shape=box, fontname=\"monospace\"];\n");
  fprintf(out, "  edge [fontname=\"monospace\"];\n\n");

  for (int bi = 0; bi < cfg->block_count; bi++) {
    struct BasicBlock *block = &cfg->blocks[bi];

    fprintf(out, "  B%d [label=\"", bi);

    fprintf(out, "B%d [%d..%d]\\l", bi, block->start, block->end);

    fputs("use = ", out);
    dot_print_pseudo_set(out, &block->use);
    fputs("\\l", out);

    fputs("def = ", out);
    dot_print_pseudo_set(out, &block->def);
    fputs("\\l", out);

    fputs("live_in = ", out);
    dot_print_pseudo_set(out, &block->live_in);
    fputs("\\l", out);

    fputs("live_out = ", out);
    dot_print_pseudo_set(out, &block->live_out);
    fputs("\\l\\l", out);

    for (int i = block->start; i <= block->end; i++) {
      dot_print_instr_summary(out, i, &fn->body.data[i]);

      if (lv) {
        fputs("\\l    before=", out);
        dot_print_pseudo_set(out, &lv[i].live_before);
        fputs("\\l    after =", out);
        dot_print_pseudo_set(out, &lv[i].live_after);
      }

      fputs("\\l", out);
    }

    fprintf(out, "\"];\n");
  }

  fprintf(out, "\n");

  for (int bi = 0; bi < cfg->block_count; bi++) {
    struct BasicBlock *block = &cfg->blocks[bi];

    for (int i = 0; i < block->succs.len; i++) {
      int succ = block->succs.data[i];

      fprintf(out, "  B%d -> B%d", bi, succ);

      struct AsmInstr *last = &fn->body.data[block->end];

      if (last->kind == AsmInstr_JMP) {
        fprintf(out, " [label=\"jmp\"]");
      } else if (last->kind == AsmInstr_JmpCC) {
        if (succ == block_for_label(fn, cfg, last->as.jmpcc.target)) {
          fprintf(out, " [label=\"true\"]");
        } else {
          fprintf(out, " [label=\"false\"]");
        }
      } else {
        fprintf(out, " [label=\"fallthrough\"]");
      }

      fprintf(out, ";\n");
    }
  }

  fprintf(out, "}\n");
}

static void free_cfg(struct CFG *cfg)
{
  for (int i = 0; i < cfg->block_count; i++) {
    vec_free(&cfg->blocks[i].succs);

    pseudo_set_free(&cfg->blocks[i].use);
    pseudo_set_free(&cfg->blocks[i].def);
    pseudo_set_free(&cfg->blocks[i].live_in);
    pseudo_set_free(&cfg->blocks[i].live_out);
  }

  free(cfg->blocks);
  free(cfg->instr_to_block);

  cfg->blocks = NULL;
  cfg->instr_to_block = NULL;
  cfg->block_count = 0;
}

static void write_cfg_dot(struct AsmFunction *fn, struct InstrLiveness *lv,
                          const char *path)
{
  FILE *out;
  struct CFG cfg;

  cfg = build_cfg(fn);

  for (int bi = 0; bi < cfg.block_count; bi++) {
    compute_block_use_def(fn, &cfg.blocks[bi], lv);
  }

  solve_block_liveness(&cfg);

  out = fopen(path, "w");
  if (!out) {
    perror("fopen");
    free_cfg(&cfg);
    return;
  }

  dump_cfg_dot(out, fn, &cfg, lv);

  fclose(out);
  free_cfg(&cfg);
}

static void release_dead_regs(struct RegAllocState *state,
                              VecPseudo *live_after)
{
  struct AllocatedReg **link = &state->allocated_regs;

  while (*link) {
    struct AllocatedReg *node = *link;

    if (is_tmp(node->pseudo) &&
        !pseudo_set_contains(live_after, node->pseudo)) {
      free_reg(state, node->reg);

      *link = node->next;

      free(node->pseudo);
      free(node);

      continue;
    }

    link = &node->next;
  }
}

bool alloc_reg(struct RegAllocState *state, enum AsmRegister *out_reg)
{
  for (int i = 0; i < NUM_ALLOCATABLE_INT_REGS; i++) {
    if (!state->reg_used[i]) {
      state->reg_used[i] = true;
      *out_reg = allocatable_int_regs[i];
      return true;
    }
  }

  return false;
}

void allocated_reg_append(struct RegAllocState *state, char *pseudo,
                          enum AsmRegister reg)
{
  struct AllocatedReg *node = malloc(sizeof(*node));
  if (!node) {
    perror("malloc");
    exit(1);
  }

  node->next = NULL;
  node->pseudo = strdup(pseudo);
  if (!node->pseudo) {
    perror("strdup");
    exit(1);
  }

  node->reg = reg;

  if (state->allocated_regs == NULL) {
    state->allocated_regs = node;
    return;
  }

  struct AllocatedReg *curr = state->allocated_regs;
  while (curr->next != NULL) {
    curr = curr->next;
  }

  curr->next = node;
}

bool allocated_reg_get(struct RegAllocState *state, char *pseudo,
                       enum AsmRegister *out_reg)
{
  for (struct AllocatedReg *curr = state->allocated_regs; curr;
       curr = curr->next) {
    if (strcmp(curr->pseudo, pseudo) == 0) {
      *out_reg = curr->reg;
      return true;
    }
  }

  return false;
}

static void add_use_operand(VecPseudo *use, struct AsmOperand *op)
{
  if (op->kind == AsmOperand_PSEUDO) {
    pseudo_set_add(use, op->as.pseudo);
  }
}

static void add_def_operand(VecPseudo *def, struct AsmOperand *op)
{
  if (op->kind == AsmOperand_PSEUDO) {
    pseudo_set_add(def, op->as.pseudo);
  }
}

static void compute_instr_use_def(struct AsmInstr *instr, VecPseudo *use,
                                  VecPseudo *def)
{
  switch (instr->kind) {
    case AsmInstr_MOV: {
      add_use_operand(use, &instr->as.mov.src);
      add_def_operand(def, &instr->as.mov.dst);
      break;
    }

    case AsmInstr_BIN: {
      /*
       * x86-style two-address instruction:
       *
       *   rhs = rhs OP lhs
       *
       * So rhs is both read and written.
       */
      add_use_operand(use, &instr->as.binary.lhs);
      add_use_operand(use, &instr->as.binary.rhs);
      add_def_operand(def, &instr->as.binary.rhs);
      break;
    }

    case AsmInstr_CMP: {
      add_use_operand(use, &instr->as.cmp.lhs);
      add_use_operand(use, &instr->as.cmp.rhs);
      break;
    }

    case AsmInstr_CVT: {
      add_use_operand(use, &instr->as.cvt.src);
      add_def_operand(def, &instr->as.cvt.dst);
      break;
    }

    case AsmInstr_SetCC: {
      add_def_operand(def, &instr->as.setcc.op);
      break;
    }

    case AsmInstr_UNARY: {
      /*
       * neg x / not x:
       *
       *   x = OP x
       */
      add_use_operand(use, &instr->as.unary.op);
      add_def_operand(def, &instr->as.unary.op);
      break;
    }

    case AsmInstr_PUSH: {
      add_use_operand(use, &instr->as.push.op);
      break;
    }

    case AsmInstr_POP: {
      add_def_operand(def, &instr->as.pop.op);
      break;
    }

    case AsmInstr_LEA: {
      /*
       * Important:
       *
       * LEA computes an address. The source is an address expression,
       * not a value read.
       *
       * So for now, only the destination is a def.
       */
      add_def_operand(def, &instr->as.lea.dst);
      break;
    }

    case AsmInstr_CALL:
    case AsmInstr_RET:
    case AsmInstr_JMP:
    case AsmInstr_JmpCC:
    case AsmInstr_LBL:
    case AsmInstr_REP_MOVSB: {
      break;
    }

    default:
      assert(0 && "Unhandled instruction in compute_instr_use_def");
  }
}

static void compute_instr_liveness_from_blocks(struct AsmFunction *fn,
                                               struct CFG *cfg,
                                               struct InstrLiveness *lv)
{
  for (int bi = 0; bi < cfg->block_count; bi++) {
    struct BasicBlock *block = &cfg->blocks[bi];

    VecPseudo live = {0};
    pseudo_set_copy(&live, &block->live_out);

    for (int i = block->end; i >= block->start; i--) {
      pseudo_set_copy(&lv[i].live_after, &live);

      VecPseudo before = {0};

      pseudo_set_copy(&before, &live);
      pseudo_set_subtract(&before, &lv[i].def);
      pseudo_set_union_into(&before, &lv[i].use);

      pseudo_set_copy(&lv[i].live_before, &before);

      pseudo_set_free(&live);
      live = before;
    }

    pseudo_set_free(&live);
  }
}

static struct InstrLiveness *compute_liveness_cfg(struct AsmFunction *fn)
{
  int n = fn->body.len;

  struct InstrLiveness *lv = calloc(n, sizeof(*lv));
  if (!lv) {
    perror("calloc");
    exit(1);
  }

  /*
   * Per-instruction use/def.
   */
  for (int i = 0; i < n; i++) {
    compute_instr_use_def(&fn->body.data[i], &lv[i].use, &lv[i].def);
  }

  struct CFG cfg = build_cfg(fn);

  /*
   * Per-block use/def.
   */
  for (int bi = 0; bi < cfg.block_count; bi++) {
    compute_block_use_def(fn, &cfg.blocks[bi], lv);
  }

  /*
   * Fixed-point block liveness.
   */
  solve_block_liveness(&cfg);

  /*
   * Convert back to per-instruction liveness.
   */
  compute_instr_liveness_from_blocks(fn, &cfg, lv);

  free_cfg(&cfg);

  return lv;
}

static void free_liveness(struct InstrLiveness *lv, int n)
{
  for (int i = 0; i < n; i++) {
    pseudo_set_free(&lv[i].use);
    pseudo_set_free(&lv[i].def);
    pseudo_set_free(&lv[i].live_before);
    pseudo_set_free(&lv[i].live_after);
  }

  free(lv);
}

static void print_liveness(struct AsmFunction *fn, struct InstrLiveness *lv)
{
  printf("Liveness for function %s:\n", fn->name);

  for (int i = 0; i < fn->body.len; i++) {
    printf("%04d: use=", i);
    pseudo_set_print(&lv[i].use);

    printf(" def=");
    pseudo_set_print(&lv[i].def);

    printf(" live_before=");
    pseudo_set_print(&lv[i].live_before);

    printf(" live_after=");
    pseudo_set_print(&lv[i].live_after);

    printf("\n");
  }
}

enum PseudoHomeKind {
  PSEUDO_HOME_REG,
  PSEUDO_HOME_STACK,
};

struct PseudoHome {
  char *pseudo;
  enum PseudoHomeKind kind;
  union {
    enum AsmRegister reg;
    int stack_offset;
  } as;
};

typedef Vector(struct PseudoHome) VecPseudoHome;

static struct PseudoHome *pseudo_home_get(VecPseudoHome *homes, char *pseudo)
{
  for (int i = 0; i < homes->len; i++) {
    if (strcmp(homes->data[i].pseudo, pseudo) == 0) {
      return &homes->data[i];
    }
  }

  return NULL;
}

static void pseudo_home_add_reg(VecPseudoHome *homes, char *pseudo,
                                enum AsmRegister reg)
{
  struct PseudoHome home;

  assert(pseudo_home_get(homes, pseudo) == NULL);

  home.pseudo = strdup(pseudo);
  if (!home.pseudo) {
    perror("strdup");
    exit(1);
  }

  home.kind = PSEUDO_HOME_REG;
  home.as.reg = reg;

  vec_insert(homes, home);
}

static void pseudo_home_add_stack(VecPseudoHome *homes, char *pseudo,
                                  int stack_offset)
{
  struct PseudoHome home;

  assert(pseudo_home_get(homes, pseudo) == NULL);

  home.pseudo = strdup(pseudo);
  if (!home.pseudo) {
    perror("strdup");
    exit(1);
  }

  home.kind = PSEUDO_HOME_STACK;
  home.as.stack_offset = stack_offset;

  vec_insert(homes, home);
}

static void free_pseudo_homes(VecPseudoHome *homes)
{
  for (int i = 0; i < homes->len; i++) {
    free(homes->data[i].pseudo);
  }

  vec_free(homes);

  homes->capacity = 0;
  homes->len = 0;
  homes->data = NULL;
}

struct ActiveInterval {
  char *pseudo;
  int end;
  enum AsmRegister reg;
};

typedef Vector(struct ActiveInterval) VecActiveInterval;

static int compare_active_intervals_by_end(const void *a, const void *b)
{
  const struct ActiveInterval *ia = a;
  const struct ActiveInterval *ib = b;

  return ia->end - ib->end;
}

static void sort_active_intervals(VecActiveInterval *active)
{
  qsort(active->data, active->len, sizeof(active->data[0]),
        compare_active_intervals_by_end);
}

static void active_remove_at(VecActiveInterval *active, int idx)
{
  assert(idx >= 0 && idx < active->len);

  for (int i = idx; i + 1 < active->len; i++) {
    active->data[i] = active->data[i + 1];
  }

  active->len--;
}

static bool fixed_reg_is_used(VecActiveInterval *active, enum AsmRegister reg)
{
  for (int i = 0; i < active->len; i++) {
    if (active->data[i].reg == reg) {
      return true;
    }
  }

  return false;
}

static bool fixed_find_free_reg(VecActiveInterval *active,
                                enum AsmRegister *out_reg)
{
  for (int i = 0; i < NUM_ALLOCATABLE_INT_REGS; i++) {
    enum AsmRegister reg = allocatable_int_regs[i];

    if (!fixed_reg_is_used(active, reg)) {
      *out_reg = reg;
      return true;
    }
  }

  return false;
}

static void expire_old_intervals(VecActiveInterval *active, int start)
{
  int i = 0;

  while (i < active->len) {
    if (active->data[i].end < start) {
      active_remove_at(active, i);
      continue;
    }

    i++;
  }
}

static VecPseudoHome allocate_pseudo_homes(VecLiveInterval *intervals,
                                           struct Map *map,
                                           int *used_stack_bytes,
                                           struct AsmFunction *fn)
{
  VecPseudoHome homes = {0};
  VecActiveInterval active = {0};

  /*
   * Intervals must already be sorted by start.
   */
  for (int i = 0; i < intervals->len; i++) {
    struct LiveInterval *interval = &intervals->data[i];
    enum AsmRegister reg;

    expire_old_intervals(&active, interval->start);
    sort_active_intervals(&active);

    /*
     * Later we can look up the pseudo's real AsmType here.
     * For now, we assume your interval list only contains pseudos
     * that came from AsmOperands and are legal allocation candidates.
     */

    if (fixed_find_free_reg(&active, &reg)) {
      struct ActiveInterval active_interval;

      pseudo_home_add_reg(&homes, interval->pseudo, reg);

      active_interval.pseudo = interval->pseudo;
      active_interval.end = interval->end;
      active_interval.reg = reg;

      vec_insert(&active, active_interval);
      continue;
    }

    /*
     * No register available.
     *
     * For the first fixed-home version, spill the new interval.
     * This is simple and safe, though not optimal.
     */
    {
      int stack_offset;

      stack_offset = get_offset(map, interval->pseudo, interval->asm_type,
                                used_stack_bytes);

      pseudo_home_add_stack(&homes, interval->pseudo, stack_offset);
    }
  }

  vec_free(&active);

  return homes;
}

struct InterferenceNode {
  /*
   * For pseudo nodes:      name = "var.x.0"
   * For precolored nodes: name = "%eax", "%edx", etc.
   */
  char *pseudo;

  bool is_precolored;
  enum AsmRegister precolored_reg;

  VecInt neighbors;
};

typedef Vector(struct InterferenceNode) VecInterferenceNode;

struct InterferenceGraph {
  VecInterferenceNode nodes;
};

static int interference_node_index(struct InterferenceGraph *graph,
                                   char *pseudo)
{
  for (int i = 0; i < graph->nodes.len; i++) {
    if (strcmp(graph->nodes.data[i].pseudo, pseudo) == 0) {
      return i;
    }
  }

  return -1;
}

static int interference_add_node(struct InterferenceGraph *graph, char *pseudo)
{
  int idx = interference_node_index(graph, pseudo);

  if (idx >= 0) {
    return idx;
  }

  struct InterferenceNode node;

  node.pseudo = strdup(pseudo);
  if (!node.pseudo) {
    perror("strdup");
    exit(1);
  }

  node.is_precolored = false;
  node.precolored_reg = AX; /* unused for non-precolored nodes */
  node.neighbors = (VecInt){0};

  vec_insert(&graph->nodes, node);

  return graph->nodes.len - 1;
}

static bool int_vec_contains(VecInt *vec, int value)
{
  for (int i = 0; i < vec->len; i++) {
    if (vec->data[i] == value) {
      return true;
    }
  }

  return false;
}

static char *precolored_reg_name(enum AsmRegister reg)
{
  switch (reg) {
    case AX:
      return "ax";
    case BX:
      return "bx";
    case DX:
      return "dx";
    case CX:
      return "cx";
    case SI:
      return "si";
    case DI:
      return "di";
    case R8:
      return "r8";
    case R9:
      return "r9";
    case R12:
      return "r12";
    case R13:
      return "r13";
    case R14:
      return "r14";
    case R15:
      return "r15";
    case BP:
      return "bp";
    case SP:
      return "sp";
    default:
      assert(0 && "unhandled precolored register");
  }
}

static int interference_add_precolored_reg(struct InterferenceGraph *graph,
                                           enum AsmRegister reg)
{
  char *name = precolored_reg_name(reg);
  int idx = interference_node_index(graph, name);

  if (idx >= 0) {
    graph->nodes.data[idx].is_precolored = true;
    graph->nodes.data[idx].precolored_reg = reg;
    return idx;
  }

  struct InterferenceNode node;

  node.pseudo = strdup(name);
  if (!node.pseudo) {
    perror("strdup");
    exit(1);
  }

  node.is_precolored = true;
  node.precolored_reg = reg;
  node.neighbors = (VecInt){0};

  vec_insert(&graph->nodes, node);

  return graph->nodes.len - 1;
}

static void interference_add_edge_by_index(struct InterferenceGraph *graph,
                                           int ai, int bi)
{
  if (ai == bi) {
    return;
  }

  if (!int_vec_contains(&graph->nodes.data[ai].neighbors, bi)) {
    vec_insert(&graph->nodes.data[ai].neighbors, bi);
  }

  if (!int_vec_contains(&graph->nodes.data[bi].neighbors, ai)) {
    vec_insert(&graph->nodes.data[bi].neighbors, ai);
  }
}

static void interference_add_edge(struct InterferenceGraph *graph, char *a,
                                  char *b)
{
  int ai;
  int bi;

  if (strcmp(a, b) == 0) {
    return;
  }

  ai = interference_add_node(graph, a);
  bi = interference_add_node(graph, b);

  interference_add_edge_by_index(graph, ai, bi);
}

static enum AsmRegister caller_saved_regs[] = {
    AX, DX, CX, SI, DI, R8, R9,
};

#define NUM_CALLER_SAVED_REGS \
  ((int) (sizeof(caller_saved_regs) / sizeof(caller_saved_regs[0])))

static enum AsmRegister callee_saved_regs[] = {
    BX, R12, R13, R14, R15,
};

#define NUM_CALLEE_SAVED_REGS \
  ((int) (sizeof(callee_saved_regs) / sizeof(callee_saved_regs[0])))

static void add_precolored_register_nodes(struct InterferenceGraph *graph)
{
  for (int i = 0; i < NUM_ALLOCATABLE_INT_REGS; i++) {
    interference_add_precolored_reg(graph, allocatable_int_regs[i]);
  }
}

static void add_call_clobber_interference(struct InterferenceGraph *graph,
                                          struct InstrLiveness *lv)
{
  for (int i = 0; i < lv->live_after.len; i++) {
    char *pseudo = lv->live_after.data[i];
    int pseudo_idx = interference_add_node(graph, pseudo);

    for (int r = 0; r < NUM_CALLER_SAVED_REGS; r++) {
      int reg_idx =
          interference_add_precolored_reg(graph, caller_saved_regs[r]);

      interference_add_edge_by_index(graph, pseudo_idx, reg_idx);
    }
  }
}

static void interference_add_nodes_from_set(struct InterferenceGraph *graph,
                                            VecPseudo *set)
{
  for (int i = 0; i < set->len; i++) {
    interference_add_node(graph, set->data[i]);
  }
}

static char *mov_pseudo_src(struct AsmInstr *instr)
{
  if (instr->kind != AsmInstr_MOV) {
    return NULL;
  }

  if (instr->as.mov.src.kind != AsmOperand_PSEUDO) {
    return NULL;
  }

  if (instr->as.mov.dst.kind != AsmOperand_PSEUDO) {
    return NULL;
  }

  return instr->as.mov.src.as.pseudo;
}

static void add_interferences_for_instr(struct InterferenceGraph *graph,
                                        struct AsmInstr *instr,
                                        struct InstrLiveness *lv)
{
  VecPseudo live = {0};
  char *move_src;

  /*
   * Make sure all mentioned pseudos become graph nodes, even if they
   * end up having no interference edges.
   */
  interference_add_nodes_from_set(graph, &lv->use);
  interference_add_nodes_from_set(graph, &lv->def);
  interference_add_nodes_from_set(graph, &lv->live_before);
  interference_add_nodes_from_set(graph, &lv->live_after);

  pseudo_set_copy(&live, &lv->live_after);

  /*
   * Move special case:
   *
   *   mov src, dst
   *
   * Do not add dst -- src here, because later coalescing may want to
   * merge src and dst and delete the move.
   */
  move_src = mov_pseudo_src(instr);
  if (move_src) {
    pseudo_set_remove(&live, move_src);
  }

  /*
   * Every def interferes with everything live after the instruction.
   */
  for (int i = 0; i < lv->def.len; i++) {
    char *defined = lv->def.data[i];

    for (int j = 0; j < live.len; j++) {
      char *live_pseudo = live.data[j];

      interference_add_edge(graph, defined, live_pseudo);
    }
  }

  pseudo_set_free(&live);
}

static struct InterferenceGraph build_interference_graph(
    struct AsmFunction *fn, struct InstrLiveness *lv)
{
  struct InterferenceGraph graph = {0};

  add_precolored_register_nodes(&graph);

  for (int i = 0; i < fn->body.len; i++) {
    add_interferences_for_instr(&graph, &fn->body.data[i], &lv[i]);

    if (fn->body.data[i].kind == AsmInstr_CALL) {
      add_call_clobber_interference(&graph, &lv[i]);
    }
  }

  return graph;
}

static void print_interference_graph(struct InterferenceGraph *graph)
{
  printf("Interference graph:\n");

  for (int i = 0; i < graph->nodes.len; i++) {
    struct InterferenceNode *node = &graph->nodes.data[i];

    if (node->is_precolored) {
      printf("  %s [precolored]:", node->pseudo);
    } else {
      printf("  %s:", node->pseudo);
    }

    for (int j = 0; j < node->neighbors.len; j++) {
      int neighbor_idx = node->neighbors.data[j];
      printf(" %s", graph->nodes.data[neighbor_idx].pseudo);
    }

    printf("\n");
  }
}

static void dump_interference_graph_dot(FILE *out,
                                        struct InterferenceGraph *graph)
{
  fprintf(out, "graph interference {\n");
  fprintf(out, "  graph [fontname=\"monospace\"];\n");
  fprintf(out, "  node [shape=ellipse, fontname=\"monospace\"];\n");
  fprintf(out, "  edge [fontname=\"monospace\"];\n\n");

  /*
   * Print nodes first, including isolated nodes.
   */
  for (int i = 0; i < graph->nodes.len; i++) {
    fprintf(out, "  \"");
    dot_escape(out, graph->nodes.data[i].pseudo);
    fprintf(out, "\";\n");
  }

  fprintf(out, "\n");

  /*
   * Print each undirected edge once.
   */
  for (int i = 0; i < graph->nodes.len; i++) {
    struct InterferenceNode *node = &graph->nodes.data[i];

    for (int j = 0; j < node->neighbors.len; j++) {
      int neighbor_idx = node->neighbors.data[j];

      if (i > neighbor_idx) {
        continue;
      }

      fprintf(out, "  \"");
      dot_escape(out, node->pseudo);
      fprintf(out, "\" -- \"");
      dot_escape(out, graph->nodes.data[neighbor_idx].pseudo);
      fprintf(out, "\";\n");
    }
  }

  fprintf(out, "}\n");
}

static void write_interference_graph_dot(struct InterferenceGraph *graph,
                                         const char *path)
{
  FILE *out = fopen(path, "w");

  if (!out) {
    perror("fopen");
    return;
  }

  dump_interference_graph_dot(out, graph);

  fclose(out);
}

static void free_interference_graph(struct InterferenceGraph *graph)
{
  for (int i = 0; i < graph->nodes.len; i++) {
    free(graph->nodes.data[i].pseudo);
    vec_free(&graph->nodes.data[i].neighbors);
  }

  vec_free(&graph->nodes);

  graph->nodes.capacity = 0;
  graph->nodes.len = 0;
  graph->nodes.data = NULL;
}

static void regalloc_op_from_homes(struct AsmOperand *op, VecPseudoHome *homes,
                                   struct Map *map, int *used_stack_bytes)
{
  struct PseudoHome *home;

  if (op->kind != AsmOperand_PSEUDO) {
    return;
  }

  home = pseudo_home_get(homes, op->as.pseudo);

  /*
   * Conservative fallback: pseudo did not appear in intervals.
   * Put it on the stack.
   */
  if (!home) {
    op->kind = AsmOperand_STACK;
    op->as.stack_offset =
        get_offset(map, op->as.pseudo, op->asm_type, used_stack_bytes);
    return;
  }

  if (home->kind == PSEUDO_HOME_REG) {
    op->kind = AsmOperand_REG;
    op->as.reg = home->as.reg;
    return;
  }

  op->kind = AsmOperand_STACK;
  op->as.stack_offset = home->as.stack_offset;
}

static struct AsmInstr *regalloc_instr_from_homes(struct AsmInstr *instr,
                                                  VecPseudoHome *homes,
                                                  struct Map *map,
                                                  int *used_stack_bytes)
{
  switch (instr->kind) {
    case AsmInstr_MOV: {
      regalloc_op_from_homes(&instr->as.mov.src, homes, map, used_stack_bytes);
      regalloc_op_from_homes(&instr->as.mov.dst, homes, map, used_stack_bytes);
      break;
    }

    case AsmInstr_BIN: {
      regalloc_op_from_homes(&instr->as.binary.lhs, homes, map,
                             used_stack_bytes);
      regalloc_op_from_homes(&instr->as.binary.rhs, homes, map,
                             used_stack_bytes);
      break;
    }

    case AsmInstr_CMP: {
      regalloc_op_from_homes(&instr->as.cmp.lhs, homes, map, used_stack_bytes);
      regalloc_op_from_homes(&instr->as.cmp.rhs, homes, map, used_stack_bytes);
      break;
    }

    case AsmInstr_CVT: {
      regalloc_op_from_homes(&instr->as.cvt.src, homes, map, used_stack_bytes);
      regalloc_op_from_homes(&instr->as.cvt.dst, homes, map, used_stack_bytes);
      break;
    }

    case AsmInstr_SetCC: {
      regalloc_op_from_homes(&instr->as.setcc.op, homes, map, used_stack_bytes);
      break;
    }

    case AsmInstr_UNARY: {
      regalloc_op_from_homes(&instr->as.unary.op, homes, map, used_stack_bytes);
      break;
    }

    case AsmInstr_PUSH: {
      regalloc_op_from_homes(&instr->as.push.op, homes, map, used_stack_bytes);
      break;
    }

    case AsmInstr_POP: {
      regalloc_op_from_homes(&instr->as.pop.op, homes, map, used_stack_bytes);
      break;
    }

    case AsmInstr_LEA: {
      regalloc_op_from_homes(&instr->as.lea.dst, homes, map, used_stack_bytes);
      break;
    }

    case AsmInstr_CALL:
    case AsmInstr_RET:
    case AsmInstr_JMP:
    case AsmInstr_JmpCC:
    case AsmInstr_LBL:
    case AsmInstr_REP_MOVSB: {
      break;
    }

    default:
      assert(0 && "Unhandled instruction in regalloc_instr_from_homes");
  }

  return instr;
}

struct SelectStackEntry {
  int node_idx;
  bool was_spill_candidate;
};

typedef Vector(struct SelectStackEntry) VecSelectStack;

static int graph_degree_after_removal(struct InterferenceGraph *graph,
                                      bool *removed, int node_idx)
{
  int degree = 0;
  struct InterferenceNode *node = &graph->nodes.data[node_idx];

  for (int i = 0; i < node->neighbors.len; i++) {
    int neighbor_idx = node->neighbors.data[i];

    if (!removed[neighbor_idx]) {
      degree++;
    }
  }

  return degree;
}

static int pick_low_degree_node(struct InterferenceGraph *graph, bool *removed,
                                int k)
{
  for (int i = 0; i < graph->nodes.len; i++) {
    if (removed[i]) {
      continue;
    }

    if (graph_degree_after_removal(graph, removed, i) < k) {
      return i;
    }
  }

  return -1;
}

static int pick_spill_candidate(struct InterferenceGraph *graph, bool *removed)
{
  int best_idx = -1;
  int best_degree = -1;

  for (int i = 0; i < graph->nodes.len; i++) {
    int degree;

    if (removed[i]) {
      continue;
    }

    degree = graph_degree_after_removal(graph, removed, i);

    if (degree > best_degree) {
      best_degree = degree;
      best_idx = i;
    }
  }

  return best_idx;
}

static bool choose_color_for_node(struct InterferenceGraph *graph,
                                  VecPseudoHome *homes, int node_idx,
                                  enum AsmRegister *out_reg)
{
  bool used[NUM_ALLOCATABLE_INT_REGS] = {0};
  struct InterferenceNode *node = &graph->nodes.data[node_idx];

  for (int i = 0; i < node->neighbors.len; i++) {
    int neighbor_idx = node->neighbors.data[i];
    struct InterferenceNode *neighbor = &graph->nodes.data[neighbor_idx];

    if (neighbor->is_precolored) {
      int reg_idx = allocatable_reg_index(neighbor->precolored_reg);

      if (reg_idx >= 0) {
        used[reg_idx] = true;
      }

      continue;
    }

    struct PseudoHome *neighbor_home = pseudo_home_get(homes, neighbor->pseudo);

    /*
     * If the neighbor is not colored yet, ignore it.
     * If it spilled to stack, it does not occupy a register.
     */
    if (!neighbor_home || neighbor_home->kind != PSEUDO_HOME_REG) {
      continue;
    }

    int reg_idx = allocatable_reg_index(neighbor_home->as.reg);
    assert(reg_idx >= 0 && "colored with non-allocatable register");

    used[reg_idx] = true;
  }

  for (int i = 0; i < NUM_ALLOCATABLE_INT_REGS; i++) {
    if (!used[i]) {
      *out_reg = allocatable_int_regs[i];
      return true;
    }
  }

  return false;
}

static void assign_stack_home(VecPseudoHome *homes, VecPseudoType *types,
                              struct Map *map, int *used_stack_bytes,
                              char *pseudo)
{
  struct AsmType *asm_type;
  int stack_offset;

  if (pseudo_home_get(homes, pseudo)) {
    return;
  }

  asm_type = pseudo_type_get(types, pseudo);
  assert(asm_type && "spilled pseudo has no known AsmType");

  stack_offset = get_offset(map, pseudo, *asm_type, used_stack_bytes);

  pseudo_home_add_stack(homes, pseudo, stack_offset);
}

static VecPseudoHome color_interference_graph(struct InterferenceGraph *graph,
                                              VecPseudoType *types,
                                              VecPseudo *force_stack,
                                              VecPseudo *out_spilled,
                                              struct Map *map,
                                              int *used_stack_bytes)
{
  VecPseudoHome homes = {0};
  VecSelectStack select_stack = {0};

  int k = NUM_ALLOCATABLE_INT_REGS;
  int n = graph->nodes.len;
  int remaining = n;

  bool *removed = calloc(n, sizeof(*removed));
  if (!removed) {
    perror("calloc");
    exit(1);
  }

  /*
   * First, remove pseudos that cannot live in integer registers.
   * They become stack homes immediately.
   */
  for (int i = 0; i < n; i++) {
    char *pseudo = graph->nodes.data[i].pseudo;

    if (graph->nodes.data[i].is_precolored) {
      removed[i] = true;
      remaining--;
      continue;
    }

    struct AsmType *asm_type = pseudo_type_get(types, pseudo);

    assert(asm_type && "interference node has no known AsmType");

    if (force_stack && pseudo_set_contains(force_stack, pseudo)) {
      assign_stack_home(&homes, types, map, used_stack_bytes, pseudo);
      removed[i] = true;
      remaining--;
      continue;
    }

    if (!asm_type_can_live_in_int_reg(*asm_type)) {
      assign_stack_home(&homes, types, map, used_stack_bytes, pseudo);
      removed[i] = true;
      remaining--;
    }
  }

  /*
   * Simplify phase.
   *
   * Repeatedly remove low-degree nodes. If none exists, choose a spill
   * candidate and remove it optimistically.
   */
  while (remaining > 0) {
    int node_idx;
    bool was_spill_candidate = false;

    node_idx = pick_low_degree_node(graph, removed, k);

    if (node_idx < 0) {
      node_idx = pick_spill_candidate(graph, removed);
      was_spill_candidate = true;
    }

    assert(node_idx >= 0 && "no node available during coloring");

    removed[node_idx] = true;
    remaining--;

    vec_insert(&select_stack, ((struct SelectStackEntry){
                                  .node_idx = node_idx,
                                  .was_spill_candidate = was_spill_candidate,
                              }));
  }

  /*
   * Select phase.
   *
   * Pop nodes and assign colors. Spill only if no color is available.
   */
  while (select_stack.len > 0) {
    struct SelectStackEntry entry;
    struct InterferenceNode *node;
    enum AsmRegister reg;

    entry = select_stack.data[--select_stack.len];
    node = &graph->nodes.data[entry.node_idx];

    if (pseudo_home_get(&homes, node->pseudo)) {
      continue;
    }

    if (choose_color_for_node(graph, &homes, entry.node_idx, &reg)) {
      pseudo_home_add_reg(&homes, node->pseudo, reg);
    } else {
      /*
       * Optimistic coloring failed for this node.
       *
       * Still assign a temporary stack home so debugging works, but also report
       * it so the caller can rewrite the program and rerun allocation.
       */
      if (out_spilled) {
        pseudo_set_add(out_spilled, node->pseudo);
      }

      assign_stack_home(&homes, types, map, used_stack_bytes, node->pseudo);
    }
  }

  free(removed);
  vec_free(&select_stack);

  return homes;
}

static void print_pseudo_homes(VecPseudoHome *homes)
{
  printf("Pseudo homes:\n");

  for (int i = 0; i < homes->len; i++) {
    struct PseudoHome *home = &homes->data[i];

    printf("  %-20s -> ", home->pseudo);

    if (home->kind == PSEUDO_HOME_REG) {
      struct AsmOperand op = {
          .kind = AsmOperand_REG,
          .asm_type = (struct AsmType){.kind = AsmType_LONGWORD},
          .as.reg = home->as.reg,
      };

      print_asm_operand(&op);
      printf("\n");
    } else {
      printf("stack(%d)\n", home->as.stack_offset);
    }
  }
}

static void verify_coloring(struct InterferenceGraph *graph,
                            VecPseudoHome *homes)
{
  for (int i = 0; i < graph->nodes.len; i++) {
    struct InterferenceNode *a_node = &graph->nodes.data[i];

    if (a_node->is_precolored) {
      continue;
    }

    struct PseudoHome *ha = pseudo_home_get(homes, a_node->pseudo);

    if (!ha || ha->kind != PSEUDO_HOME_REG) {
      continue;
    }

    for (int j = 0; j < a_node->neighbors.len; j++) {
      int neighbor_idx = a_node->neighbors.data[j];

      if (i > neighbor_idx) {
        continue;
      }

      struct InterferenceNode *b_node = &graph->nodes.data[neighbor_idx];

      if (b_node->is_precolored) {
        if (ha->as.reg == b_node->precolored_reg) {
          fprintf(stderr,
                  "register coloring bug: %s assigned clobbered/precolored "
                  "register %s\n",
                  a_node->pseudo, b_node->pseudo);
          abort();
        }

        continue;
      }

      struct PseudoHome *hb = pseudo_home_get(homes, b_node->pseudo);

      if (!hb || hb->kind != PSEUDO_HOME_REG) {
        continue;
      }

      if (ha->as.reg == hb->as.reg) {
        fprintf(
            stderr,
            "register coloring bug: %s and %s both assigned same register\n",
            a_node->pseudo, b_node->pseudo);
        abort();
      }
    }
  }
}

struct Move {
  int src_idx;
  int dst_idx;
};

typedef Vector(struct Move) VecMove;

static char *move_node_name(struct InterferenceGraph *graph, int idx)
{
  assert(idx >= 0 && idx < graph->nodes.len);
  return graph->nodes.data[idx].pseudo;
}

static bool interference_has_edge_idx(struct InterferenceGraph *graph,
                                      int a_idx, int b_idx)
{
  if (a_idx < 0 || b_idx < 0) {
    return false;
  }

  return int_vec_contains(&graph->nodes.data[a_idx].neighbors, b_idx);
}

static bool move_operand_node(struct InterferenceGraph *graph,
                              struct AsmOperand *op, int *out_idx)
{
  if (op->kind == AsmOperand_PSEUDO) {
    *out_idx = interference_node_index(graph, op->as.pseudo);
    return *out_idx >= 0;
  }

  if (op->kind == AsmOperand_REG) {
    char *name = precolored_reg_name(op->as.reg);
    *out_idx = interference_node_index(graph, name);
    return *out_idx >= 0;
  }

  return false;
}

static bool is_pseudo_to_pseudo_move(struct AsmInstr *instr, char **out_src,
                                     char **out_dst)
{
  if (instr->kind != AsmInstr_MOV) {
    return false;
  }

  if (instr->as.mov.src.kind != AsmOperand_PSEUDO) {
    return false;
  }

  if (instr->as.mov.dst.kind != AsmOperand_PSEUDO) {
    return false;
  }

  *out_src = instr->as.mov.src.as.pseudo;
  *out_dst = instr->as.mov.dst.as.pseudo;

  if (strcmp(*out_src, *out_dst) == 0) {
    return false;
  }

  return true;
}

static VecMove collect_moves_from_graph(struct AsmFunction *fn,
                                        struct InterferenceGraph *graph)
{
  VecMove moves = {0};

  for (int i = 0; i < fn->body.len; i++) {
    struct AsmInstr *instr = &fn->body.data[i];
    int src_idx;
    int dst_idx;

    if (instr->kind != AsmInstr_MOV) {
      continue;
    }

    if (!move_operand_node(graph, &instr->as.mov.src, &src_idx)) {
      continue;
    }

    if (!move_operand_node(graph, &instr->as.mov.dst, &dst_idx)) {
      continue;
    }

    if (src_idx == dst_idx) {
      continue;
    }

    vec_insert(&moves, ((struct Move){
                           .src_idx = src_idx,
                           .dst_idx = dst_idx,
                       }));
  }

  return moves;
}

static void print_moves(struct InterferenceGraph *graph, VecMove *moves)
{
  printf("Moves:\n");

  for (int i = 0; i < moves->len; i++) {
    int src = moves->data[i].src_idx;
    int dst = moves->data[i].dst_idx;

    printf("  %s -> %s\n", move_node_name(graph, src),
           move_node_name(graph, dst));
  }
}

static void free_moves(VecMove *moves)
{
  vec_free(moves);

  moves->capacity = 0;
  moves->len = 0;
  moves->data = NULL;
}

static void dump_moves_dot(FILE *out, struct InterferenceGraph *graph,
                           VecMove *moves)
{
  fprintf(out, "digraph moves {\n");
  fprintf(out, "  graph [fontname=\"monospace\"];\n");
  fprintf(out, "  node [shape=ellipse, fontname=\"monospace\"];\n");
  fprintf(out, "  edge [fontname=\"monospace\"];\n\n");

  for (int i = 0; i < moves->len; i++) {
    int src = moves->data[i].src_idx;
    int dst = moves->data[i].dst_idx;

    fprintf(out, "  \"");
    dot_escape(out, move_node_name(graph, src));
    fprintf(out, "\" -> \"");
    dot_escape(out, move_node_name(graph, dst));
    fprintf(out, "\" [label=\"mov\"];\n");
  }

  fprintf(out, "}\n");
}

static void write_moves_dot(struct InterferenceGraph *graph, VecMove *moves,
                            const char *path)
{
  FILE *out = fopen(path, "w");

  if (!out) {
    perror("fopen");
    return;
  }

  dump_moves_dot(out, graph, moves);

  fclose(out);
}

static bool is_same_reg_operand(struct AsmOperand *a, struct AsmOperand *b)
{
  return a->kind == AsmOperand_REG && b->kind == AsmOperand_REG &&
         a->as.reg == b->as.reg;
}

static bool is_redundant_mov(struct AsmInstr *instr)
{
  return instr->kind == AsmInstr_MOV &&
         is_same_reg_operand(&instr->as.mov.src, &instr->as.mov.dst);
}

static bool interference_has_edge(struct InterferenceGraph *graph, char *a,
                                  char *b)
{
  int ai = interference_node_index(graph, a);
  int bi = interference_node_index(graph, b);

  if (ai < 0 || bi < 0) {
    return false;
  }

  return int_vec_contains(&graph->nodes.data[ai].neighbors, bi);
}

static int interference_degree(struct InterferenceGraph *graph, char *pseudo)
{
  int idx = interference_node_index(graph, pseudo);

  if (idx < 0) {
    return 0;
  }

  return graph->nodes.data[idx].neighbors.len;
}

static bool briggs_can_coalesce(struct InterferenceGraph *graph, char *a,
                                char *b, int k)
{
  int ai = interference_node_index(graph, a);
  int bi = interference_node_index(graph, b);
  VecInt neighbors = {0};
  int high_degree_count = 0;

  assert(ai >= 0);
  assert(bi >= 0);

  for (int i = 0; i < graph->nodes.data[ai].neighbors.len; i++) {
    int n = graph->nodes.data[ai].neighbors.data[i];

    if (n != bi) {
      int_set_add(&neighbors, n);
    }
  }

  for (int i = 0; i < graph->nodes.data[bi].neighbors.len; i++) {
    int n = graph->nodes.data[bi].neighbors.data[i];

    if (n != ai) {
      int_set_add(&neighbors, n);
    }
  }

  for (int i = 0; i < neighbors.len; i++) {
    int n = neighbors.data[i];

    if (graph->nodes.data[n].neighbors.len >= k) {
      high_degree_count++;
    }
  }

  vec_free(&neighbors);

  return high_degree_count < k;
}

static int aliased_index_for_orig_rep(struct InterferenceGraph *aliased,
                                      struct InterferenceGraph *orig,
                                      int orig_rep_idx)
{
  char *name = orig->nodes.data[orig_rep_idx].pseudo;

  return interference_node_index(aliased, name);
}

static int uf_find(int *parent, int x)
{
  if (parent[x] != x) {
    parent[x] = uf_find(parent, parent[x]);
  }

  return parent[x];
}

static void uf_union(int *parent, int a, int b)
{
  int ra = uf_find(parent, a);
  int rb = uf_find(parent, b);

  if (ra != rb) {
    parent[rb] = ra;
  }
}

static bool george_ok(struct InterferenceGraph *graph, int t_idx, int r_idx,
                      int k)
{
  struct InterferenceNode *t = &graph->nodes.data[t_idx];

  if (t->is_precolored) {
    return true;
  }

  if (t->neighbors.len < k) {
    return true;
  }

  if (interference_has_edge_idx(graph, t_idx, r_idx)) {
    return true;
  }

  return false;
}

static bool george_can_coalesce(struct InterferenceGraph *graph, int pseudo_idx,
                                int precolored_idx, int k)
{
  assert(!graph->nodes.data[pseudo_idx].is_precolored);
  assert(graph->nodes.data[precolored_idx].is_precolored);

  for (int i = 0; i < graph->nodes.data[pseudo_idx].neighbors.len; i++) {
    int t_idx = graph->nodes.data[pseudo_idx].neighbors.data[i];

    if (!george_ok(graph, t_idx, precolored_idx, k)) {
      return false;
    }
  }

  return true;
}

static void uf_union_into(int *parent, int keep, int discard)
{
  int r_keep = uf_find(parent, keep);
  int r_discard = uf_find(parent, discard);

  if (r_keep != r_discard) {
    parent[r_discard] = r_keep;
  }
}

static void debug_briggs_moves(struct InterferenceGraph *graph, VecMove *moves)
{
  int k = NUM_ALLOCATABLE_INT_REGS;

  printf("Move coalescing decisions:\n");

  for (int i = 0; i < moves->len; i++) {
    int src_idx = moves->data[i].src_idx;
    int dst_idx = moves->data[i].dst_idx;

    char *src = move_node_name(graph, src_idx);
    char *dst = move_node_name(graph, dst_idx);

    bool src_pre = graph->nodes.data[src_idx].is_precolored;
    bool dst_pre = graph->nodes.data[dst_idx].is_precolored;

    if (interference_has_edge_idx(graph, src_idx, dst_idx)) {
      printf("  reject %-20s -> %-20s : constrained\n", src, dst);
      continue;
    }

    if (src_pre && dst_pre) {
      printf("  reject %-20s -> %-20s : both precolored\n", src, dst);
      continue;
    }

    if (src_pre || dst_pre) {
      int pre_idx = src_pre ? src_idx : dst_idx;
      int pseudo_idx = src_pre ? dst_idx : src_idx;

      if (george_can_coalesce(graph, pseudo_idx, pre_idx, k)) {
        printf("  accept %-20s -> %-20s : George OK\n", src, dst);
      } else {
        printf("  reject %-20s -> %-20s : George risky\n", src, dst);
      }

      continue;
    }

    if (briggs_can_coalesce(graph, src, dst, k)) {
      printf("  accept %-20s -> %-20s : Briggs OK\n", src, dst);
    } else {
      printf("  reject %-20s -> %-20s : Briggs risky\n", src, dst);
    }
  }
}

static bool asm_types_coalesce_compatible(struct AsmType a, struct AsmType b)
{
  return a.kind == b.kind && asm_type_can_live_in_int_reg(a) &&
         asm_type_can_live_in_int_reg(b);
}

static struct InterferenceGraph build_aliased_interference_graph(
    struct InterferenceGraph *orig, int *parent)
{
  struct InterferenceGraph out = {0};

  /*
   * Add representative nodes.
   */
  for (int i = 0; i < orig->nodes.len; i++) {
    int ri = orig->nodes.data[i].is_precolored ? i : uf_find(parent, i);

    if (orig->nodes.data[ri].is_precolored) {
      interference_add_precolored_reg(&out,
                                      orig->nodes.data[ri].precolored_reg);
    } else {
      interference_add_node(&out, orig->nodes.data[ri].pseudo);
    }
  }

  /*
   * Add edges between representatives.
   */
  for (int i = 0; i < orig->nodes.len; i++) {
    int ri = orig->nodes.data[i].is_precolored ? i : uf_find(parent, i);

    for (int j = 0; j < orig->nodes.data[i].neighbors.len; j++) {
      int n = orig->nodes.data[i].neighbors.data[j];
      int rn = orig->nodes.data[n].is_precolored ? n : uf_find(parent, n);

      if (ri == rn) {
        continue;
      }

      if (orig->nodes.data[ri].is_precolored &&
          orig->nodes.data[rn].is_precolored) {
        int ai = interference_add_precolored_reg(
            &out, orig->nodes.data[ri].precolored_reg);
        int bi = interference_add_precolored_reg(
            &out, orig->nodes.data[rn].precolored_reg);

        interference_add_edge_by_index(&out, ai, bi);
      } else if (orig->nodes.data[ri].is_precolored) {
        int ai = interference_add_precolored_reg(
            &out, orig->nodes.data[ri].precolored_reg);
        int bi = interference_add_node(&out, orig->nodes.data[rn].pseudo);

        interference_add_edge_by_index(&out, ai, bi);
      } else if (orig->nodes.data[rn].is_precolored) {
        int ai = interference_add_node(&out, orig->nodes.data[ri].pseudo);
        int bi = interference_add_precolored_reg(
            &out, orig->nodes.data[rn].precolored_reg);

        interference_add_edge_by_index(&out, ai, bi);
      } else {
        interference_add_edge(&out, orig->nodes.data[ri].pseudo,
                              orig->nodes.data[rn].pseudo);
      }
    }
  }

  return out;
}

static int *coalesce_briggs_george(struct InterferenceGraph *orig,
                                   VecPseudoType *types, VecMove *moves)
{
  int n = orig->nodes.len;
  int *parent = malloc(sizeof(*parent) * n);

  if (!parent) {
    perror("malloc");
    exit(1);
  }

  for (int i = 0; i < n; i++) {
    parent[i] = i;
  }

  bool changed = true;

  while (changed) {
    changed = false;

    struct InterferenceGraph aliased =
        build_aliased_interference_graph(orig, parent);

    for (int i = 0; i < moves->len; i++) {
      int src_idx = moves->data[i].src_idx;
      int dst_idx = moves->data[i].dst_idx;

      int src_rep_idx = uf_find(parent, src_idx);
      int dst_rep_idx = uf_find(parent, dst_idx);

      if (src_rep_idx == dst_rep_idx) {
        continue;
      }

      int src_alias_idx =
          aliased_index_for_orig_rep(&aliased, orig, src_rep_idx);
      int dst_alias_idx =
          aliased_index_for_orig_rep(&aliased, orig, dst_rep_idx);

      if (src_alias_idx < 0 || dst_alias_idx < 0) {
        continue;
      }

      bool src_pre = orig->nodes.data[src_rep_idx].is_precolored;
      bool dst_pre = orig->nodes.data[dst_rep_idx].is_precolored;

      /*
       * If they already interfere, the move is constrained.
       */
      if (interference_has_edge_idx(&aliased, src_alias_idx, dst_alias_idx)) {
        continue;
      }

      /*
       * precolored <-> precolored: never coalesce.
       */
      if (src_pre && dst_pre) {
        continue;
      }

      /*
       * George: pseudo <-> precolored.
       */
      if (src_pre || dst_pre) {
        int pre_orig_idx = src_pre ? src_rep_idx : dst_rep_idx;
        int pseudo_orig_idx = src_pre ? dst_rep_idx : src_rep_idx;

        int pre_alias_idx = src_pre ? src_alias_idx : dst_alias_idx;
        int pseudo_alias_idx = src_pre ? dst_alias_idx : src_alias_idx;

        if (george_can_coalesce(&aliased, pseudo_alias_idx, pre_alias_idx,
                                NUM_ALLOCATABLE_INT_REGS)) {
#ifdef DEBUG_COALESCE
          printf("george coalesce %s <- %s\n",
                 orig->nodes.data[pre_orig_idx].pseudo,
                 orig->nodes.data[pseudo_orig_idx].pseudo);
#endif

          /*
           * Keep the precolored representative.
           */
          uf_union_into(parent, pre_orig_idx, pseudo_orig_idx);
          changed = true;
          break;
        }

        continue;
      }

      /*
       * Briggs: pseudo <-> pseudo.
       */
      {
        char *src_rep = orig->nodes.data[src_rep_idx].pseudo;
        char *dst_rep = orig->nodes.data[dst_rep_idx].pseudo;

        struct AsmType *src_type = pseudo_type_get(types, src_rep);
        struct AsmType *dst_type = pseudo_type_get(types, dst_rep);

        assert(src_type && dst_type);

        if (!asm_types_coalesce_compatible(*src_type, *dst_type)) {
          continue;
        }

        if (briggs_can_coalesce(&aliased, src_rep, dst_rep,
                                NUM_ALLOCATABLE_INT_REGS)) {
#ifdef DEBUG_COALESCE
          printf("briggs coalesce %s <- %s\n", src_rep, dst_rep);
#endif

          uf_union_into(parent, src_rep_idx, dst_rep_idx);
          changed = true;
          break;
        }
      }
    }

    free_interference_graph(&aliased);
  }

  return parent;
}

static VecPseudoHome expand_coalesced_homes(struct InterferenceGraph *orig,
                                            int *parent,
                                            VecPseudoHome *coalesced_homes)
{
  VecPseudoHome homes = {0};

  for (int i = 0; i < orig->nodes.len; i++) {
    if (orig->nodes.data[i].is_precolored) {
      continue;
    }

    int rep_idx = uf_find(parent, i);

    char *pseudo = orig->nodes.data[i].pseudo;

    if (orig->nodes.data[rep_idx].is_precolored) {
      pseudo_home_add_reg(&homes, pseudo,
                          orig->nodes.data[rep_idx].precolored_reg);
      continue;
    }

    char *rep = orig->nodes.data[rep_idx].pseudo;
    struct PseudoHome *rep_home = pseudo_home_get(coalesced_homes, rep);

    assert(rep_home && "coalesced representative has no home");

    if (rep_home->kind == PSEUDO_HOME_REG) {
      pseudo_home_add_reg(&homes, pseudo, rep_home->as.reg);
    } else {
      pseudo_home_add_stack(&homes, pseudo, rep_home->as.stack_offset);
    }
  }

  return homes;
}

static bool same_asm_operand(struct AsmOperand *a, struct AsmOperand *b)
{
  if (a->kind != b->kind) {
    return false;
  }

  switch (a->kind) {
    case AsmOperand_REG:
      return a->as.reg == b->as.reg;

    case AsmOperand_STACK:
      return a->as.stack_offset == b->as.stack_offset;

    case AsmOperand_IMM:
      return a->as.imm == b->as.imm;

    case AsmOperand_PSEUDO:
      return strcmp(a->as.pseudo, b->as.pseudo) == 0;

    default:
      return false;
  }
}

static void remove_redundant_moves(struct AsmFunction *fn)
{
  int out = 0;

  for (int i = 0; i < fn->body.len; i++) {
    struct AsmInstr *instr = &fn->body.data[i];

    if (instr->kind == AsmInstr_MOV &&
        same_asm_operand(&instr->as.mov.src, &instr->as.mov.dst)) {
      continue;
    }

    fn->body.data[out++] = fn->body.data[i];
  }

  fn->body.len = out;
}

static bool instr_is_call(struct AsmInstr *instr)
{
  return instr->kind == AsmInstr_CALL;
}

static bool pseudo_set_any_contains(VecPseudo *set, char *pseudo)
{
  return pseudo_set_contains(set, pseudo);
}

static VecPseudo collect_call_live_pseudos(struct AsmFunction *fn,
                                           struct InstrLiveness *lv)
{
  VecPseudo call_live = {0};

  for (int i = 0; i < fn->body.len; i++) {
    if (!instr_is_call(&fn->body.data[i])) {
      continue;
    }

    for (int j = 0; j < lv[i].live_after.len; j++) {
      pseudo_set_add(&call_live, lv[i].live_after.data[j]);
    }
  }

  return call_live;
}

static bool is_callee_saved_reg(enum AsmRegister reg)
{
  for (int i = 0; i < NUM_CALLEE_SAVED_REGS; i++) {
    if (callee_saved_regs[i] == reg) {
      return true;
    }
  }

  return false;
}

static bool reg_vec_contains(VecInt *regs, enum AsmRegister reg)
{
  for (int i = 0; i < regs->len; i++) {
    if (regs->data[i] == reg) {
      return true;
    }
  }

  return false;
}

static void reg_vec_add(VecInt *regs, enum AsmRegister reg)
{
  if (!reg_vec_contains(regs, reg)) {
    vec_insert(regs, reg);
  }
}

static VecInt collect_used_callee_saved_regs(VecPseudoHome *homes)
{
  VecInt used = {0};

  for (int i = 0; i < homes->len; i++) {
    struct PseudoHome *home = &homes->data[i];

    if (home->kind != PSEUDO_HOME_REG) {
      continue;
    }

    if (is_callee_saved_reg(home->as.reg)) {
      reg_vec_add(&used, home->as.reg);
    }
  }

  return used;
}

static struct AsmOperand make_reg_operand(enum AsmRegister reg,
                                          struct AsmType asm_type)
{
  return (struct AsmOperand){
      .kind = AsmOperand_REG,
      .asm_type = asm_type,
      .as.reg = reg,
  };
}

static char *fresh_spill_tmp(void)
{
  return mkstr("tmp.%d", mktmp());
}

static struct AsmOperand make_pseudo_operand(char *pseudo,
                                             struct AsmType asm_type)
{
  return (struct AsmOperand){
      .kind = AsmOperand_PSEUDO,
      .asm_type = asm_type,
      .as.pseudo = strdup(pseudo),
  };
}

static struct AsmOperand make_stack_operand(int offset, struct AsmType asm_type)
{
  return (struct AsmOperand){
      .kind = AsmOperand_STACK,
      .asm_type = asm_type,
      .as.stack_offset = offset,
  };
}

static struct AsmInstr make_mov_instr(struct AsmOperand src,
                                      struct AsmOperand dst,
                                      struct AsmType asm_type)
{
  struct AsmInstr instr = {0};

  instr.kind = AsmInstr_MOV;
  instr.asm_type = asm_type;
  instr.as.mov.src = src;
  instr.as.mov.dst = dst;

  return instr;
}

static bool is_spilled_pseudo_operand(struct AsmOperand *op, VecPseudo *spilled)
{
  return op->kind == AsmOperand_PSEUDO &&
         pseudo_set_contains(spilled, op->as.pseudo);
}

static struct AsmInstr make_push_reg(enum AsmRegister reg)
{
  return (struct AsmInstr){
      .kind = AsmInstr_PUSH,
      .asm_type = (struct AsmType){.kind = AsmType_QUADWORD},
      .as.push =
          {
              .op = make_reg_operand(
                  reg, (struct AsmType){.kind = AsmType_QUADWORD}),
          },
  };
}

static struct AsmInstr make_pop_reg(enum AsmRegister reg)
{
  return (struct AsmInstr){
      .kind = AsmInstr_POP,
      .asm_type = (struct AsmType){.kind = AsmType_QUADWORD},
      .as.pop =
          {
              .op = make_reg_operand(
                  reg, (struct AsmType){.kind = AsmType_QUADWORD}),
          },
  };
}

static bool is_reg_operand(struct AsmOperand *op, enum AsmRegister reg)
{
  return op->kind == AsmOperand_REG && op->as.reg == reg;
}

static bool is_restore_sp_from_bp(struct AsmInstr *instr)
{
  return instr->kind == AsmInstr_MOV &&
         is_reg_operand(&instr->as.mov.src, BP) &&
         is_reg_operand(&instr->as.mov.dst, SP);
}

static bool is_pop_bp(struct AsmInstr *instr)
{
  return instr->kind == AsmInstr_POP && is_reg_operand(&instr->as.pop.op, BP);
}

static bool is_stack_alloc_instr(struct AsmInstr *instr)
{
  return instr->kind == AsmInstr_BIN &&
         instr->as.binary.kind == AsmInstrBinary_SUB &&
         instr->as.binary.lhs.kind == AsmOperand_IMM &&
         is_reg_operand(&instr->as.binary.rhs, SP);
}

static void save_restore_callee_saved_regs(struct AsmFunction *fn,
                                           VecInt *used_regs)
{
  if (used_regs->len == 0) {
    return;
  }

  VecAsmInstr new_body = {0};

  for (int i = 0; i < fn->body.len; i++) {
    /*
     * Before every epilogue `mov %rbp, %rsp`, restore in reverse order.
     */
    if (is_restore_sp_from_bp(&fn->body.data[i])) {
      for (int r = used_regs->len - 1; r >= 0; r--) {
        vec_insert(&new_body,
                   make_pop_reg((enum AsmRegister) used_regs->data[r]));
      }
    }

    vec_insert(&new_body, fn->body.data[i]);

    /*
     * After stack allocation, save all used callee-saved registers.
     */
    if (is_stack_alloc_instr(&fn->body.data[i])) {
      for (int r = 0; r < used_regs->len; r++) {
        vec_insert(&new_body,
                   make_push_reg((enum AsmRegister) used_regs->data[r]));
      }
    }
  }

  vec_free(&fn->body);
  fn->body = new_body;
}

static void patch_stack_alloc(struct AsmFunction *fn, int stack_size)
{
  for (int i = 0; i < fn->body.len; i++) {
    struct AsmInstr *instr = &fn->body.data[i];

    if (instr->kind == AsmInstr_BIN &&
        instr->as.binary.kind == AsmInstrBinary_SUB &&
        instr->as.binary.lhs.kind == AsmOperand_IMM &&
        is_reg_operand(&instr->as.binary.rhs, SP)) {
      instr->as.binary.lhs.as.imm = stack_size;
      return;
    }
  }

  assert(0 && "could not find prologue stack allocation");
}

static void rewrite_spilled_use(VecAsmInstr *before, struct AsmOperand *op,
                                VecPseudo *spilled, struct Map *map,
                                int *used_stack_bytes)
{
  if (!is_spilled_pseudo_operand(op, spilled)) {
    return;
  }

  struct AsmType asm_type = op->asm_type;
  int offset = get_offset(map, op->as.pseudo, asm_type, used_stack_bytes);

  char *tmp = fresh_spill_tmp();

  struct AsmOperand stack_op = make_stack_operand(offset, asm_type);
  struct AsmOperand tmp_dst = make_pseudo_operand(tmp, asm_type);

  vec_insert(before, make_mov_instr(stack_op, tmp_dst, asm_type));

  *op = make_pseudo_operand(tmp, asm_type);

  free(tmp);
}

static void rewrite_spilled_def(VecAsmInstr *after, struct AsmOperand *op,
                                VecPseudo *spilled, struct Map *map,
                                int *used_stack_bytes)
{
  if (!is_spilled_pseudo_operand(op, spilled)) {
    return;
  }

  struct AsmType asm_type = op->asm_type;
  int offset = get_offset(map, op->as.pseudo, asm_type, used_stack_bytes);

  char *tmp = fresh_spill_tmp();

  struct AsmOperand tmp_src = make_pseudo_operand(tmp, asm_type);
  struct AsmOperand stack_op = make_stack_operand(offset, asm_type);

  *op = make_pseudo_operand(tmp, asm_type);

  vec_insert(after, make_mov_instr(tmp_src, stack_op, asm_type));

  free(tmp);
}

static void rewrite_spilled_use_def(VecAsmInstr *before, VecAsmInstr *after,
                                    struct AsmOperand *op, VecPseudo *spilled,
                                    struct Map *map, int *used_stack_bytes)
{
  if (!is_spilled_pseudo_operand(op, spilled)) {
    return;
  }

  struct AsmType asm_type = op->asm_type;
  int offset = get_offset(map, op->as.pseudo, asm_type, used_stack_bytes);

  char *tmp = fresh_spill_tmp();

  struct AsmOperand stack_load = make_stack_operand(offset, asm_type);
  struct AsmOperand tmp_load_dst = make_pseudo_operand(tmp, asm_type);

  vec_insert(before, make_mov_instr(stack_load, tmp_load_dst, asm_type));

  *op = make_pseudo_operand(tmp, asm_type);

  struct AsmOperand tmp_store_src = make_pseudo_operand(tmp, asm_type);
  struct AsmOperand stack_store = make_stack_operand(offset, asm_type);

  vec_insert(after, make_mov_instr(tmp_store_src, stack_store, asm_type));

  free(tmp);
}

static void append_instrs(VecAsmInstr *dst, VecAsmInstr *src)
{
  for (int i = 0; i < src->len; i++) {
    vec_insert(dst, src->data[i]);
  }
}

static void rewrite_spills(struct AsmFunction *fn, VecPseudo *spilled,
                           struct Map *map, int *used_stack_bytes)
{
  if (spilled->len == 0) {
    return;
  }

  VecAsmInstr new_body = {0};

  for (int i = 0; i < fn->body.len; i++) {
    struct AsmInstr instr = fn->body.data[i];

    VecAsmInstr before = {0};
    VecAsmInstr after = {0};

    switch (instr.kind) {
      case AsmInstr_MOV:
        rewrite_spilled_use(&before, &instr.as.mov.src, spilled, map,
                            used_stack_bytes);

        rewrite_spilled_def(&after, &instr.as.mov.dst, spilled, map,
                            used_stack_bytes);
        break;

      case AsmInstr_BIN:
        /*
         * x86-style binary:
         *   rhs = rhs OP lhs
         *
         * lhs is use-only.
         * rhs is use+def.
         */
        rewrite_spilled_use(&before, &instr.as.binary.lhs, spilled, map,
                            used_stack_bytes);

        rewrite_spilled_use_def(&before, &after, &instr.as.binary.rhs, spilled,
                                map, used_stack_bytes);
        break;

      case AsmInstr_CMP:
        rewrite_spilled_use(&before, &instr.as.cmp.lhs, spilled, map,
                            used_stack_bytes);

        rewrite_spilled_use(&before, &instr.as.cmp.rhs, spilled, map,
                            used_stack_bytes);
        break;

      case AsmInstr_CVT:
        rewrite_spilled_use(&before, &instr.as.cvt.src, spilled, map,
                            used_stack_bytes);

        rewrite_spilled_def(&after, &instr.as.cvt.dst, spilled, map,
                            used_stack_bytes);
        break;

      case AsmInstr_SetCC:
        rewrite_spilled_def(&after, &instr.as.setcc.op, spilled, map,
                            used_stack_bytes);
        break;

      case AsmInstr_UNARY:
        rewrite_spilled_use_def(&before, &after, &instr.as.unary.op, spilled,
                                map, used_stack_bytes);
        break;

      case AsmInstr_PUSH:
        rewrite_spilled_use(&before, &instr.as.push.op, spilled, map,
                            used_stack_bytes);
        break;

      case AsmInstr_POP:
        rewrite_spilled_def(&after, &instr.as.pop.op, spilled, map,
                            used_stack_bytes);
        break;

      case AsmInstr_LEA:
        rewrite_spilled_use(&before, &instr.as.lea.src, spilled, map,
                            used_stack_bytes);

        rewrite_spilled_def(&after, &instr.as.lea.dst, spilled, map,
                            used_stack_bytes);
        break;

      case AsmInstr_CALL:
      case AsmInstr_RET:
      case AsmInstr_JMP:
      case AsmInstr_JmpCC:
      case AsmInstr_LBL:
      case AsmInstr_REP_MOVSB:
        break;

      default:
        assert(0 && "Unhandled instruction in rewrite_spills");
    }

    append_instrs(&new_body, &before);
    vec_insert(&new_body, instr);
    append_instrs(&new_body, &after);

    vec_free(&before);
    vec_free(&after);
  }

  vec_free(&fn->body);
  fn->body = new_body;
}

static void print_spilled_pseudos(VecPseudo *spilled)
{
  printf("Spilled pseudos:\n");

  for (int i = 0; i < spilled->len; i++) {
    printf("  %s\n", spilled->data[i]);
  }
}

#define MAX_REGALLOC_ATTEMPTS 20

struct AsmFunction *regalloc_fn(struct AsmFunction *fn, struct Map *map,
                                int *used_stack_bytes,
                                int *used_callee_saved_count)
{
  for (int attempt = 0; attempt < MAX_REGALLOC_ATTEMPTS; attempt++) {
    printf("regalloc attempt %d\n", attempt);

    struct InstrLiveness *lv;
    int original_instr_count;

    VecPseudoType types;
    VecMove moves;
    VecPseudoHome homes;
    VecPseudoHome coalesced_homes;
    VecInt used_callee_saved;
    VecPseudo spilled = {0};

    struct InterferenceGraph interference;
    struct InterferenceGraph coalesced_graph;

    int *coalesce_parent;

#ifdef DEBUG_INTERVALS
    VecLiveInterval intervals;
#endif

    original_instr_count = fn->body.len;
    lv = compute_liveness_cfg(fn);

    types = collect_pseudo_types(fn);

    interference = build_interference_graph(fn, lv);

#ifdef DEBUG_INTERFERENCE
    print_interference_graph(&interference);
#endif

#ifdef DEBUG_INTERFERENCE_DOT
    {
      char *path = mkstr("%s.interference.%d.dot", fn->name, attempt);
      write_interference_graph_dot(&interference, path);
      free(path);
    }
#endif

    moves = collect_moves_from_graph(fn, &interference);

#ifdef DEBUG_MOVES
    print_moves(&interference, &moves);
#endif

#ifdef DEBUG_MOVES_DOT
    {
      char *path = mkstr("%s.moves.%d.dot", fn->name, attempt);
      write_moves_dot(&interference, &moves, path);
      free(path);
    }
#endif

#ifdef DEBUG_BRIGGS
    debug_briggs_moves(&interference, &moves);
#endif

#ifdef DEBUG_INTERVALS
    intervals = build_live_intervals(lv, original_instr_count, &types);
    sort_live_intervals(&intervals);
    print_live_intervals(&intervals);
    free_live_intervals(&intervals);
#endif

    coalesce_parent = coalesce_briggs_george(&interference, &types, &moves);

    coalesced_graph =
        build_aliased_interference_graph(&interference, coalesce_parent);

#ifdef DEBUG_COALESCED_INTERFERENCE
    printf("Coalesced ");
    print_interference_graph(&coalesced_graph);
#endif

    coalesced_homes = color_interference_graph(&coalesced_graph, &types, NULL,
                                               &spilled, map, used_stack_bytes);

    homes = expand_coalesced_homes(&interference, coalesce_parent,
                                   &coalesced_homes);

    /*
     * If coloring spilled anything, rewrite the pseudo program and retry.
     */
    if (spilled.len > 0) {
#ifdef DEBUG_SPILLS
      print_spilled_pseudos(&spilled);
#endif

      rewrite_spills(fn, &spilled, map, used_stack_bytes);

      free_pseudo_homes(&homes);
      free_pseudo_homes(&coalesced_homes);

      free_interference_graph(&coalesced_graph);
      free_interference_graph(&interference);

      free(coalesce_parent);

      free_moves(&moves);
      free_pseudo_types(&types);
      free_liveness(lv, original_instr_count);
      pseudo_set_free(&spilled);

      /*
       * Restart on the rewritten program.
       */
      continue;
    }

    /*
     * No spills: this is the final allocation.
     */
    verify_coloring(&interference, &homes);

#ifdef DEBUG_HOMES
    print_pseudo_homes(&homes);
#endif

    used_callee_saved = collect_used_callee_saved_regs(&homes);
    *used_callee_saved_count = used_callee_saved.len;

    for (int i = 0; i < fn->body.len; i++) {
      regalloc_instr_from_homes(&fn->body.data[i], &homes, map,
                                used_stack_bytes);
    }

    remove_redundant_moves(fn);
    save_restore_callee_saved_regs(fn, &used_callee_saved);

    vec_free(&used_callee_saved);

    free_pseudo_homes(&homes);
    free_pseudo_homes(&coalesced_homes);

    free_interference_graph(&coalesced_graph);
    free_interference_graph(&interference);

    free(coalesce_parent);

    free_moves(&moves);
    free_pseudo_types(&types);
    free_liveness(lv, original_instr_count);
    pseudo_set_free(&spilled);

    return fn;
  }

  fprintf(stderr, "register allocation did not converge after %d attempts\n",
          MAX_REGALLOC_ATTEMPTS);
  abort();
}

static int compute_frame_stack_adjustment(int local_stack_bytes,
                                          int used_callee_saved_count)
{
  int callee_saved_bytes = 8 * used_callee_saved_count;
  int frame_bytes = local_stack_bytes + callee_saved_bytes;

  int padding = frame_bytes % 16 != 0 ? 16 - (frame_bytes % 16) : 0;

  /*
   * Only return the amount for `subq $N, %rsp`.
   * The callee-saved bytes are created by actual PUSH instructions.
   */
  return local_stack_bytes + padding;
}

struct AsmProgram *regalloc(struct AsmProgram *asmcode)
{
  for (int i = 0; i < asmcode->funcs.len; i++) {
    struct Map *map = malloc(sizeof(struct Map));
    memset(map, 0, sizeof(struct Map));
    map->next = NULL;

    int used_stack_bytes = 0;
    int used_callee_saved_count = 0;

    regalloc_fn(&asmcode->funcs.data[i], map, &used_stack_bytes,
                &used_callee_saved_count);

    int stack_size = compute_frame_stack_adjustment(used_stack_bytes,
                                                    used_callee_saved_count);
    patch_stack_alloc(&asmcode->funcs.data[i], stack_size);
  }
  return asmcode;
}

struct AsmProgram *replace_pseudo(struct AsmProgram *asmcode)
{
  for (int i = 0; i < asmcode->funcs.len; i++) {
    struct Map *map = malloc(sizeof(struct Map));
    memset(map, 0, sizeof(struct Map));
    map->next = NULL;

    int used_stack_bytes = 0;

    for (int j = 0; j < asmcode->funcs.data[i].body.len; j++) {
      struct AsmInstr *asminstr = &asmcode->funcs.data[i].body.data[j];

      switch (asminstr->kind) {
        case AsmInstr_CVT:
          replace_operand_pseudo(&asminstr->as.cvt.src, map, &used_stack_bytes);
          replace_operand_pseudo(&asminstr->as.cvt.dst, map, &used_stack_bytes);
          break;
        case AsmInstr_UNARY:
          replace_operand_pseudo(&asminstr->as.unary.op, map,
                                 &used_stack_bytes);
          break;
        case AsmInstr_LEA:
          replace_operand_pseudo(&asminstr->as.lea.src, map, &used_stack_bytes);
          replace_operand_pseudo(&asminstr->as.lea.dst, map, &used_stack_bytes);
          break;
        case AsmInstr_SetCC:
          replace_operand_pseudo(&asminstr->as.setcc.op, map,
                                 &used_stack_bytes);
          break;
        case AsmInstr_MOV:
          replace_operand_pseudo(&asminstr->as.mov.src, map, &used_stack_bytes);
          replace_operand_pseudo(&asminstr->as.mov.dst, map, &used_stack_bytes);
          break;
        case AsmInstr_BIN:
          replace_operand_pseudo(&asminstr->as.binary.lhs, map,
                                 &used_stack_bytes);
          replace_operand_pseudo(&asminstr->as.binary.rhs, map,
                                 &used_stack_bytes);
          break;
        case AsmInstr_CMP:
          replace_operand_pseudo(&asminstr->as.cmp.lhs, map, &used_stack_bytes);
          replace_operand_pseudo(&asminstr->as.cmp.rhs, map, &used_stack_bytes);
          break;
        case AsmInstr_PUSH:
          replace_operand_pseudo(&asminstr->as.push.op, map, &used_stack_bytes);
          break;
        case AsmInstr_POP:
          replace_operand_pseudo(&asminstr->as.pop.op, map, &used_stack_bytes);
          break;
        case AsmInstr_RET:
        case AsmInstr_CALL:
        case AsmInstr_JMP:
        case AsmInstr_JmpCC:
        case AsmInstr_LBL:
          break;
      }
    }

    int stack_size = align_up_int(used_stack_bytes, 16);
    asmcode->funcs.data[i].body.data[2].as.binary.lhs.as.imm = stack_size;

    struct Map *curr = map, *tmp;
    while (curr) {
      tmp = curr;
      curr = tmp->next;
      free(tmp);
    }
  }

  return asmcode;
}

struct AsmProgram *fixup(struct AsmProgram *prog)
{
  for (int i = 0; i < prog->funcs.len; i++) {
    VecAsmInstr instrs = {0};
    for (int j = 0; j < prog->funcs.data[i].body.len; j++) {
      struct AsmInstr *asminstr = &prog->funcs.data[i].body.data[j];
      switch (asminstr->kind) {
        case AsmInstr_CMP: {
          bool is_float, is_both_stack, is_dst_imm, is_dst_float_stack;

          is_float = (asminstr->asm_type.kind == AsmType_FLOAT ||
                      asminstr->asm_type.kind == AsmType_DOUBLE);

          is_both_stack = (asminstr->as.cmp.lhs.kind == AsmOperand_STACK &&
                           asminstr->as.cmp.rhs.kind == AsmOperand_STACK);

          is_dst_imm = (asminstr->as.cmp.rhs.kind == AsmOperand_IMM);

          is_dst_float_stack =
              (is_float && asminstr->as.cmp.rhs.kind == AsmOperand_STACK);

          if (is_both_stack || is_dst_imm || is_dst_float_stack) {
            enum AsmRegister scratch_reg = is_float ? XMM8 : R10;
            struct AsmOperand scratch_op = {.kind = AsmOperand_REG,
                                            .as.reg = scratch_reg,
                                            .asm_type = asminstr->asm_type};
            struct AsmInstr i1 = {0}, i2 = {0};

            i1.kind = AsmInstr_MOV;
            i1.as.mov.src = asminstr->as.cmp.rhs;
            i1.as.mov.dst = scratch_op;
            i1.asm_type = asminstr->asm_type;

            i2.kind = AsmInstr_CMP;
            i2.as.cmp.lhs = asminstr->as.cmp.lhs;
            i2.as.cmp.rhs = scratch_op;
            i2.asm_type = asminstr->asm_type;

            vec_insert(&instrs, i1);
            vec_insert(&instrs, i2);
          } else {
            vec_insert(&instrs, *asminstr);
          }
          break;
        }
        case AsmInstr_CVT: {
          if (asminstr->as.cvt.dst.kind == AsmOperand_STACK) {
            bool is_float_dst =
                (asminstr->as.cvt.dst.asm_type.kind == AsmType_FLOAT ||
                 asminstr->as.cvt.dst.asm_type.kind == AsmType_DOUBLE);

            struct AsmOperand scratch_op = {
                .kind = AsmOperand_REG,
                .as.reg = is_float_dst ? XMM8 : R10,
                .asm_type = asminstr->as.cvt.dst.asm_type};

            struct AsmInstr i1 = {0}, i2 = {0};

            i1.kind = AsmInstr_CVT;
            i1.as.cvt.kind = asminstr->as.cvt.kind;
            i1.as.cvt.src = asminstr->as.cvt.src;
            i1.as.cvt.dst = scratch_op;
            i1.asm_type = asminstr->asm_type;

            i2.kind = AsmInstr_MOV;
            i2.as.mov.src = scratch_op;
            i2.as.mov.dst = asminstr->as.cvt.dst;
            i2.asm_type = asminstr->as.cvt.dst.asm_type;

            vec_insert(&instrs, i1);
            vec_insert(&instrs, i2);
          } else {
            vec_insert(&instrs, *asminstr);
          }
          break;
        }
        case AsmInstr_LEA: {
          if (asminstr->as.lea.dst.kind == AsmOperand_STACK) {
            struct AsmOperand scratch_op = {
                .kind = AsmOperand_REG,
                .as.reg = R10,
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};
            struct AsmInstr i1 = {0}, i2 = {0};

            i1.kind = AsmInstr_LEA;
            i1.as.lea.src = asminstr->as.lea.src;
            i1.as.lea.dst = scratch_op;
            i1.asm_type = (struct AsmType){.kind = AsmType_QUADWORD};

            i2.kind = AsmInstr_MOV;
            i2.as.mov.src = scratch_op;
            i2.as.mov.dst = asminstr->as.lea.dst;
            i2.asm_type = (struct AsmType){.kind = AsmType_QUADWORD};

            vec_insert(&instrs, i1);
            vec_insert(&instrs, i2);
          } else {
            vec_insert(&instrs, *asminstr);
          }
          break;
        }
        case AsmInstr_MOV: {
          if ((asminstr->as.mov.src.kind == AsmOperand_STACK &&
               asminstr->as.mov.dst.kind == AsmOperand_STACK) ||
              ((asminstr->asm_type.kind == AsmType_FLOAT ||
                asminstr->asm_type.kind == AsmType_DOUBLE) &&
               asminstr->as.mov.dst.kind == AsmOperand_STACK) ||
              (asminstr->as.mov.src.kind == AsmOperand_MEMORY &&
               asminstr->as.mov.dst.kind == AsmOperand_STACK)) {
            enum AsmRegister scratch_reg;
            struct AsmOperand scratch_op;
            struct AsmInstrMov mov1, mov2;
            struct AsmInstr i1 = {0}, i2 = {0};

            scratch_reg = (asminstr->asm_type.kind == AsmType_FLOAT ||
                           asminstr->asm_type.kind == AsmType_DOUBLE)
                              ? XMM8
                              : R10;
            scratch_op.kind = AsmOperand_REG;
            scratch_op.as.reg = scratch_reg;
            scratch_op.asm_type = asminstr->asm_type;

            i1.kind = AsmInstr_MOV;
            mov1.src = asminstr->as.mov.src;
            mov1.dst = scratch_op;
            i1.as.mov = mov1;
            i1.asm_type = asminstr->asm_type;

            i2.kind = AsmInstr_MOV;
            mov2.src = scratch_op;
            mov2.dst = asminstr->as.mov.dst;
            i2.as.mov = mov2;
            i2.asm_type = asminstr->asm_type;

            vec_insert(&instrs, i1);
            vec_insert(&instrs, i2);
          } else {
            vec_insert(&instrs, *asminstr);
          }

          break;
        }
        case AsmInstr_BIN: {
          bool is_float = (asminstr->asm_type.kind == AsmType_FLOAT ||
                           asminstr->asm_type.kind == AsmType_DOUBLE);

          if (is_shift_asm_binary(asminstr->as.binary.kind) &&
              asminstr->as.binary.lhs.kind != AsmOperand_IMM) {
            struct AsmOperand count_src = asminstr->as.binary.lhs;
            count_src.asm_type = (struct AsmType){.kind = AsmType_BYTE};

            struct AsmOperand cl = {
                .kind = AsmOperand_REG,
                .as.reg = CX,
                .asm_type = (struct AsmType){.kind = AsmType_BYTE}};

            struct AsmInstr i1 = {0}, i2 = {0};
            i1.kind = AsmInstr_MOV;
            i1.as.mov.src = count_src;
            i1.as.mov.dst = cl;
            i1.asm_type = (struct AsmType){.kind = AsmType_BYTE};

            i2.kind = AsmInstr_BIN;
            i2.as.binary.kind = asminstr->as.binary.kind;
            i2.as.binary.lhs = cl;
            i2.as.binary.rhs = asminstr->as.binary.rhs;
            i2.asm_type = asminstr->asm_type;

            vec_insert(&instrs, i1);
            vec_insert(&instrs, i2);
            break;
          }

          /* imul and float ops cannot use mem as dst */
          if ((asminstr->as.binary.kind == AsmInstrBinary_MUL || is_float) &&
              asminstr->as.binary.rhs.kind == AsmOperand_STACK) {
            enum AsmRegister scratch_reg = is_float ? XMM8 : R10;
            struct AsmOperand scratch_op = {.kind = AsmOperand_REG,
                                            .as.reg = scratch_reg,
                                            .asm_type = asminstr->asm_type};
            struct AsmInstrMov mov1, mov2;
            struct AsmInstrBinary bin;
            struct AsmInstr i1 = {0}, i2 = {0}, i3 = {0};

            i1.kind = AsmInstr_MOV;
            mov1.src = asminstr->as.binary.rhs;
            mov1.dst = scratch_op;
            i1.as.mov = mov1;
            i1.asm_type = asminstr->asm_type;

            i2.kind = AsmInstr_BIN;
            bin.kind = asminstr->as.binary.kind;
            bin.lhs = asminstr->as.binary.lhs;
            bin.rhs = scratch_op;
            i2.as.binary = bin;
            i2.asm_type = asminstr->asm_type;

            i3.kind = AsmInstr_MOV;
            mov2.src = scratch_op;
            mov2.dst = asminstr->as.binary.rhs;
            i3.as.mov = mov2;
            i3.asm_type = asminstr->asm_type;

            vec_insert(&instrs, i1);
            vec_insert(&instrs, i2);
            vec_insert(&instrs, i3);

            break;
          }

          /* integer binary ops cannot use mem as both operands */
          if (asminstr->as.binary.lhs.kind == AsmOperand_STACK &&
              asminstr->as.binary.rhs.kind == AsmOperand_STACK) {
            enum AsmRegister scratch_reg = R10;
            struct AsmOperand scratch_op;
            struct AsmInstrMov mov;
            struct AsmInstrBinary bin;
            struct AsmInstr i1 = {0}, i2 = {0};

            scratch_op.kind = AsmOperand_REG;
            scratch_op.as.reg = scratch_reg;
            scratch_op.asm_type = asminstr->asm_type;

            i1.kind = AsmInstr_MOV;
            mov.src = asminstr->as.binary.lhs;
            mov.dst = scratch_op;
            i1.as.mov = mov;
            i1.asm_type = asminstr->asm_type;

            i2.kind = AsmInstr_BIN;
            bin.kind = asminstr->as.binary.kind;
            bin.lhs = scratch_op;
            bin.rhs = asminstr->as.binary.rhs;
            i2.as.binary = bin;
            i2.asm_type = asminstr->asm_type;

            vec_insert(&instrs, i1);
            vec_insert(&instrs, i2);

            break;
          } else {
            vec_insert(&instrs, *asminstr);
            break;
          }
          break;
        }
        default:
          vec_insert(&instrs, *asminstr);
          break;
      }
    }

    vec_free(&prog->funcs.data[i].body);
    prog->funcs.data[i].body = instrs;
  }

  return prog;
}

void emit_operand(FILE *f, struct AsmOperand *op)
{
  switch (op->kind) {
    case AsmOperand_INDEXED: {
      fprintf(f, "(%s, %s, %d)", reg_to_str_64(op->as.indexed.base),
              reg_to_str_64(op->as.indexed.index), op->as.indexed.scale);
      break;
    }
    case AsmOperand_MEMORY: {
      fprintf(f, "%d(", op->as.mem.offset);
      switch (op->as.mem.base) {
        case AX:
          fprintf(f, "%%rax");
          break;
        case BX:
          fprintf(f, "%%rbx");
          break;
        case DI:
          fprintf(f, "%%rdi");
          break;
        case SI:
          fprintf(f, "%%rsi");
          break;
        case DX:
          fprintf(f, "%%rdx");
          break;
        case CX:
          fprintf(f, "%%rcx");
          break;
        case R8:
          fprintf(f, "%%r8");
          break;
        case R9:
          fprintf(f, "%%r9");
          break;
        case R10:
          fprintf(f, "%%r10");
          break;
        case R11:
          fprintf(f, "%%r11");
          break;
        case R12:
          fprintf(f, "%%r12");
          break;
        case R13:
          fprintf(f, "%%r13");
          break;
        case R14:
          fprintf(f, "%%r14");
          break;
        case R15:
          fprintf(f, "%%r15");
          break;
        case BP:
          fprintf(f, "%%rbp");
          break;
        case SP:
          fprintf(f, "%%rsp");
          break;
        default:
          assert(0 && "Invalid memory base register");
      }
      fprintf(f, ")");
      break;
    }
    case AsmOperand_DATA: {
      fprintf(f, "%s(%%rip)", op->as.data);
      break;
    }
    case AsmOperand_IMM: {
      fprintf(f, "$%lld", op->as.imm);
      break;
    }
    case AsmOperand_PSEUDO: {
      assert(0 && "not implemented");
      break;
    }
    case AsmOperand_REG: {
      switch (op->as.reg) {
        case XMM0: {
          fprintf(f, "%%xmm0");
          break;
        }
        case XMM1: {
          fprintf(f, "%%xmm1");
          break;
        }
        case XMM2: {
          fprintf(f, "%%xmm2");
          break;
        }
        case XMM3: {
          fprintf(f, "%%xmm3");
          break;
        }
        case XMM4: {
          fprintf(f, "%%xmm4");
          break;
        }
        case XMM5: {
          fprintf(f, "%%xmm5");
          break;
        }
        case XMM6: {
          fprintf(f, "%%xmm6");
          break;
        }
        case XMM7: {
          fprintf(f, "%%xmm7");
          break;
        }
        case XMM8: {
          fprintf(f, "%%xmm8");
          break;
        }
        case XMM9: {
          fprintf(f, "%%xmm9");
          break;
        }
        case XMM10: {
          fprintf(f, "%%xmm10");
          break;
        }
        case XMM11: {
          fprintf(f, "%%xmm11");
          break;
        }
        case XMM12: {
          fprintf(f, "%%xmm12");
          break;
        }
        case XMM13: {
          fprintf(f, "%%xmm13");
          break;
        }
        case XMM14: {
          fprintf(f, "%%xmm14");
          break;
        }
        case XMM15: {
          fprintf(f, "%%xmm15");
          break;
        }

        case AX: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              fprintf(f, "%%al");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%ax");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%eax");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%rax");
              break;
            }
            default:
              assert(0);
          }

          break;
        }
        case BX: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              fprintf(f, "%%bl");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%bx");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%ebx");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%rbx");
              break;
            }
            default:
              assert(0);
          }

          break;
        }
        case DI: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              fprintf(f, "%%dil");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%di");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%edi");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%rdi");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case SI: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              fprintf(f, "%%sil");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%si");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%esi");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%rsi");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case DX: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              fprintf(f, "%%dl");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%dx");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%edx");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%rdx");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case CX: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              fprintf(f, "%%cl");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%cx");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%ecx");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%rcx");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case R8: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              fprintf(f, "%%r8b");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%r8w");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%r8d");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%r8");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case R9: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              fprintf(f, "%%r9b");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%r9w");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%r9d");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%r9");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case R10: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              fprintf(f, "%%r10b");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%r10w");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%r10d");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%r10");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case R11: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              fprintf(f, "%%r11b");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%r11w");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%r11d");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%r11");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case R12: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              fprintf(f, "%%r12b");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%r12w");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%r12d");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%r12");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case R13: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              fprintf(f, "%%r13b");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%r13w");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%r13d");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%r13");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case R14: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              fprintf(f, "%%r14b");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%r14w");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%r14d");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%r14");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case R15: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              fprintf(f, "%%r15b");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%r15w");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%r15d");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%r15");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case BP: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              fprintf(f, "%%bpl");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%bp");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%ebp");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%rbp");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case SP: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              fprintf(f, "%%spl");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%sp");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%esp");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%rsp");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
      }
      break;
    }
    case AsmOperand_STACK: {
      fprintf(f, "%d(%%rbp)", op->as.stack_offset);
      break;
    }
  }
}

void emit(struct AsmProgram *prog, char *path)
{
  FILE *f;

  f = fopen(path, "w");
  if (global_constants.len > 0) {
    fprintf(f, ".section .rodata\n");
    for (int i = 0; i < global_constants.len; i++) {
      fprintf(f, "%s:\n", global_constants.data[i].name);
      if (strncmp(global_constants.data[i].value, ".long", 5) == 0 ||
          strncmp(global_constants.data[i].value, ".quad", 5) == 0) {
        fprintf(f, "\t%s\n", global_constants.data[i].value);
      } else {
        fprintf(f, "\t.string \"%s\"\n", global_constants.data[i].value);
      }
    }
    fprintf(f, "\n");
  }

  fprintf(f, ".section .text\n");
  for (int i = 0; i < prog->funcs.len; i++) {
    fprintf(f, ".global %s\n", prog->funcs.data[i].name);
    fprintf(f, "%s:\n", prog->funcs.data[i].name);
    for (int j = 0; j < prog->funcs.data[i].body.len; j++) {
      struct AsmInstr *instr = &prog->funcs.data[i].body.data[j];
      switch (instr->kind) {
        case AsmInstr_LBL:
          break;
        default:
          fprintf(f, "\t");
      }
      switch (instr->kind) {
        case AsmInstr_PUSH: {
          fprintf(f, "pushq ");
          emit_operand(f, &instr->as.push.op);
          fprintf(f, "\n");
          break;
        }
        case AsmInstr_POP: {
          fprintf(f, "popq ");
          emit_operand(f, &instr->as.pop.op);
          fprintf(f, "\n");
          break;
        }
        case AsmInstr_MOV: {
          if (instr->asm_type.kind == AsmType_FLOAT) {
            fprintf(f, "movss ");
          } else if (instr->asm_type.kind == AsmType_DOUBLE) {
            fprintf(f, "movsd ");
          } else {
            fprintf(f, "mov");
            switch (instr->asm_type.kind) {
              case AsmType_BYTE:
                fprintf(f, "b ");
                break;
              case AsmType_WORD:
                fprintf(f, "w ");
                break;
              case AsmType_LONGWORD:
                fprintf(f, "l ");
                break;
              case AsmType_QUADWORD:
                fprintf(f, "q ");
                break;
              default:
                assert(0);
            }
          }

          emit_operand(f, &instr->as.mov.src);
          fprintf(f, ", ");
          emit_operand(f, &instr->as.mov.dst);
          fprintf(f, "\n");
          break;
        }
        case AsmInstr_BIN: {
          switch (instr->as.binary.kind) {
            case AsmInstrBinary_ADD: {
              if (instr->asm_type.kind == AsmType_FLOAT) {
                fprintf(f, "addss ");
              } else if (instr->asm_type.kind == AsmType_DOUBLE) {
                fprintf(f, "addsd ");
              } else {
                fprintf(f, "add");
                switch (instr->asm_type.kind) {
                  case AsmType_BYTE:
                    fprintf(f, "b ");
                    break;
                  case AsmType_WORD:
                    fprintf(f, "w ");
                    break;
                  case AsmType_LONGWORD:
                    fprintf(f, "l ");
                    break;
                  case AsmType_QUADWORD:
                    fprintf(f, "q ");
                    break;
                  default:
                    assert(0);
                }
              }
              break;
            }
            case AsmInstrBinary_SUB: {
              if (instr->asm_type.kind == AsmType_FLOAT) {
                fprintf(f, "subss ");
              } else if (instr->asm_type.kind == AsmType_DOUBLE) {
                fprintf(f, "subsd ");
              } else {
                fprintf(f, "sub");
                switch (instr->asm_type.kind) {
                  case AsmType_BYTE:
                    fprintf(f, "b ");
                    break;
                  case AsmType_WORD:
                    fprintf(f, "w ");
                    break;
                  case AsmType_LONGWORD:
                    fprintf(f, "l ");
                    break;
                  case AsmType_QUADWORD:
                    fprintf(f, "q ");
                    break;
                  default:
                    assert(0);
                }
              }
              break;
            }
            case AsmInstrBinary_MUL: {
              if (instr->asm_type.kind == AsmType_FLOAT) {
                fprintf(f, "mulss ");
              } else if (instr->asm_type.kind == AsmType_DOUBLE) {
                fprintf(f, "mulsd ");
              } else {
                fprintf(f, "imul ");
                switch (instr->asm_type.kind) {
                  case AsmType_BYTE:
                    fprintf(f, "b ");
                    break;
                  case AsmType_WORD:
                    fprintf(f, "w ");
                    break;
                  case AsmType_LONGWORD:
                    fprintf(f, "l ");
                    break;
                  case AsmType_QUADWORD:
                    fprintf(f, "q ");
                    break;
                  default:
                    assert(0);
                }
              }
              break;
            }
            case AsmInstrBinary_DIV: {
              if (instr->asm_type.kind == AsmType_FLOAT) {
                fprintf(f, "divss ");
              } else if (instr->asm_type.kind == AsmType_DOUBLE) {
                fprintf(f, "divsd ");
              } else {
                assert(0 && "integer div not implemented");
              }
              break;
            }
            case AsmInstrBinary_BIT_AND:
            case AsmInstrBinary_BIT_XOR:
            case AsmInstrBinary_BIT_OR:
            case AsmInstrBinary_SHL:
            case AsmInstrBinary_SHR:
            case AsmInstrBinary_SAR: {
              switch (instr->as.binary.kind) {
                case AsmInstrBinary_BIT_AND:
                  fprintf(f, "and");
                  break;
                case AsmInstrBinary_BIT_XOR:
                  fprintf(f, "xor");
                  break;
                case AsmInstrBinary_BIT_OR:
                  fprintf(f, "or");
                  break;
                case AsmInstrBinary_SHL:
                  fprintf(f, "sal");
                  break;
                case AsmInstrBinary_SHR:
                  fprintf(f, "shr");
                  break;
                case AsmInstrBinary_SAR:
                  fprintf(f, "sar");
                  break;
                default:
                  assert(0);
              }
              switch (instr->asm_type.kind) {
                case AsmType_BYTE:
                  fprintf(f, "b ");
                  break;
                case AsmType_WORD:
                  fprintf(f, "w ");
                  break;
                case AsmType_LONGWORD:
                  fprintf(f, "l ");
                  break;
                case AsmType_QUADWORD:
                  fprintf(f, "q ");
                  break;
                default:
                  assert(0 && "bitwise operators require integer asm types");
              }
              break;
            }
            default:
              assert(0 && "not implemented");
              break;
          }
          emit_operand(f, &instr->as.binary.lhs);
          fprintf(f, ", ");
          emit_operand(f, &instr->as.binary.rhs);
          fprintf(f, "\n");
          break;
        }
        case AsmInstr_RET: {
          fprintf(f, "ret\n");
          break;
        }
        case AsmInstr_CALL: {
          fprintf(f, "call %s\n", instr->as.call.target);
          break;
        }
        case AsmInstr_JMP: {
          fprintf(f, "jmp .L%s\n", instr->as.jmp.target);
          break;
        }
        case AsmInstr_LBL: {
          fprintf(f, ".L%s:\n", instr->as.lbl.name);
          break;
        }
        case AsmInstr_CMP: {
          char *i;

          switch (instr->as.cmp.asm_type.kind) {
            case AsmType_BYTE:
              i = "cmpb";
              break;
            case AsmType_WORD:
              i = "cmpw";
              break;
            case AsmType_LONGWORD:
              i = "cmpl";
              break;
            case AsmType_QUADWORD:
              i = "cmpq";
              break;
            case AsmType_FLOAT:
              i = "ucomiss";
              break;
            case AsmType_DOUBLE:
              i = "ucomisd";
              break;
            default:
              assert(0);
          }

          fprintf(f, "%s ", i);
          emit_operand(f, &instr->as.cmp.lhs);
          fprintf(f, ", ");
          emit_operand(f, &instr->as.cmp.rhs);
          fprintf(f, "\n");

          break;
        }
        case AsmInstr_JmpCC: {
          char *suffix;

          switch (instr->as.jmpcc.cc) {
            case CC_E:
              suffix = "e";
              break;
            case CC_NE:
              suffix = "ne";
              break;
            case CC_L:
              suffix = "l";
              break;
            case CC_LE:
              suffix = "le";
              break;
            case CC_G:
              suffix = "g";
              break;
            case CC_GE:
              suffix = "ge";
              break;
            case CC_A:
              suffix = "a";
              break;
            case CC_AE:
              suffix = "ae";
              break;
            case CC_B:
              suffix = "b";
              break;
            case CC_BE:
              suffix = "be";
              break;
          }

          fprintf(f, "j%s .L%s\n", suffix, instr->as.jmpcc.target);
          break;
        }
        case AsmInstr_SetCC: {
          char *suffix;

          switch (instr->as.setcc.cc) {
            case CC_E:
              suffix = "e";
              break;
            case CC_NE:
              suffix = "ne";
              break;
            case CC_L:
              suffix = "l";
              break;
            case CC_LE:
              suffix = "le";
              break;
            case CC_G:
              suffix = "g";
              break;
            case CC_GE:
              suffix = "ge";
              break;
            case CC_A:
              suffix = "a";
              break;
            case CC_AE:
              suffix = "ae";
              break;
            case CC_B:
              suffix = "b";
              break;
            case CC_BE:
              suffix = "be";
              break;
          }

          fprintf(f, "set%s ", suffix);
          emit_operand(f, &instr->as.setcc.op);
          fprintf(f, "\n");
          break;
        }
        case AsmInstr_LEA: {
          fprintf(f, "leaq ");
          emit_operand(f, &instr->as.lea.src);
          fprintf(f, ", ");
          emit_operand(f, &instr->as.lea.dst);
          fprintf(f, "\n");
          break;
        }
        case AsmInstr_UNARY: {
          fprintf(
              f, instr->as.unary.kind == AsmInstrUnary_BIT_NOT ? "not" : "neg");
          switch (instr->asm_type.kind) {
            case AsmType_BYTE:
              fprintf(f, "b");
              break;
            case AsmType_WORD:
              fprintf(f, "w");
              break;
            case AsmType_LONGWORD:
              fprintf(f, "l");
              break;
            case AsmType_QUADWORD:
              fprintf(f, "q");
              break;
            default:
              assert(0);
          }
          fprintf(f, " ");
          emit_operand(f, &instr->as.unary.op);
          fprintf(f, "\n");
          break;
        }
        case AsmInstr_CVT: {
          switch (instr->as.cvt.kind) {
            case AsmCast_SignExtend:
              if (instr->as.cvt.src.asm_type.kind == AsmType_BYTE) {
                if (instr->as.cvt.dst.asm_type.kind == AsmType_WORD) {
                  fprintf(f, "movsbw ");
                } else if (instr->as.cvt.dst.asm_type.kind ==
                           AsmType_LONGWORD) {
                  fprintf(f, "movsbl ");
                } else {
                  fprintf(f, "movsbq ");
                }
              } else if (instr->as.cvt.src.asm_type.kind == AsmType_WORD) {
                if (instr->as.cvt.dst.asm_type.kind == AsmType_LONGWORD) {
                  fprintf(f, "movswl ");
                } else {
                  fprintf(f, "movswq ");
                }
              } else if (instr->as.cvt.src.asm_type.kind == AsmType_LONGWORD) {
                fprintf(f, "movslq ");
              }
              emit_operand(f, &instr->as.cvt.src);
              fprintf(f, ", ");
              emit_operand(f, &instr->as.cvt.dst);
              fprintf(f, "\n");
              break;

            case AsmCast_ZeroExtend:
              if (instr->as.cvt.src.asm_type.kind == AsmType_BYTE) {
                if (instr->as.cvt.dst.asm_type.kind == AsmType_WORD) {
                  fprintf(f, "movzbw ");
                } else if (instr->as.cvt.dst.asm_type.kind ==
                           AsmType_LONGWORD) {
                  fprintf(f, "movzbl ");
                } else {
                  fprintf(f, "movzbq ");
                }
                emit_operand(f, &instr->as.cvt.src);
                fprintf(f, ", ");
                emit_operand(f, &instr->as.cvt.dst);
                fprintf(f, "\n");
              } else if (instr->as.cvt.src.asm_type.kind == AsmType_WORD) {
                if (instr->as.cvt.dst.asm_type.kind == AsmType_LONGWORD) {
                  fprintf(f, "movzwl ");
                } else {
                  fprintf(f, "movzwq ");
                }
                emit_operand(f, &instr->as.cvt.src);
                fprintf(f, ", ");
                emit_operand(f, &instr->as.cvt.dst);
                fprintf(f, "\n");
              } else if (instr->as.cvt.src.asm_type.kind == AsmType_LONGWORD) {
                /* Hardware automatically zero extends the upper 32 bits on
                 * 32-bit register writes */
                fprintf(f, "movl ");
                emit_operand(f, &instr->as.cvt.src);
                fprintf(f, ", ");
                struct AsmOperand narrowed_dst = instr->as.cvt.dst;
                narrowed_dst.asm_type =
                    (struct AsmType){.kind = AsmType_LONGWORD};
                emit_operand(f, &narrowed_dst);
                fprintf(f, "\n");
              }
              break;

            case AsmCast_FloatPromote:
              fprintf(f, "cvtss2sd ");
              emit_operand(f, &instr->as.cvt.src);
              fprintf(f, ", ");
              emit_operand(f, &instr->as.cvt.dst);
              fprintf(f, "\n");
              break;

            case AsmCast_FloatDemote:
              fprintf(f, "cvtsd2ss ");
              emit_operand(f, &instr->as.cvt.src);
              fprintf(f, ", ");
              emit_operand(f, &instr->as.cvt.dst);
              fprintf(f, "\n");
              break;

            case AsmCast_IntToFloat:
              if (instr->as.cvt.src.asm_type.kind == AsmType_QUADWORD) {
                fprintf(f, "cvtsi2ssq ");
              } else {
                fprintf(f, "cvtsi2ssl ");
              }
              emit_operand(f, &instr->as.cvt.src);
              fprintf(f, ", ");
              emit_operand(f, &instr->as.cvt.dst);
              fprintf(f, "\n");
              break;

            case AsmCast_IntToDouble:
              if (instr->as.cvt.src.asm_type.kind == AsmType_QUADWORD) {
                fprintf(f, "cvtsi2sdq ");
              } else {
                fprintf(f, "cvtsi2sdl ");
              }
              emit_operand(f, &instr->as.cvt.src);
              fprintf(f, ", ");
              emit_operand(f, &instr->as.cvt.dst);
              fprintf(f, "\n");
              break;

            case AsmCast_FloatToInt:
            case AsmCast_DoubleToInt:
              if (instr->as.cvt.kind == AsmCast_FloatToInt) {
                if (instr->as.cvt.dst.asm_type.kind == AsmType_QUADWORD) {
                  fprintf(f, "cvttss2siq ");
                } else {
                  fprintf(f, "cvttss2sil ");
                }
              } else {
                if (instr->as.cvt.dst.asm_type.kind == AsmType_QUADWORD) {
                  fprintf(f, "cvttsd2siq ");
                } else {
                  fprintf(f, "cvttsd2sil ");
                }
              }
              emit_operand(f, &instr->as.cvt.src);
              fprintf(f, ", ");
              emit_operand(f, &instr->as.cvt.dst);
              fprintf(f, "\n");
              break;

            default:
              assert(0 && "Unhandled cast type");
          }
          break;
        }
        case AsmInstr_REP_MOVSB: {
          fprintf(f, "rep movsb\n");
          break;
        }
        default:
          break;
      }
    }
  }

  fclose(f);
}

struct AssembleLinkResult {
  bool is_ok;
  char *msg;
};

struct AssembleLinkResult assemble_and_link(const char *path,
                                            const char *out_path,
                                            bool assemble_only)
{
  struct AssembleLinkResult result;
  result.is_ok = true;
  result.msg = NULL;

  pid_t pid = fork();

  if (pid < 0) {
    result.is_ok = false;
    result.msg = "Fork failed";
    return result;
  } else if (pid == 0) {
    if (assemble_only) {
      execlp("gcc", "gcc", "-c", path, "-o", out_path, NULL);
    } else {
      execlp("gcc", "gcc", path, "-o", out_path, NULL);
    }

    exit(EXIT_FAILURE);
  } else {
    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
      return result;
    } else {
      result.is_ok = false;
      result.msg = assemble_only
                       ? "gcc failed at the assemble stage\n"
                       : "gcc failed at the assemble-and-link stage\n";
      return result;
    }
  }
}

enum TargetStage {
  STAGE_FULL,
  STAGE_TOKENIZE,
  STAGE_PARSE,
  STAGE_RESOLVE,
  STAGE_TYPECHECK,
  STAGE_IR,
  STAGE_CODEGEN_RAW,
  STAGE_CODEGEN_REPLACE_PSEUDO,
  STAGE_CODEGEN_FIXUP,
  STAGE_EMIT,
  STAGE_ASM,
  STAGE_LINK,
};

struct CompilerOptions {
  enum TargetStage target_stage;
  char *path;
};

struct CompilerOptions parse_args(int argc, char **argv)
{
  struct CompilerOptions opts;
  opts.target_stage = STAGE_FULL;

  static struct option long_options[] = {
      {"tokenize", no_argument, 0, 't'},
      {"parse", no_argument, 0, 'p'},
      {"resolve", no_argument, 0, 'r'},
      {"typecheck", no_argument, 0, 'c'},
      {"ir", no_argument, 0, 'i'},
      {"codegen", required_argument, 0, 'g'},
      {"raw", no_argument, 0, 'R'},
      {"replace-pseudo", no_argument, 0, 'P'},
      {"fixup", no_argument, 0, 'F'},
      {"emit", no_argument, 0, 'e'},
      {"asm", no_argument, 0, 'a'},
      {"ln", no_argument, 0, 'l'},
      {0, 0, 0, 0}};

  int opt;
  int option_index = 0;

  while ((opt = getopt_long(argc, argv, "tprcig:", long_options,
                            &option_index)) != -1) {
    switch (opt) {
      case 't':
        opts.target_stage = STAGE_TOKENIZE;
        break;
      case 'p':
        opts.target_stage = STAGE_PARSE;
        break;
      case 'r':
        opts.target_stage = STAGE_RESOLVE;
        break;
      case 'c':
        opts.target_stage = STAGE_TYPECHECK;
        break;
      case 'i':
        opts.target_stage = STAGE_IR;
        break;
      case 'g':
        if (strcmp(optarg, "raw") == 0 || strcmp(optarg, "--raw") == 0) {
          opts.target_stage = STAGE_CODEGEN_RAW;
        } else if (strcmp(optarg, "replace-pseudo") == 0 ||
                   strcmp(optarg, "--replace-pseudo") == 0) {
          opts.target_stage = STAGE_CODEGEN_REPLACE_PSEUDO;
        } else if (strcmp(optarg, "fixup") == 0 ||
                   strcmp(optarg, "--fixup") == 0) {
          opts.target_stage = STAGE_CODEGEN_FIXUP;
        } else {
          fprintf(stderr, "Unknown codegen stage: %s\n", optarg);
          exit(1);
        }
        break;
      case 'R':
        opts.target_stage = STAGE_CODEGEN_RAW;
        break;
      case 'P':
        opts.target_stage = STAGE_CODEGEN_REPLACE_PSEUDO;
        break;
      case 'F':
        opts.target_stage = STAGE_CODEGEN_FIXUP;
        break;
      case 'e':
        opts.target_stage = STAGE_EMIT;
        break;
      case 'a':
        opts.target_stage = STAGE_ASM;
        break;
      case 'l':
        opts.target_stage = STAGE_LINK;
        break;
      case '?':
        exit(1);
      default:
        abort();
    }
  }

  if (optind < argc) {
    opts.path = argv[optind];
  }

  return opts;
}

char *replace_ext(char *path, char *ext)
{
  int a, b, total;
  char *replaced, *dot;

  a = strlen(path);
  b = strlen(ext);
  total = a + b + 1;

  replaced = malloc(total);

  dot = strrchr(path, '.') + 1;

  strncpy(replaced, path, dot - path);
  replaced[dot - path] = '\0';
  strcat(replaced, ext);

  return replaced;
}

char *strip_ext(char *path)
{
  int a, total;
  char *dot, *new;

  a = strlen(path);
  total = a + 1;

  dot = strrchr(path, '.');
  new = malloc(total);

  strncpy(new, path, dot - path);
  new[dot - path] = '\0';

  return new;
}

void free_labels(VecCharPtr *labels)
{
  for (int i = 0; i < labels->len; i++) {
    free(labels->data[i]);
  }
  vec_free(labels);
}

struct RunResult {
  bool is_ok;
  char *msg;
};

struct RunResult run(struct CompilerOptions *opts)
{
  enum TargetStage target_stage;
  char *path;
  struct ReadFileResult read_file_result;
  char *src;
  struct Tokenizer tokenizer;
  struct TokenizeResult tokenize_result;
  VecToken tokens;
  struct Parser parser;
  struct ParseResult parse_result;
  struct AST *ast, *resolved_ast, *typechecked_ast, *labeled_ast;
  struct ResolveResult resolve_result;
  struct TypecheckResult typecheck_result;
  struct LoopLabelResult loop_label_result;
  struct CollectLabelsResult collect_labels_result;
  struct LabelCheckResult label_check_result;
  struct IrfyResult irfy_result;
  struct IRProgram ir_prog;
  struct AsmResult asm_result;
  struct AsmProgram asm_prog;
  struct RunResult r;

  r.is_ok = true;
  r.msg = NULL;

  target_stage = opts->target_stage;
  path = opts->path;

  read_file_result = read_file(path);
  if (!read_file_result.is_ok) {
    r.msg = "Couldn't read file";
    r.is_ok = false;
    goto free_up2_fread;
  }

  src = read_file_result.contents;

  init_tokenizer(&tokenizer, src);
  tokenize_result = tokenize(&tokenizer);

  if (!tokenize_result.is_ok) {
    r.msg = tokenize_result.msg;
    r.is_ok = false;
    goto free_up2_tokenize;
  }

  tokens = tokenize_result.tokens;
  print_tokens(&tokens);

  if (target_stage == STAGE_TOKENIZE) {
    goto free_up2_tokenize;
  }

  init_parser(&parser, &tokens);
  parse_result = parse(&parser);

  if (!parse_result.is_ok) {
    r.msg = parse_result.msg;
    r.is_ok = false;
    goto free_up2_parse;
  }

  ast = parse_result.ast;
  print_ast(ast);

  if (target_stage == STAGE_PARSE) {
    goto free_up2_parse;
  }

  resolve_result = resolve(ast);
  if (!resolve_result.is_ok) {
    r.msg = resolve_result.msg;
    r.is_ok = false;
    goto free_up2_parse;
  }

  printf("resolved ast:\n");
  resolved_ast = resolve_result.as.ast;
  print_ast(resolved_ast);

  if (target_stage == STAGE_RESOLVE) {
    goto free_up2_parse;
  }

  typecheck_result = typecheck(resolved_ast);
  if (!typecheck_result.is_ok) {
    r.msg = typecheck_result.msg;
    r.is_ok = false;
    goto free_up2_parse;
  }

  typechecked_ast = typecheck_result.ast;

  printf("typechecked ast:\n");
  print_ast(typechecked_ast);

  if (target_stage == STAGE_TYPECHECK) {
    goto free_up2_parse;
  }

  loop_label_result = loop_label(typechecked_ast);
  if (!loop_label_result.is_ok) {
    r.msg = loop_label_result.msg;
    r.is_ok = false;
    goto free_up2_parse;
  }

  labeled_ast = loop_label_result.ast;

  printf("labeled ast:\n");
  print_ast(labeled_ast);

  collect_labels_result = collect_labels(labeled_ast);
  if (!collect_labels_result.is_ok) {
    r.msg = collect_labels_result.msg;
    r.is_ok = false;
    goto free_up2_collect_labels;
  }

  label_check_result = check_labels(labeled_ast, &collect_labels_result.labels);
  if (!label_check_result.is_ok) {
    r.msg = label_check_result.msg;
    r.is_ok = false;
    goto free_up2_collect_labels;
  }

  irfy_result = irfy_ast(labeled_ast);
  if (!irfy_result.is_ok) {
    r.msg = irfy_result.msg;
    r.is_ok = false;
    goto free_up2_irfy;
  }

  ir_prog = irfy_result.prog;
  print_ir(&ir_prog);

  if (target_stage == STAGE_IR) {
    goto free_up2_irfy;
  }

  asm_result = codegen(&ir_prog);
  if (!asm_result.is_ok) {
    r.msg = asm_result.msg;
    r.is_ok = false;
    goto free_up2_asm;
  }

  asm_prog = asm_result.prog;
  print_asm(&asm_prog);

  if (target_stage == STAGE_CODEGEN_RAW) {
    goto free_up2_asm;
  }

  printf("regalloc\n");
  asm_prog = *regalloc(&asm_prog);
  print_asm(&asm_prog);

  // printf("replacing pseudo...\n");
  // asm_prog = *replace_pseudo(&asm_prog);
  // print_asm(&asm_prog);

  if (target_stage == STAGE_CODEGEN_REPLACE_PSEUDO) {
    goto free_up2_asm;
  }

  printf("fixup...\n");
  asm_prog = *fixup(&asm_prog);
  print_asm(&asm_prog);

  if (target_stage == STAGE_CODEGEN_FIXUP) {
    goto free_up2_asm;
  }

  char *asm_path;
  asm_path = replace_ext(path, "s");

  emit(&asm_prog, asm_path);
  if (target_stage == STAGE_EMIT) {
    goto free_up2_asm;
  }

  char *o_path, *exe_path;
  o_path = replace_ext(path, "o");
  exe_path = strip_ext(path);

  if (target_stage == STAGE_ASM) {
    assemble_and_link(asm_path, o_path, true);
  } else if (target_stage == STAGE_LINK || target_stage == STAGE_FULL) {
    assemble_and_link(asm_path, exe_path, false);
  }

free_up2_asm:
  free_asm(&asm_result.prog);
  free(asm_path);
  free(o_path);
  free(exe_path);

free_up2_irfy:
  free_ir_prog(&irfy_result.prog);

free_up2_collect_labels:
  free_labels(&collect_labels_result.labels);

free_up2_parse:
  free_ast(parse_result.ast);

free_up2_tokenize:
  vec_free(&tokenize_result.tokens);

free_up2_fread:
  free(read_file_result.contents);

  free_global_constants();
  free_enum_types();
  free_enum_variants();

  return r;
}

int main(int argc, char **argv)
{
  struct CompilerOptions opts;
  struct RunResult r;

  opts = parse_args(argc, argv);
  r = run(&opts);
  if (!r.is_ok) {
    fprintf(stderr, "err: %s\n", r.msg);
    return 1;
  }

  return 0;
}
