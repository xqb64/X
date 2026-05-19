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

char *own_string_n(const char *string, int n)
{
  char *s;

  s = malloc(strlen(string) + 1);
  snprintf(s, n + 1, "%s", string);

  return s;
}

int mktmp(void)
{
  static int i = 0;
  return i++;
}

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

/* Generates an allocated string based on a format (like printf) */
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
  TOKEN_IF,
  TOKEN_ELSE,
  TOKEN_WHILE,
  TOKEN_BREAK,
  TOKEN_CONTINUE,
  TOKEN_RET,
  TOKEN_EXTERN,
  TOKEN_VOID,
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

  /* two chars */
  TOKEN_LESS_EQUAL,
  TOKEN_GREATER_EQUAL,
  TOKEN_EQUAL_EQUAL,
  TOKEN_BANG_EQUAL,
  TOKEN_PIPE_PIPE,
  TOKEN_AMPERSAND_AMPERSAND,
  TOKEN_ARROW,

  /* three chars */
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
        if (lookahead(tokenizer, 3, "lse") == 0) {
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
        if (lookahead(tokenizer, 2, "tr") == 0) {
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
        if (lookahead(tokenizer, 1, "8") == 0) {
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
        vec_insert(&tokens, mktoken(tokenizer, TOKEN_PLUS, 1));
        break;
      }
      case '-': {
        if (lookahead(tokenizer, 1, ">") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_ARROW, 2));
        } else {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_MINUS, 1));
        }
        break;
      }
      case '*': {
        vec_insert(&tokens, mktoken(tokenizer, TOKEN_STAR, 1));
        break;
      }
      case '/': {
        vec_insert(&tokens, mktoken(tokenizer, TOKEN_SLASH, 1));
        break;
      }
      case '.': {
        if (lookahead(tokenizer, 2, "..") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_ELLIPSIS, 3));
        } else {
          /* we do not have structs yet, so error for now */
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_ERROR, 1));
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
        } else {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_PIPE, 1));
        }
        break;
      }
      case '&': {
        if (lookahead(tokenizer, 1, "&") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_AMPERSAND_AMPERSAND, 2));
        } else {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_AMPERSAND, 1));
        }
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
        if (lookahead(tokenizer, 1, "=") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_GREATER_EQUAL, 2));
        } else {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_GREATER, 1));
        }
        break;
      }
      case '<': {
        if (lookahead(tokenizer, 1, "=") == 0) {
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
    case TOKEN_IF:
      printf("if");
      break;
    case TOKEN_ELSE:
      printf("else");
      break;
    case TOKEN_WHILE:
      printf("while");
      break;
    case TOKEN_BREAK:
      printf("break");
      break;
    case TOKEN_CONTINUE:
      printf("continue");
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
  } as;
};

Type clone_type(Type t)
{
  Type copy = t;
  if (t.kind == PTR_T) {
    copy.as.base = malloc(sizeof(Type));
    *copy.as.base = clone_type(*t.as.base);
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
  EXPR_CALL,
  EXPR_ADDROF,
  EXPR_DEREF,
  EXPR_CAST,
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

struct ExprCast {
  struct Expr *expr;
};

struct Expr {
  enum ExprKind kind;
  union {
    struct Literal literal;
    struct ExprBin binary;
    struct ExprVar var;
    struct ExprCall call;
    struct ExprAssign assign;
    struct ExprUnary unary;
    struct ExprAddrOf addrof;
    struct ExprDeref deref;
    struct ExprCast cast;
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
      print_indent(spaces);
      printf(")");
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
  }
}

void free_expr(struct Expr *expr)
{
  switch (expr->kind) {
    case EXPR_DEREF: {
      free_expr(expr->as.deref.expr);
      free(expr->as.deref.expr);
      break;
    }
    case EXPR_ADDROF: {
      free_expr(expr->as.addrof.expr);
      free(expr->as.addrof.expr);
      break;
    }
    case EXPR_CAST: {
      free_expr(expr->as.cast.expr);
      free(expr->as.cast.expr);
      break;
    }
    case EXPR_UNARY: {
      free_expr(expr->as.unary.expr);
      free(expr->as.unary.expr);
      free(expr->as.unary.op);
      break;
    }
    case EXPR_ASSIGN: {
      free_expr(expr->as.assign.lhs);
      free_expr(expr->as.assign.rhs);
      free(expr->as.assign.lhs);
      free(expr->as.assign.rhs);
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
    case EXPR_BINARY: {
      free_expr(expr->as.binary.lhs);
      free_expr(expr->as.binary.rhs);
      free(expr->as.binary.lhs);
      free(expr->as.binary.rhs);
      break;
    }
  }
}

struct Parameter {
  char *name;
  Type type;
};

typedef Vector(struct Stmt) VecStmt;
typedef Vector(struct Parameter) VecParam;

enum StmtKind {
  STMT_FN,
  STMT_LET,
  STMT_RET,
  STMT_IF,
  STMT_WHILE,
  STMT_BREAK,
  STMT_CONTINUE,
  STMT_BLOCK,
  STMT_EXTERN,
  STMT_EXPR,
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

struct StmtBreak {
  char *label;
};

struct StmtContinue {
  char *label;
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

struct Stmt {
  enum StmtKind kind;
  union {
    struct StmtFn fn;
    struct StmtLet let;
    struct StmtRet ret;
    struct StmtIf if_stmt;
    struct StmtWhile while_stmt;
    struct StmtBreak break_stmt;
    struct StmtContinue continue_stmt;
    struct StmtBlock block;
    struct StmtExtern extern_stmt;
    struct StmtExpr expr_stmt;
  } as;
};

void print_stmt(struct Stmt *stmt, int spaces)
{
  switch (stmt->kind) {
    case STMT_BREAK: {
      print_indent(spaces);
      printf("STMT_BREAK(...)\n");
      break;
    }
    case STMT_CONTINUE: {
      print_indent(spaces);
      printf("STMT_CONTINUE(...)\n");
      break;
    }
    case STMT_WHILE: {
      print_indent(spaces);
      printf("STMT_WHILE(\n");

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
    case STMT_FN: {
      print_indent(spaces);
      printf("STMT_FN(\n");

      print_indent(spaces + 2);
      printf("name = %s,\n", stmt->as.fn.name);

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
    case STMT_EXTERN: {
      print_indent(spaces);
      printf("STMT_EXTERN(...)");
      break;
    }
    default:
      assert(0);
  }
}

void free_stmt(struct Stmt *stmt)
{
  switch (stmt->kind) {
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
    literal.as.str = own_string_n(token_literal->start, token_literal->len);
    literal.type = (Type){.kind = STR_T};

    res.as.expr = (struct Expr){.kind = EXPR_LITERAL, .as.literal = literal};
  } else if (check(parser, TOKEN_IDENTIFIER)) {
    struct Token *token_id;

    token_id = consume(parser, TOKEN_IDENTIFIER);
    if (!token_id) {
      return (struct ParseFnResult){
          .is_ok = false, .as.expr = {0}, .msg = "Expected identifier"};
    }

    struct ExprVar var;

    var.name = own_string_n(token_id->start, token_id->len);
    var.type = (Type){.kind = UNKNOWN_T};

    res.as.expr = (struct Expr){.kind = EXPR_VARIABLE, .as.var = var};
  } else if (check(parser, TOKEN_TRUE) || check(parser, TOKEN_FALSE)) {
    struct Literal literal;
    struct Token *token_literal;
    bool is_true;

    is_true = check(parser, TOKEN_TRUE);

    token_literal = consume(parser, is_true ? TOKEN_TRUE : TOKEN_FALSE);
    if (!token_literal) {
      return (struct ParseFnResult){
          .is_ok = false, .msg = "Expected bool literal", .as.expr = {0}};
    }

    literal.kind = LITERAL_BOOL;
    literal.as.boolean = is_true ? true : false;
    literal.type = (Type){.kind = BOOL_T};

    res.as.expr = (struct Expr){
        .kind = EXPR_LITERAL, .as.literal = literal, .type = literal.type};
  } else {
    res.is_ok = false;
    res.msg = "Expected number, string, or identifier";
    return res;
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

struct ParseFnResult call(struct Parser *parser)
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
    } else {
      break;
    }
  }

  return (struct ParseFnResult){.as.expr = expr, .is_ok = true, .msg = NULL};
}

struct ParseFnResult unary(struct Parser *parser)
{
  if (match(parser, 4, TOKEN_MINUS, TOKEN_BANG, TOKEN_AMPERSAND, TOKEN_STAR)) {
    char *op = own_string_n(parser->prev->start, parser->prev->len);

    if (strncmp(op, "-", 1) == 0 || strncmp(op, "!", 1) == 0) {
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
  }

  return call(parser);
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

struct ParseFnResult comparison(struct Parser *parser)
{
  struct ParseFnResult left_res, right_res;
  struct Expr left, right;

  left_res = term(parser);
  if (!left_res.is_ok) {
    return left_res;
  }

  left = left_res.as.expr;
  while (match(parser, 6, TOKEN_LESS, TOKEN_LESS_EQUAL, TOKEN_GREATER,
               TOKEN_GREATER_EQUAL, TOKEN_EQUAL_EQUAL, TOKEN_BANG_EQUAL)) {
    char *op = parser->prev->start;

    right_res = term(parser);
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

struct ParseFnResult logical_and(struct Parser *parser)
{
  struct ParseFnResult left_res, right_res;
  struct Expr left, right;

  left_res = comparison(parser);
  if (!left_res.is_ok) {
    return left_res;
  }

  left = left_res.as.expr;
  while (match(parser, 1, TOKEN_AMPERSAND_AMPERSAND)) {
    right_res = comparison(parser);
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

  if (match(parser, 1, TOKEN_EQUAL)) {
    char *op = parser->prev->start;

    (void) op;

    right_result = assignment(parser);
    if (!right_result.is_ok) {
      free_expr(&expr);
      return right_result;
    }

    right = right_result.as.expr;

    struct ExprAssign assignexp = {.lhs = ALLOC(expr), .rhs = ALLOC(right)};
    expr = (struct Expr){.kind = EXPR_ASSIGN, .as.assign = assignexp};
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
  }
  return (Type){.kind = UNKNOWN_T};
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

      name = own_string_n(name_token->start, name_token->len);
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
  stmt_fn.name = own_string_n(token_id->start, token_id->len);
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

  token_id = consume(parser, TOKEN_IDENTIFIER);
  if (!token_id) {
    return (struct ParseFnResult){.is_ok = false,
                                  .as.stmt = {0},
                                  .msg = "Expected identifier after 'let'"};
  }

  char *name = own_string_n(token_id->start, token_id->len);

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

      name = own_string_n(name_token->start, name_token->len);
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
  extern_stmt.name =
      own_string_n(token_identifier->start, token_identifier->len);
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

struct ParseFnResult parse_stmt(struct Parser *parser)
{
  struct ParseFnResult result;

  result.is_ok = true;
  result.msg = NULL;

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

  result.ast = malloc(sizeof(VecStmt));
  result.ast->stmts = (VecStmt){0};

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
  clone->type = v->type;

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
    default:
      assert(0);
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

struct IRInstr_Cast {
  struct IRValue *src;
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
      assert(0);
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
      switch (expr->as.binary.kind) {
        case EXPR_BIN_ADD:
          kind = IRInstrBinary_ADD;
          break;
        case EXPR_BIN_SUB:
          kind = IRInstrBinary_SUB;
          break;
        case EXPR_BIN_MUL:
          kind = IRInstrBinary_MUL;
          break;
        case EXPR_BIN_DIV:
          kind = IRInstrBinary_DIV;
          break;
        case EXPR_BIN_LESS:
          kind = IRInstrBinary_L;
          break;
        case EXPR_BIN_LESS_EQUAL:
          kind = IRInstrBinary_LE;
          break;
        case EXPR_BIN_GREATER:
          kind = IRInstrBinary_G;
          break;
        case EXPR_BIN_GREATER_EQUAL:
          kind = IRInstrBinary_GE;
          break;
        case EXPR_BIN_EQUAL_EQUAL:
          kind = IRInstrBinary_E;
          break;
        case EXPR_BIN_BANG_EQUAL:
          kind = IRInstrBinary_NE;
          break;
        default:
          assert(0);
      }

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
      } else {
        assert(0 && "Unhandled left-hand side in assignment");
      }

      break;
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
          assert(0);
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
      /* When we evaluate a type cast (e.g., `x as i32`), we are creating a
       * new value of the target type based on the src.  */
      struct IRValue *src, *dst;

      /* 1. Lvalue-to-Rvalue Conversion: We must evaluate the inner expression
       *    using `irfy_expr_and_convert`. If `x` is a variable in memory, this
       *    forces a LOAD instruction to fetch its actual data. We cannot cast
       *    an abstract memory location; we must cast the data itself.  */
      src = irfy_expr_and_convert(instrs, expr->as.cast.expr);

      /* 2. Temporary Generation: A cast does not mutate the original variable.
       *    Therefore, we generate a fresh temporary IR variable (`dst`) and set
       *    its type to the new, casted type.  */
      dst = mkirvar();
      dst->type = expr->type;

      /* 3. IR Emission: We explicitly emit an `IRInstr_CAST`. This acts as a
       *    directive for the backend, which will later decide if this requires
       *    sign-extension (e.g., `movsbl`), floating-point conversion
       *    (e.g., `cvtsi2sd`), or just a simple register move.  */
      struct IRInstr i;
      i.kind = IRInstr_CAST;
      i.as.cast.src = src;
      i.as.cast.dst = clone_irval(dst);
      vec_insert(instrs, i);

      /* 4. Rvalue Return: The result of a cast is always an rvalue (you cannot
       *    assign to a cast, like `(int)x = 5;`). Thus, we package our new
       *    temporary variable into an `EXPRESULT_PLAIN` and return it.  */
      return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = dst};
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
      assert(0);
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
      i.as.jmp.target = label;

      vec_insert(instrs, i);
      break;
    }
    case STMT_EXTERN:
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
      if (!resolved_name) {
        return (struct ResolveResult){.is_ok = false,
                                      .msg = "Undefined variable"};
      }

      free(expr->as.var.name);
      expr->as.var.name = strdup(resolved_name);
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
    case STMT_EXTERN: {
      char *cpy;

      cpy = strdup(stmt->as.extern_stmt.name);
      varmap_insert(varmap, stmt->as.extern_stmt.name, cpy);
      break;
    }
    case STMT_BREAK:
    case STMT_CONTINUE:
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
};

void free_symbol(struct Symbol *sym)
{
  free_type(&sym->type);
  free(sym);
}

void sym_insert(struct Symbol **sym, char *name, Type type)
{
  struct Symbol *node;

  node = malloc(sizeof(struct Symbol));

  node->name = name;
  node->type = type;
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
    default:
      assert(0);
  }
}

#define IN_RANGE(val, min, max) ((val) >= (min) && (val) <= (max))

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

static inline int get_type_size(enum TypeKind kind)
{
  switch (kind) {
    case I8_T:
    case U8_T:
    case BOOL_T:
      return 1;
    case I16_T:
    case U16_T:
      return 2;
    case I32_T:
    case U32_T:
      return 4;
    case I64_T:
    case U64_T:
      return 8;
    default:
      return -1;
  }
}

Type get_common_type(struct Expr *lhs, struct Expr *rhs)
{
  Type t1 = lhs->type;
  Type t2 = rhs->type;

  if (t1.kind == t2.kind) {
    return t1;
  }

  int size1 = get_type_size(t1.kind);
  int size2 = get_type_size(t2.kind);

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

struct TypecheckResult typecheck_expr(struct Expr *expr,
                                      struct Symbol *sym_table)
{
  struct TypecheckResult res = {.is_ok = true, .msg = NULL, .ast = NULL};

  switch (expr->kind) {
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
      struct TypecheckResult r;

      r = typecheck_expr(expr->as.cast.expr, sym_table);
      if (!r.is_ok) {
        return r;
      }

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
      }

      break;
    }
    case EXPR_VARIABLE: {
      struct Symbol *sym = sym_get(sym_table, expr->as.var.name);

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

      expr->type = sym->type;

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
          expr->as.assign.lhs->kind != EXPR_DEREF) {
        return (struct TypecheckResult){
            .is_ok = false,
            .msg = "Invalid assignment target: left side must be a variable",
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
                (expr->as.call.arguments.data[i].kind == EXPR_LITERAL &&
                 expr->as.call.arguments.data[i].as.literal.kind ==
                     LITERAL_NUM);
            bool is_unary_literal =
                (expr->as.call.arguments.data[i].kind == EXPR_UNARY &&
                 expr->as.call.arguments.data[i].as.unary.expr->kind ==
                     EXPR_LITERAL &&
                 expr->as.call.arguments.data[i]
                         .as.unary.expr->as.literal.kind == LITERAL_NUM);

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
              cast_expr.type = expected_type;
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

        expr->type = *callee_sym->type.as.func.retval;
      }

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

      sym_insert(sym_table, stmt->as.extern_stmt.name, t);
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

        sym_insert(sym_table, stmt->as.fn.name, clone_type(t));
      }

      struct Symbol *fn_sym_table = sym_table ? *sym_table : NULL;
      struct Symbol *outer_sym = fn_sym_table;

      for (int i = 0; i < stmt->as.fn.params.len; i++) {
        sym_insert(&fn_sym_table, stmt->as.fn.params.data[i].name,
                   stmt->as.fn.params.data[i].type);
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
      sym_insert(sym_table, stmt->as.let.name, clone_type(stmt->as.let.type));
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
          } else {
            struct Expr *inner = malloc(sizeof(struct Expr));
            *inner = *stmt->as.ret.val;

            stmt->as.ret.val->kind = EXPR_CAST;
            stmt->as.ret.val->type = expected_type;
            stmt->as.ret.val->as.cast.expr = inner;
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
    case STMT_BREAK:
    case STMT_CONTINUE:
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
};

enum AsmRegister {
  AX,
  DI,
  SI,
  CX,
  DX,
  R8,
  R9,
  R10,
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

enum AsmType {
  AsmType_BYTE,
  AsmType_WORD,
  AsmType_LONGWORD,
  AsmType_QUADWORD,
  AsmType_FLOAT,
  AsmType_DOUBLE,
};

void print_asm_type(enum AsmType type)
{
  switch (type) {
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

enum AsmType type_to_asm_type(Type type)
{
  switch (type.kind) {
    case U8_T:
    case I8_T:
    case BOOL_T:
      return AsmType_BYTE;
    case U16_T:
    case I16_T:
      return AsmType_WORD;
    case U32_T:
    case I32_T:
      return AsmType_LONGWORD;
    case U64_T:
    case I64_T:
    case PTR_T:
      return AsmType_QUADWORD;
    case F32_T:
      return AsmType_FLOAT;
    case F64_T:
      return AsmType_DOUBLE;
    default:
      return AsmType_QUADWORD;
  }
}

struct AsmOperand {
  enum AsmOperandKind kind;
  enum AsmType asm_type;
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
  } as;
};

void print_asm_operand(struct AsmOperand *op)
{
  switch (op->kind) {
    case AsmOperand_MEMORY: {
      printf("AsmOperand_MEMORY(offset = %d, base = ", op->as.mem.offset);
      switch (op->as.mem.base) {
        case AX:
          printf("%%rax");
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
          switch (op->asm_type) {
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
        case DI: {
          switch (op->asm_type) {
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
          switch (op->asm_type) {
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
          switch (op->asm_type) {
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
          switch (op->asm_type) {
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
          switch (op->asm_type) {
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
          switch (op->asm_type) {
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
          switch (op->asm_type) {
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
        case BP: {
          switch (op->asm_type) {
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
          switch (op->asm_type) {
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
  E,  /* equal */
  NE, /* not equal */
  L,  /* less, */
  LE, /* less or equal */
  G,  /* greater */
  GE, /* greater or equal */

  /* unsigned */
  A,  /* above */
  AE, /* above or equal */
  B,  /* below */
  BE, /* below or equal */
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
  enum AsmType asm_type;
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
};

struct AsmInstrUnary {
  enum AsmInstrUnaryKind kind;
  struct AsmOperand op;
};

struct AsmInstrCvt {
  bool is_unsigned;
  struct AsmOperand src;
  struct AsmOperand dst;
};

struct AsmInstr {
  enum AsmInstrKind kind;
  enum AsmType asm_type;
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
    case E:
      printf("E");
      break;
    case NE:
      printf("NE");
      break;
    case L:
      printf("L");
      break;
    case LE:
      printf("LE");
      break;
    case G:
      printf("G");
      break;
    case GE:
      printf("GE");
      break;
    case A:
      printf("A");
      break;
    case AE:
      printf("AE");
      break;
    case B:
      printf("B");
      break;
    case BE:
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

void codegen_instr(struct IRInstr *ir_instr, VecAsmInstr *instrs)
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
              cc = E;
              break;
            case IRInstrBinary_NE:
              cc = NE;
              break;
            case IRInstrBinary_L:
              cc = L;
              break;
            case IRInstrBinary_G:
              cc = G;
              break;
            case IRInstrBinary_LE:
              cc = LE;
              break;
            case IRInstrBinary_GE:
              cc = GE;
              break;
            default:
              assert(0 && "Unreachable or unhandled signed condition");
          }
        } else {
          switch (ir_instr->as.binary.kind) {
            case IRInstrBinary_E:
              cc = E;
              break;
            case IRInstrBinary_NE:
              cc = NE;
              break;
            case IRInstrBinary_L:
              cc = B;
              break;
            case IRInstrBinary_G:
              cc = A;
              break;
            case IRInstrBinary_LE:
              cc = BE;
              break;
            case IRInstrBinary_GE:
              cc = AE;
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
        i3.as.setcc.cc = E;
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
      struct AsmOperand retval;
      struct AsmInstr i1 = {0}, e1 = {0}, e2 = {0}, i2 = {0};

      ret.__dummy = 0;

      if (ir_instr->as.ret.val) {
        retval = codegen_irvalue(ir_instr->as.ret.val);
      } else {
        retval = (struct AsmOperand){.kind = AsmOperand_IMM, .as.imm = 0};
      }

      bool is_ret_float = (retval.asm_type == AsmType_FLOAT ||
                           retval.asm_type == AsmType_DOUBLE);

      i1.kind = AsmInstr_MOV;
      i1.asm_type = retval.asm_type;
      i1.as.mov.src = retval;
      i1.as.mov.dst = (struct AsmOperand){.kind = AsmOperand_REG,
                                          .as.reg = is_ret_float ? XMM0 : AX,
                                          .asm_type = retval.asm_type};

      mov.src = (struct AsmOperand){
          .kind = AsmOperand_REG, .as.reg = BP, .asm_type = AsmType_QUADWORD};
      mov.dst = (struct AsmOperand){
          .kind = AsmOperand_REG, .as.reg = SP, .asm_type = AsmType_QUADWORD};

      pop.op = (struct AsmOperand){
          .kind = AsmOperand_REG, .as.reg = BP, .asm_type = AsmType_QUADWORD};

      e1.kind = AsmInstr_MOV;
      e1.as.mov = mov;
      e1.asm_type = AsmType_QUADWORD;

      e2.kind = AsmInstr_POP;
      e2.as.pop = pop;

      i2.kind = AsmInstr_RET;
      i2.as.ret = ret;

      vec_insert(instrs, i1);
      vec_insert(instrs, e1);
      vec_insert(instrs, e2);
      vec_insert(instrs, i2);

      break;
    }
    case IRInstr_CPY: {
      struct AsmOperand src, dst;

      src = codegen_irvalue(ir_instr->as.copy.src);
      dst = codegen_irvalue(ir_instr->as.copy.dst);

      struct AsmInstr i = {0};
      struct AsmInstrMov mov;

      mov.src = src;
      mov.dst = dst;

      i.asm_type = src.asm_type;
      i.kind = AsmInstr_MOV;
      i.as.mov = mov;

      vec_insert(instrs, i);
      break;
    }
    case IRInstr_CALL: {
      enum AsmRegister int_arg_regs[] = {DI, SI, DX, CX, R8, R9};
      enum AsmRegister xmm_arg_regs[] = {XMM0, XMM1, XMM2, XMM3,
                                         XMM4, XMM5, XMM6, XMM7};

      int num_args = ir_instr->as.call.args.len;
      int int_reg_idx = 0;
      int xmm_reg_idx = 0;
      int num_stack_args = 0;

      struct AsmOperand *arg_dsts =
          malloc(sizeof(struct AsmOperand) * num_args);

      for (int i = 0; i < num_args; i++) {
        struct IRValue *arg_val = ir_instr->as.call.args.data[i];
        bool is_float =
            (arg_val->type.kind == F32_T || arg_val->type.kind == F64_T);

        if (is_float) {
          if (xmm_reg_idx < 8) {
            arg_dsts[i].kind = AsmOperand_REG;
            arg_dsts[i].as.reg = xmm_arg_regs[xmm_reg_idx++];
            arg_dsts[i].asm_type = type_to_asm_type(arg_val->type);
          } else {
            arg_dsts[i].kind = AsmOperand_STACK;
            num_stack_args++;
          }
        } else {
          if (int_reg_idx < 6) {
            arg_dsts[i].kind = AsmOperand_REG;
            arg_dsts[i].as.reg = int_arg_regs[int_reg_idx++];
            arg_dsts[i].asm_type = type_to_asm_type(arg_val->type);
          } else {
            arg_dsts[i].kind = AsmOperand_STACK;
            num_stack_args++;
          }
        }
      }

      /* System V AMD64 ABI requires that the stack be aligned to 16 bytes
       * before the call instruction.
       *
       * In case we pushed an odd number of stack arguments, the stack will be
       * misaligned by 8 bytes, since each argument on the stack (`pushq
       * arg7`) occupies 8 bytes.  The stack will NOT be misaligned if the
       * number of pushed arguments is even.  */
      int stack_padding = (num_stack_args % 2 != 0) ? 8 : 0;

      if (stack_padding != 0) {
        struct AsmInstr padding_instr = {0};

        padding_instr.kind = AsmInstr_BIN;
        padding_instr.asm_type = AsmType_QUADWORD;

        padding_instr.as.binary.kind = AsmInstrBinary_SUB;
        padding_instr.as.binary.lhs = (struct AsmOperand){
            .kind = AsmOperand_IMM, .as.imm = stack_padding};
        padding_instr.as.binary.rhs = (struct AsmOperand){
            .kind = AsmOperand_REG, .as.reg = SP, .asm_type = AsmType_QUADWORD};

        vec_insert(instrs, padding_instr);
      }

      /* Move the register arguments */
      for (int i = 0; i < num_args; i++) {
        if (arg_dsts[i].kind == AsmOperand_REG) {
          struct AsmOperand arg_op =
              codegen_irvalue(ir_instr->as.call.args.data[i]);
          struct AsmInstr mov_instr = {0};
          mov_instr.kind = AsmInstr_MOV;
          mov_instr.as.mov.src = arg_op;
          mov_instr.as.mov.dst = arg_dsts[i];
          mov_instr.asm_type = arg_op.asm_type;

          vec_insert(instrs, mov_instr);
        }
      }

      /* ...the rest of the arguments goes on the stack, in reverse order. */
      for (int i = num_args - 1; i >= 6; i--) {
        struct AsmOperand arg_op =
            codegen_irvalue(ir_instr->as.call.args.data[i]);
        struct AsmInstr push_instr = {0};
        push_instr.kind = AsmInstr_PUSH;
        push_instr.as.push.op = arg_op;
        vec_insert(instrs, push_instr);
      }

      /* Set %eax to the number of XMM registers used (needed for variadic
       * functions) */
      struct AsmInstr eax_instr = {0};
      eax_instr.kind = AsmInstr_MOV;
      eax_instr.asm_type = AsmType_LONGWORD;
      eax_instr.as.mov.src =
          (struct AsmOperand){.kind = AsmOperand_IMM, .as.imm = xmm_reg_idx};
      eax_instr.as.mov.dst = (struct AsmOperand){
          .kind = AsmOperand_REG, .as.reg = AX, .asm_type = AsmType_LONGWORD};
      vec_insert(instrs, eax_instr);

      struct AsmInstr call_instr = {0};
      call_instr.kind = AsmInstr_CALL;

      call_instr.as.call.target = ir_instr->as.call.target.as.var.name;
      vec_insert(instrs, call_instr);

      /* The caller is responsible for cleaning up the stack after the call
       * instruction. Since the stack on x86 grows downward, we need to ADD
       * (not SUB).  */
      int bytes_to_remove = (num_stack_args * 8) + stack_padding;
      if (bytes_to_remove != 0) {
        struct AsmInstr cleanup_instr = {0};
        cleanup_instr.kind = AsmInstr_BIN;
        cleanup_instr.asm_type = AsmType_QUADWORD;

        cleanup_instr.as.binary.kind = AsmInstrBinary_ADD;
        cleanup_instr.as.binary.lhs = (struct AsmOperand){
            .kind = AsmOperand_IMM, .as.imm = bytes_to_remove};
        cleanup_instr.as.binary.rhs = (struct AsmOperand){
            .kind = AsmOperand_REG, .as.reg = SP, .asm_type = AsmType_QUADWORD};
        vec_insert(instrs, cleanup_instr);
      }

      /* The caller is responsible for moving the return value from AX to a
       * safe destination if it wants to keep it, because AX is easily
       * clobbered.  */
      if (ir_instr->as.call.dst) {
        struct AsmOperand dst_op = codegen_irvalue(ir_instr->as.call.dst);
        struct AsmInstr mov_instr = {0};

        bool is_dst_float = (dst_op.asm_type == AsmType_FLOAT ||
                             dst_op.asm_type == AsmType_DOUBLE);

        mov_instr.kind = AsmInstr_MOV;
        mov_instr.as.mov.src =
            (struct AsmOperand){.kind = AsmOperand_REG,
                                .as.reg = is_dst_float ? XMM0 : AX,
                                .asm_type = dst_op.asm_type};
        mov_instr.as.mov.dst = dst_op;
        mov_instr.asm_type = dst_op.asm_type;
        vec_insert(instrs, mov_instr);
      }

      free(arg_dsts);
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
      i2.as.jmpcc.cc = E;
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
      i.asm_type = AsmType_QUADWORD;
      i.as.lea.src = codegen_irvalue(ir_instr->as.getaddr.src);
      i.as.lea.dst = codegen_irvalue(ir_instr->as.getaddr.dst);
      vec_insert(instrs, i);
      break;
    }
    case IRInstr_LOAD: {
      struct AsmOperand src_ptr, dst_val, scratch_ptr;

      src_ptr = codegen_irvalue(ir_instr->as.load.src);
      dst_val = codegen_irvalue(ir_instr->as.load.dst);

      scratch_ptr = (struct AsmOperand){
          .kind = AsmOperand_REG, .as.reg = R10, .asm_type = AsmType_QUADWORD};
      struct AsmInstr i1 = {0};
      i1.kind = AsmInstr_MOV;
      i1.as.mov.src = src_ptr;
      i1.as.mov.dst = scratch_ptr;
      i1.asm_type = AsmType_QUADWORD;

      struct AsmOperand indirect_op = {.kind = AsmOperand_MEMORY,
                                       .as.mem = {.base = R10, .offset = 0},
                                       .asm_type = dst_val.asm_type};
      struct AsmInstr i2 = {0};
      i2.kind = AsmInstr_MOV;
      i2.as.mov.src = indirect_op;
      i2.as.mov.dst = dst_val;
      i2.asm_type = dst_val.asm_type;

      vec_insert(instrs, i1);
      vec_insert(instrs, i2);
      break;
    }
    case IRInstr_STORE: {
      struct AsmOperand src_val, dst_ptr, scratch_val, scratch_ptr;
      bool is_float;
      enum AsmRegister val_reg;

      src_val = codegen_irvalue(ir_instr->as.store.val);
      dst_ptr = codegen_irvalue(ir_instr->as.store.dst);

      is_float = (src_val.asm_type == AsmType_FLOAT ||
                  src_val.asm_type == AsmType_DOUBLE);
      val_reg = is_float ? XMM8 : R9;

      scratch_val = (struct AsmOperand){.kind = AsmOperand_REG,
                                        .as.reg = val_reg,
                                        .asm_type = src_val.asm_type};
      struct AsmInstr i1 = {0};
      i1.kind = AsmInstr_MOV;
      i1.as.mov.src = src_val;
      i1.as.mov.dst = scratch_val;
      i1.asm_type = src_val.asm_type;

      scratch_ptr = (struct AsmOperand){
          .kind = AsmOperand_REG, .as.reg = R10, .asm_type = AsmType_QUADWORD};
      struct AsmInstr i2 = {0};
      i2.kind = AsmInstr_MOV;
      i2.as.mov.src = dst_ptr;
      i2.as.mov.dst = scratch_ptr;
      i2.asm_type = AsmType_QUADWORD;

      struct AsmOperand indirect_op = {.kind = AsmOperand_MEMORY,
                                       .as.mem = {.base = R10, .offset = 0},
                                       .asm_type = src_val.asm_type};
      struct AsmInstr i3 = {0};
      i3.kind = AsmInstr_MOV;
      i3.as.mov.src = scratch_val;
      i3.as.mov.dst = indirect_op;
      i3.asm_type = src_val.asm_type;

      vec_insert(instrs, i1);
      vec_insert(instrs, i2);
      vec_insert(instrs, i3);
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
      } else if (src.asm_type == dst.asm_type) {
        struct AsmInstr i = {0};
        i.kind = AsmInstr_MOV;
        i.as.mov.src = src;
        i.as.mov.dst = dst;
        i.asm_type = dst.asm_type;

        vec_insert(instrs, i);
      } else {
        struct AsmInstr i = {0};
        i.kind = AsmInstr_CVT;
        i.as.cvt.is_unsigned = is_unsigned(ir_instr->as.cast.src->type.kind) ||
                               ir_instr->as.cast.src->type.kind == BOOL_T;
        i.as.cvt.src = src;
        i.as.cvt.dst = dst;
        i.asm_type = src.asm_type;

        vec_insert(instrs, i);
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
      .kind = AsmOperand_REG, .as.reg = BP, .asm_type = AsmType_QUADWORD};
  p1.kind = AsmInstr_PUSH;
  p1.as.push = push;
  /*  ...and place ours SP into BP.  */
  mov.src = (struct AsmOperand){
      .kind = AsmOperand_REG, .as.reg = SP, .asm_type = AsmType_QUADWORD};
  mov.dst = (struct AsmOperand){
      .kind = AsmOperand_REG, .as.reg = BP, .asm_type = AsmType_QUADWORD};
  p2.kind = AsmInstr_MOV;
  p2.as.mov = mov;
  p2.asm_type = AsmType_QUADWORD;

  /* We will need to reserve the stack space for our local variables.
   * NOTE: 0 for now is a placeholder that is patched up later on.  */
  sub.kind = AsmInstrBinary_SUB;
  sub.lhs = (struct AsmOperand){.kind = AsmOperand_IMM, .as.imm = 0};
  sub.rhs = (struct AsmOperand){
      .kind = AsmOperand_REG, .as.reg = SP, .asm_type = AsmType_QUADWORD};

  p3.kind = AsmInstr_BIN;
  p3.as.binary = sub;
  p3.asm_type = AsmType_QUADWORD;

  vec_insert(&func.body, p1);
  vec_insert(&func.body, p2);
  vec_insert(&func.body, p3);

  enum AsmRegister int_arg_regs[] = {DI, SI, DX, CX, R8, R9};
  enum AsmRegister xmm_arg_regs[] = {XMM0, XMM1, XMM2, XMM3,
                                     XMM4, XMM5, XMM6, XMM7};

  int num_params = ir_func->params.len;
  int int_reg_idx = 0;
  int xmm_reg_idx = 0;
  int stack_offset = 16;

  /* Move the values from the registers and from the stack that we had
   * received previously by the caller, into pseudo registers.  */
  for (int i = 0; i < num_params; i++) {
    enum AsmType param_asm_type =
        type_to_asm_type(ir_func->params.data[i].type);
    bool is_float = (ir_func->params.data[i].type.kind == F32_T ||
                     ir_func->params.data[i].type.kind == F64_T);

    struct AsmOperand dst;
    dst.kind = AsmOperand_PSEUDO;
    dst.as.pseudo = ir_func->params.data[i].name;
    dst.asm_type = param_asm_type;

    struct AsmOperand src;
    if (is_float) {
      if (xmm_reg_idx < 8) {
        src.kind = AsmOperand_REG;
        src.as.reg = xmm_arg_regs[xmm_reg_idx++];
        src.asm_type = param_asm_type;
      } else {
        src.kind = AsmOperand_STACK;
        src.as.stack_offset = stack_offset;
        stack_offset += 8;
        src.asm_type = param_asm_type;
      }
    } else {
      if (int_reg_idx < 6) {
        src.kind = AsmOperand_REG;
        src.as.reg = int_arg_regs[int_reg_idx++];
        src.asm_type = param_asm_type;
      } else {
        src.kind = AsmOperand_STACK;
        src.as.stack_offset = stack_offset;
        stack_offset += 8;
        src.asm_type = param_asm_type;
      }
    }

    struct AsmInstr param_mov;
    param_mov.kind = AsmInstr_MOV;
    param_mov.as.mov.src = src;
    param_mov.as.mov.dst = dst;
    param_mov.asm_type = param_asm_type;

    vec_insert(&func.body, param_mov);
  }

  for (int i = 0; i < ir_func->body.len; i++) {
    codegen_instr(&ir_func->body.data[i], &func.body);
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
};

int get_offset(struct Map *map, char *name, int *offset)
{
  struct Map *curr;

  curr = map;

  while (curr) {
    if (curr->name && strcmp(curr->name, name) == 0) {
      return curr->offset;
    }

    if (!curr->next) {
      break;
    }

    curr = curr->next;
  }

  *offset -= 8;

  struct Map *new_entry;

  new_entry = malloc(sizeof(struct Map));

  new_entry->next = NULL;
  new_entry->name = name;
  new_entry->offset = *offset;

  curr->next = new_entry;

  return *offset;
}

static inline void replace_operand_pseudo(struct AsmOperand *op,
                                          struct Map *map, int *offset)
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
    op->as.stack_offset = get_offset(map, op->as.pseudo, offset);
  }
}

struct AsmProgram *replace_pseudo(struct AsmProgram *asmcode)
{
  for (int i = 0; i < asmcode->funcs.len; i++) {
    struct Map *map = malloc(sizeof(struct Map));
    memset(map, 0, sizeof(struct Map));
    map->next = NULL;

    int offset = 0;
    int stack_size = 0;

    for (int j = 0; j < asmcode->funcs.data[i].body.len; j++) {
      struct AsmInstr *asminstr = &asmcode->funcs.data[i].body.data[j];

      switch (asminstr->kind) {
        case AsmInstr_CVT:
          replace_operand_pseudo(&asminstr->as.cvt.src, map, &offset);
          replace_operand_pseudo(&asminstr->as.cvt.dst, map, &offset);
          break;
        case AsmInstr_UNARY:
          replace_operand_pseudo(&asminstr->as.unary.op, map, &offset);
          break;
        case AsmInstr_LEA:
          replace_operand_pseudo(&asminstr->as.lea.src, map, &offset);
          replace_operand_pseudo(&asminstr->as.lea.dst, map, &offset);
          break;
        case AsmInstr_SetCC:
          replace_operand_pseudo(&asminstr->as.setcc.op, map, &offset);
          break;
        case AsmInstr_MOV:
          replace_operand_pseudo(&asminstr->as.mov.src, map, &offset);
          replace_operand_pseudo(&asminstr->as.mov.dst, map, &offset);
          break;
        case AsmInstr_BIN:
          replace_operand_pseudo(&asminstr->as.binary.lhs, map, &offset);
          replace_operand_pseudo(&asminstr->as.binary.rhs, map, &offset);
          break;
        case AsmInstr_CMP:
          replace_operand_pseudo(&asminstr->as.cmp.lhs, map, &offset);
          replace_operand_pseudo(&asminstr->as.cmp.rhs, map, &offset);
          break;
        case AsmInstr_PUSH:
          replace_operand_pseudo(&asminstr->as.push.op, map, &offset);
          break;
        case AsmInstr_POP:
          replace_operand_pseudo(&asminstr->as.pop.op, map, &offset);
          break;
        case AsmInstr_RET:
        case AsmInstr_CALL:
        case AsmInstr_JMP:
        case AsmInstr_JmpCC:
        case AsmInstr_LBL:
          break;
      }
    }

    stack_size = -offset;
    if (stack_size % 16 != 0) {
      stack_size = (stack_size / 16 + 1) * 16;
    }

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

          is_float = (asminstr->asm_type == AsmType_FLOAT ||
                      asminstr->asm_type == AsmType_DOUBLE);

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
            bool is_float = (asminstr->asm_type == AsmType_FLOAT ||
                             asminstr->asm_type == AsmType_DOUBLE);

            struct AsmOperand scratch_op = {
                .kind = AsmOperand_REG,
                .as.reg = is_float ? XMM8 : R10,
                .asm_type = asminstr->as.cvt.dst.asm_type};
            struct AsmInstr i1 = {0}, i2 = {0};

            i1.kind = AsmInstr_CVT;
            i1.as.cvt.is_unsigned = asminstr->as.cvt.is_unsigned;
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
            struct AsmOperand scratch_op = {.kind = AsmOperand_REG,
                                            .as.reg = R10,
                                            .asm_type = AsmType_QUADWORD};
            struct AsmInstr i1 = {0}, i2 = {0};

            i1.kind = AsmInstr_LEA;
            i1.as.lea.src = asminstr->as.lea.src;
            i1.as.lea.dst = scratch_op;
            i1.asm_type = AsmType_QUADWORD;

            i2.kind = AsmInstr_MOV;
            i2.as.mov.src = scratch_op;
            i2.as.mov.dst = asminstr->as.lea.dst;
            i2.asm_type = AsmType_QUADWORD;

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
              ((asminstr->asm_type == AsmType_FLOAT ||
                asminstr->asm_type == AsmType_DOUBLE) &&
               asminstr->as.mov.dst.kind == AsmOperand_STACK) ||
              (asminstr->as.mov.src.kind == AsmOperand_MEMORY &&
               asminstr->as.mov.dst.kind == AsmOperand_STACK)) {
            enum AsmRegister scratch_reg;
            struct AsmOperand scratch_op;
            struct AsmInstrMov mov1, mov2;
            struct AsmInstr i1 = {0}, i2 = {0};

            scratch_reg = (asminstr->asm_type == AsmType_FLOAT ||
                           asminstr->asm_type == AsmType_DOUBLE)
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
          bool is_float = (asminstr->asm_type == AsmType_FLOAT ||
                           asminstr->asm_type == AsmType_DOUBLE);

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
    case AsmOperand_MEMORY: {
      fprintf(f, "%d(", op->as.mem.offset);
      switch (op->as.mem.base) {
        case AX:
          fprintf(f, "%%rax");
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
          switch (op->asm_type) {
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
        case DI: {
          switch (op->asm_type) {
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
          switch (op->asm_type) {
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
          switch (op->asm_type) {
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
          switch (op->asm_type) {
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
          switch (op->asm_type) {
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
          switch (op->asm_type) {
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
          switch (op->asm_type) {
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
        case BP: {
          switch (op->asm_type) {
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
          switch (op->asm_type) {
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
      fprintf(f, "\t");
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
          if (instr->asm_type == AsmType_FLOAT) {
            fprintf(f, "movss ");
          } else if (instr->asm_type == AsmType_DOUBLE) {
            fprintf(f, "movsd ");
          } else {
            fprintf(f, "mov");
            switch (instr->asm_type) {
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
              if (instr->asm_type == AsmType_FLOAT) {
                fprintf(f, "addss ");
              } else if (instr->asm_type == AsmType_DOUBLE) {
                fprintf(f, "addsd ");
              } else {
                fprintf(f, "add");
                switch (instr->asm_type) {
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
              if (instr->asm_type == AsmType_FLOAT) {
                fprintf(f, "subss ");
              } else if (instr->asm_type == AsmType_DOUBLE) {
                fprintf(f, "subsd ");
              } else {
                fprintf(f, "sub");
                switch (instr->asm_type) {
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
              if (instr->asm_type == AsmType_FLOAT) {
                fprintf(f, "mulss ");
              } else if (instr->asm_type == AsmType_DOUBLE) {
                fprintf(f, "mulsd ");
              } else {
                fprintf(f, "imul ");
                switch (instr->asm_type) {
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
              if (instr->asm_type == AsmType_FLOAT) {
                fprintf(f, "divss ");
              } else if (instr->asm_type == AsmType_DOUBLE) {
                fprintf(f, "divsd ");
              } else {
                assert(0 && "integer div not implemented");
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

          switch (instr->as.cmp.asm_type) {
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
            case E:
              suffix = "e";
              break;
            case NE:
              suffix = "ne";
              break;
            case L:
              suffix = "l";
              break;
            case LE:
              suffix = "le";
              break;
            case G:
              suffix = "g";
              break;
            case GE:
              suffix = "ge";
              break;
            case A:
              suffix = "a";
              break;
            case AE:
              suffix = "ae";
              break;
            case B:
              suffix = "b";
              break;
            case BE:
              suffix = "be";
              break;
          }

          fprintf(f, "j%s .L%s\n", suffix, instr->as.jmpcc.target);
          break;
        }
        case AsmInstr_SetCC: {
          char *suffix;

          switch (instr->as.setcc.cc) {
            case E:
              suffix = "e";
              break;
            case NE:
              suffix = "ne";
              break;
            case L:
              suffix = "l";
              break;
            case LE:
              suffix = "le";
              break;
            case G:
              suffix = "g";
              break;
            case GE:
              suffix = "ge";
              break;
            case A:
              suffix = "a";
              break;
            case AE:
              suffix = "ae";
              break;
            case B:
              suffix = "b";
              break;
            case BE:
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
          fprintf(f, "neg");
          switch (instr->asm_type) {
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
          if (instr->asm_type == AsmType_FLOAT) {
            fprintf(f, "cvtss2sd ");
          } else if (instr->asm_type == AsmType_DOUBLE) {
            fprintf(f, "cvtsd2ss ");
          } else if (instr->asm_type == AsmType_BYTE) {
            fprintf(f, instr->as.cvt.is_unsigned ? "movzbl " : "movsbl ");
          } else if (instr->asm_type == AsmType_WORD) {
            fprintf(f, instr->as.cvt.is_unsigned ? "movzwl " : "movswl ");
          } else if (instr->asm_type == AsmType_LONGWORD) {
            if (instr->as.cvt.is_unsigned) {
              fprintf(f, "movl ");
            } else {
              fprintf(f, "movslq ");
            }
          } else {
            assert(0 && "Unhandled cast type");
          }
          emit_operand(f, &instr->as.cvt.src);
          fprintf(f, ", ");
          emit_operand(f, &instr->as.cvt.dst);
          fprintf(f, "\n");
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
    perror("Fork failed");
    result.is_ok = false;
    result.msg = "Fork failed";
    return result;
  } else if (pid == 0) {
    if (assemble_only) {
      execlp("gcc", "gcc", "-c", path, "-o", out_path, NULL);
    } else {
      execlp("gcc", "gcc", path, "-o", out_path, NULL);
    }

    perror("Failed to execute gcc");
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

  printf("replacing pseudo...\n");
  asm_prog = *replace_pseudo(&asm_prog);
  print_asm(&asm_prog);

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

free_up2_parse:
  free_ast(parse_result.ast);

free_up2_tokenize:
  vec_free(&tokenize_result.tokens);

free_up2_fread:
  free(read_file_result.contents);

  for (int i = 0; i < global_constants.len; i++) {
    free(global_constants.data[i].name);
    free(global_constants.data[i].value);
  }
  vec_free(&global_constants);

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
