#include <assert.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

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
  TOKEN_LET,
  TOKEN_EQUAL,
  TOKEN_RETURN,
  TOKEN_NUMBER,
  TOKEN_PLUS,
  TOKEN_MINUS,
  TOKEN_STAR,
  TOKEN_SLASH,
  TOKEN_COMMA,
  TOKEN_COLON,
  TOKEN_SEMICOLON,
  TOKEN_ARROW,
  TOKEN_I8,
  TOKEN_I16,
  TOKEN_I32,
  TOKEN_I64,
  TOKEN_STR,
  TOKEN_RBRACE,
  TOKEN_STRING,
  TOKEN_ERROR,
};

struct Token {
  enum TokenKind kind;
  char *start;
  int len;
};

static inline void print_indent(int spaces)
{
  if (spaces > 0) {
    printf("%*s", spaces, "");
  }
}

struct Tokenizer {
  char *src;
};

void init_tokenizer(struct Tokenizer *tokenizer, char *src)
{
  tokenizer->src = src;
}

bool is_alpha(char c)
{
  return ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z'));
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

#define ALLOC(obj) (memcpy(malloc(sizeof((obj))), &(obj), sizeof(obj)))

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
  struct Token token = {.kind = kind, .start = tokenizer->src, .len = len};
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
      case 'l': {
        if (lookahead(tokenizer, 2, "et") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_LET, 3));
        } else {
          vec_insert(&tokens, identifier(tokenizer));
        }

        break;
      }
      case 'i': {
        if (lookahead(tokenizer, 1, "8")) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_I8, 2));
        } else if (lookahead(tokenizer, 2, "16")) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_I16, 3));
        } else if (lookahead(tokenizer, 2, "32")) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_I32, 3));
        } else if (lookahead(tokenizer, 2, "64") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_I64, 3));
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
      case 's': {
        if (lookahead(tokenizer, 2, "tr") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_STR, 3));
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
      case '=': {
        vec_insert(&tokens, mktoken(tokenizer, TOKEN_EQUAL, 1));
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
    case TOKEN_LET:
      printf("let");
      break;
    case TOKEN_EQUAL:
      printf("equal");
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
    case TOKEN_COLON:
      printf("colon");
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
    case TOKEN_ARROW:
      printf("arrow");
      break;
    case TOKEN_I8:
      printf("i8");
      break;
    case TOKEN_I16:
      printf("16");
      break;
    case TOKEN_I32:
      printf("i32");
      break;
    case TOKEN_I64:
      printf("i64");
      break;
    case TOKEN_STR:
      printf("str");
      break;
    case TOKEN_STRING:
      printf("string(\"%.*s\")", token->len, token->start);
      break;
    case TOKEN_ERROR:
      printf("ERROR");
      break;
    case TOKEN_COMMA:
      printf("comma");
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

enum LiteralKind {
  LITERAL_NUM,
  LITERAL_STR,
};

struct Literal {
  enum LiteralKind kind;
  union {
    char *str;
    long long num;
  } as;
};

enum ExprKind {
  EXPR_LITERAL,
  EXPR_VARIABLE,
  EXPR_BINARY,
  EXPR_CALL,
};

enum ExprBinKind {
  EXPR_BIN_ADD,
  EXPR_BIN_SUB,
  EXPR_BIN_MUL,
  EXPR_BIN_DIV,
};

struct ExprBin {
  enum ExprBinKind kind;
  struct Expr *lhs;
  struct Expr *rhs;
};

typedef Vector(struct Expr) VecExpr;

struct ExprCall {
  struct Expr *target;
  VecExpr arguments;
};

enum TypeKind {
  I8_T,
  I16_T,
  I32_T,
  I64_T,
  STR_T,
  FN_T,
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
    } func;
  } as;
};

void print_type(Type *t);
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
    case I8_T:
    case I16_T:
    case I32_T:
    case I64_T:
    case STR_T:
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

struct ExprVar {
  char *name;
  Type type;
};

struct Expr {
  enum ExprKind kind;
  union {
    struct Literal literal;
    struct ExprBin binary;
    struct ExprVar var;
    struct ExprCall call;
  } as;
  Type type;
};

typedef Vector(struct Stmt) VecStmt;

struct StmtRet {
  struct Expr *val;
  Type expected_retval;
};

struct StmtLet {
  char *name;
  Type type;
  struct Expr *init;
};

struct StmtBlock {
  VecStmt stmts;
};

enum StmtKind {
  STMT_LET,
  STMT_FN,
  STMT_BLOCK,
  STMT_RET,
};

typedef Vector(struct Parameter) VecParam;

struct StmtFn {
  char *name;
  VecParam params;
  Type retval;
  VecStmt body;
};

struct Stmt {
  enum StmtKind kind;
  union {
    struct StmtLet let;
    struct StmtRet ret;
    struct StmtFn fn;
    struct StmtBlock block;
  } as;
};

struct Parser {
  struct StmtFn *current_fn;
  struct Token *curr;
  struct Token *prev;
  VecToken *tokens;
  int idx;
};

struct AST {
  VecStmt stmts;
};

struct ParseResult {
  bool is_ok;
  char *msg;
  struct AST *ast;
};

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

void init_parser(struct Parser *parser, VecToken *tokens)
{
  parser->idx = 0;
  parser->tokens = tokens;
  parser->prev = NULL;
  parser->curr = NULL;
  parser->current_fn = NULL;

  advance_parser(parser);
}

struct ParseFnResult {
  bool is_ok;
  char *msg;
  union {
    struct Expr expr;
    struct Stmt stmt;
  } as;
};

struct Token *consume(struct Parser *parser, enum TokenKind kind)
{
  if (parser->curr && parser->curr->kind == kind) {
    return advance_parser(parser);
  }
  printf("Encountered wrong token:");
  print_token(parser->prev);
  printf("\n");
  return NULL;
}

struct Token *consume_any(struct Parser *parser, int n, ...)
{
  va_list ap;
  va_start(ap, n);

  for (int i = 0; i < n; i++) {
    enum TokenKind kind;
    struct Token *t;

    kind = va_arg(ap, enum TokenKind);
    t = consume(parser, kind);
    if (t) {
      va_end(ap);
      return t;
    }
  }

  va_end(ap);
  return NULL;
}

struct ParseFnResult parse_stmt(struct Parser *parser);

bool check(struct Parser *parser, enum TokenKind kind)
{
  return parser->curr->kind == kind;
}

struct ParseFnResult block(struct Parser *parser)
{
  VecStmt stmts = {0};
  struct ParseFnResult result;

  result.is_ok = true;
  result.msg = NULL;

  while (!check(parser, TOKEN_RBRACE)) {
    struct ParseFnResult r;

    r = parse_stmt(parser);
    if (!r.is_ok) {
      return r;
    }

    vec_insert(&stmts, r.as.stmt);
  }

  struct Stmt block_stmt;
  block_stmt.kind = STMT_BLOCK;
  block_stmt.as.block.stmts = stmts;

  result.as.stmt = block_stmt;

  return result;
}

char *own_string_n(const char *string, int n)
{
  char *s;

  s = malloc(strlen(string) + 1);
  snprintf(s, n + 1, "%s", string);

  return s;
}

struct Parameter {
  char *name;
  Type type;
};

Type parse_type(struct Token *token)
{ 
  if (strncmp(token->start, "i8", token->len) == 0) {
    return (Type){.kind = I8_T};
  } else if (strncmp(token->start, "i16", token->len) == 0) {
    return (Type){.kind = I16_T};
  } else if (strncmp(token->start, "i32", token->len) == 0) {
    return (Type){.kind = I32_T};
  } else if (strncmp(token->start, "i64", token->len) == 0) {
    return (Type){.kind = I64_T};
  } else if (strncmp(token->start, "str", token->len) == 0) {
    return (Type){.kind = STR_T};
  } else {
    return (Type){.kind = UNKNOWN_T};
  }
}

struct ParseFnResult parse_fn_stmt(struct Parser *parser)
{
  struct ParseFnResult result;
  struct Token *token_fn, *token_id, *token_lparen, *token_void, *token_rparen,
      *token_arrow, *token_retval, *token_lbrace, *token_rbrace;

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
      struct Token *name_token, *semicolon_token, *type_token;
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

      type_token = consume_any(parser, 5, TOKEN_I8, TOKEN_I16, TOKEN_I32, TOKEN_I64, TOKEN_STR);
      if (!type_token) {
        return (struct ParseFnResult){
            .is_ok = false,
            .as.stmt = {0},
            .msg = "Expected `name: type` format for paramters"};
      }

      name = own_string_n(name_token->start, name_token->len);
      type = parse_type(type_token);

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

  token_retval = consume_any(parser, 5, TOKEN_I8, TOKEN_I16, TOKEN_I32, TOKEN_I64, TOKEN_STR);
  if (!token_retval) {
    return (struct ParseFnResult){.is_ok = false,
                                  .as.stmt = {0},
                                  .msg = "Expected token 'i64' after '->'"};
  }

  token_lbrace = consume(parser, TOKEN_LBRACE);
  if (!token_lbrace) {
    return (struct ParseFnResult){.is_ok = false,
                                  .as.stmt = {0},
                                  .msg = "Expected token 'void' after '('"};
  }

  struct StmtFn stmt_fn;
  stmt_fn.name = own_string_n(token_id->start, token_id->len);
  stmt_fn.params = parameters;

  if (strncmp(token_retval->start, "i64", token_retval->len) == 0) {
    stmt_fn.retval = (Type){.kind = I64_T};
  } else if (strncmp(token_retval->start, "str", token_retval->len) == 0) {
    stmt_fn.retval = (Type){.kind = STR_T};
  } else {
    stmt_fn.retval = (Type){.kind = UNKNOWN_T};
  }

  struct StmtFn *prev_fn = parser->current_fn;
  parser->current_fn = &stmt_fn;

  struct ParseFnResult block_result = block(parser);

  parser->current_fn = prev_fn;

  if (!block_result.is_ok) {
    return (struct ParseFnResult){
        .is_ok = false, .as.stmt = {0}, .msg = "Expected block after '{'"};
  }

  struct Stmt body = block_result.as.stmt;

  token_rbrace = consume(parser, TOKEN_RBRACE);
  if (!token_rbrace) {
    return (struct ParseFnResult){.is_ok = false,
                                  .as.stmt = {0},
                                  .msg = "Expected token '}' after fn body"};
  }

  stmt_fn.body = body.as.block.stmts;
  stmt_fn.retval = parse_type(token_retval);

  struct Stmt stmt;
  stmt.kind = STMT_FN;
  stmt.as.fn = stmt_fn;

  result.as.stmt = stmt;

  return result;
}

static bool match(struct Parser *parser, int size, ...)
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

struct ParseFnResult primary(struct Parser *parser)
{
  struct ParseFnResult res;

  res.is_ok = true;
  res.msg = NULL;

  if (check(parser, TOKEN_NUMBER)) {
    struct Literal literal;
    struct Token *token_literal;

    token_literal = consume(parser, TOKEN_NUMBER);
    if (!token_literal) {
      return (struct ParseFnResult){
          .is_ok = false, .as.expr = {0}, .msg = "Expected number"};
    }

    literal.kind = LITERAL_NUM;
    literal.as.num = strtol(parser->prev->start, NULL, 10);

    res.as.expr = (struct Expr){.kind = EXPR_LITERAL, .as.literal = literal};
  } else if (check(parser, TOKEN_STRING)) {
    struct Literal literal;
    struct Token *token_literal;

    token_literal = consume(parser, TOKEN_STRING);
    if (!token_literal) {
      return (struct ParseFnResult){
          .is_ok = false, .as.expr = {0}, .msg = "Expected string"};
    }

    literal.kind = LITERAL_STR;
    literal.as.str = own_string_n(token_literal->start, token_literal->len);

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
  } else {
    res.is_ok = false;
    res.msg = "Expected number, string, or identifier";
    return res;
  }

  return res;
}

struct ParseFnResult parse_expr(struct Parser *parser);
void print_expr(struct Expr *expr, int spaces);
void free_expr(struct Expr *expr);

struct ParseFnResult finish_call(struct Parser *parser, struct Expr callee)
{
  VecExpr arguments = {0};
  if (!check(parser, TOKEN_RPAREN)) {
    do {
      struct ParseFnResult r;

      r = parse_expr(parser);
      if (!r.is_ok) {
        for (size_t i = 0; i < arguments.len; i++) {
          free_expr(&arguments.data[i]);
        }
        vec_free(&arguments);
        return r;
      }
      vec_insert(&arguments, r.as.expr);
    } while (match(parser, 1, TOKEN_COMMA));
  }

  struct Token *token_rparen;

  token_rparen = consume(parser, TOKEN_RPAREN);
  if (!token_rparen) {
    for (size_t i = 0; i < arguments.len; i++) {
      free_expr(&arguments.data[i]);
    }
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

struct ParseFnResult factor(struct Parser *parser)
{
  struct ParseFnResult left_res, right_res;
  struct Expr left, right;

  left_res = call(parser);
  if (!left_res.is_ok) {
    return left_res;
  }

  left = left_res.as.expr;
  while (match(parser, 2, TOKEN_STAR, TOKEN_SLASH)) {
    char *op = parser->prev->start;

    right_res = call(parser);
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

struct ParseFnResult parse_expr(struct Parser *parser)
{
  struct ParseFnResult res;
  struct Expr expr;

  res.is_ok = false;
  res.msg = NULL;

  res = term(parser);
  if (!res.is_ok) {
    return res;
  }

  expr = res.as.expr;

  return (struct ParseFnResult){.is_ok = true, .msg = NULL, .as.expr = expr};
}

struct ParseFnResult parse_ret_stmt(struct Parser *parser)
{
  struct ParseFnResult result;
  struct Token *token_ret, *token_semicolon;
  struct Expr *expr;

  expr = NULL;

  result.is_ok = true;
  result.msg = NULL;

  token_ret = consume(parser, TOKEN_RETURN);
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

Type parse_type_decl(struct Parser *parser)
{
  if (match(parser, 1, TOKEN_I8)) {
    return (Type){.kind = I8_T};
  } else if (match(parser, 1, TOKEN_I16)) {
    return (Type){.kind = I16_T};
  } else if (match(parser, 1, TOKEN_I32)) {
    return (Type){.kind = I32_T};
  } else if (match(parser, 1, TOKEN_I64)) {
    return (Type){.kind = I64_T};
  } else if (match(parser, 1, TOKEN_STR)) {
    return (Type){.kind = STR_T};
  }
  return (Type){.kind = UNKNOWN_T};
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

  type = parse_type_decl(parser);

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

struct ParseFnResult parse_stmt(struct Parser *parser)
{
  struct ParseFnResult result;

  result.is_ok = true;
  result.msg = NULL;

  switch (parser->curr->kind) {
    case TOKEN_FN: {
      struct ParseFnResult fn_res = parse_fn_stmt(parser);
      if (!fn_res.is_ok) {
        return (struct ParseFnResult){
            .is_ok = false, .msg = "Failed parsing 'fn' stmt", .as.stmt = {0}};
      }
      result.as.stmt = fn_res.as.stmt;
      break;
    }
    case TOKEN_RETURN: {
      struct ParseFnResult ret_res = parse_ret_stmt(parser);
      if (!ret_res.is_ok) {
        return (struct ParseFnResult){
            .is_ok = false, .msg = "Failed parsing 'ret' stmt", .as.stmt = {0}};
      }
      result.as.stmt = ret_res.as.stmt;
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
    default:
      assert(0);
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
void print_binary_op(int kind)
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
    default:
      printf("???");
      break;
  }
}

void print_expr(struct Expr *expr, int spaces)
{
  switch (expr->kind) {
    case EXPR_LITERAL: {
      if (expr->as.literal.kind == LITERAL_NUM) {
        printf("Literal(%lld)", expr->as.literal.as.num);
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
      printf(",\n");

      print_indent(spaces);
      printf(")");
      break;
    }
    case EXPR_CALL: {
      printf("Call(\n");

      print_indent(spaces + 2);
      printf("target = ");
      print_expr(expr->as.call.target, 0);
      printf(",\n");

      print_indent(spaces + 2);
      printf("arguments: [\n");

      for (int i = 0; i < expr->as.call.arguments.len; i++) {
        print_indent(spaces + 4);
        print_expr(&expr->as.call.arguments.data[i], 0);
        printf(",\n");
      }

      print_indent(spaces + 2);
      printf("],\n");

      print_indent(spaces);
      printf(")");
      break;
    }
  }
}

void print_stmt(struct Stmt *stmt, int spaces)
{
  switch (stmt->kind) {
    case STMT_FN: {
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
      print_type(&stmt->as.fn.retval);
      printf(",\n");

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_BLOCK: {
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
      print_type(&stmt->as.let.type);
      printf(",\n");

      print_indent(spaces + 2);
      printf("init: ");
      print_expr(stmt->as.let.init, spaces + 2);
      printf(",\n");

      print_indent(spaces);
      printf(")\n");
      break;
    }
    default:
      assert(0);
  }
}

void print_ast(struct AST *ast)
{
  for (int i = 0; i < ast->stmts.len; i++) {
    print_stmt(&ast->stmts.data[i], 0);
  }
}

void free_expr(struct Expr *expr)
{
  switch (expr->kind) {
    case EXPR_CALL: {
      free_expr(expr->as.call.target);
      free(expr->as.call.target);
      vec_free(&expr->as.call.arguments);
      break;
    }
    case EXPR_LITERAL: {
      switch (expr->as.literal.kind) {
        case LITERAL_NUM:
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

void free_stmt(struct Stmt *stmt)
{
  switch (stmt->kind) {
    case STMT_LET: {
      free(stmt->as.let.name);
      free_expr(stmt->as.let.init);
      free(stmt->as.let.init);
      break;
    }
    case STMT_FN: {
      free(stmt->as.fn.name);
      for (int i = 0; i < stmt->as.fn.params.len; i++) {
        free(stmt->as.fn.params.data[i].name);
      }
      vec_free(&stmt->as.fn.params);
      for (int i = 0; i < stmt->as.fn.body.len; i++) {
        free_stmt(&stmt->as.fn.body.data[i]);
      }
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
  }
}

enum IRInstrKind {
  IRInstr_BINARY,
  IRInstr_RET,
  IRInstr_COPY,
  IRInstr_CALL,
};

enum IRInstrBinaryKind {
  IRInstrBinary_ADD,
  IRInstrBinary_SUB,
  IRInstrBinary_MUL,
  IRInstrBinary_DIV,
};

enum IRValueKind {
  IRValue_CONST,
  IRValue_VAR,
};

struct IRValue {
  enum IRValueKind kind;
  union {
    char *var;
    long long konst;
  } as;
};

typedef Vector(struct IRValue *) VecIRValuePtr;

struct IRInstr_Call {
  struct Expr target;
  VecIRValuePtr args;
  struct IRValue *dst;
};

struct IRInstr_Binary {
  enum IRInstrBinaryKind kind;
  struct IRValue *lhs;
  struct IRValue *rhs;
  struct IRValue *dst;
};

struct IRInstr_Ret {
  struct IRValue *val;
};

struct IRInstr_Copy {
  struct IRValue *src;
  struct IRValue *dst;
};

struct IRInstr {
  enum IRInstrKind kind;
  union {
    struct IRInstr_Call call;
    struct IRInstr_Binary binary;
    struct IRInstr_Ret ret;
    struct IRInstr_Copy copy;
  } as;
};

typedef Vector(struct IRInstr) VecIRInstr;

struct IRFunction {
  char *name;
  VecParam params;
  Type retval;
  VecIRInstr body;
};

typedef Vector(struct IRFunction *) VecIRFunctionPtr;

struct IRProgram {
  VecIRFunctionPtr funcs;
};

int mktmp(void)
{
  static int i = 0;
  return i++;
}

struct IRValue *make_ir_var(void)
{
  struct IRValue *var;
  int i;

  i = mktmp();

  var = malloc(sizeof(struct IRValue));
  memset(var, 0, sizeof(struct IRValue));

  var->kind = IRValue_VAR;

  int digit_len = snprintf(NULL, 0, "%d", i);
  int total_len = strlen("tmp") + strlen(".") + digit_len + 1;
  var->as.var = malloc(total_len);

  snprintf(var->as.var, total_len, "tmp.%d", i);

  return var;
}

void free_ir_val(struct IRValue *v);

struct IRValue *irfy_expr(VecIRInstr *instrs, struct Expr *expr)
{
  switch (expr->kind) {
    case EXPR_LITERAL: {
      struct IRValue *ir_val;

      ir_val = malloc(sizeof(struct IRValue));
      memset(ir_val, 0, sizeof(struct IRValue));

      ir_val->kind = IRValue_CONST;
      ir_val->as.konst = expr->as.literal.as.num;

      return ir_val;
    }
    case EXPR_VARIABLE: {
      struct IRValue *r;

      r = malloc(sizeof(struct IRValue));

      r->kind = IRValue_VAR;

      r->as.var = malloc(strlen(expr->as.var.name) + 1);
      strcpy(r->as.var, expr->as.var.name);

      return r;
    }
    case EXPR_BINARY: {
      struct IRValue *lhs, *rhs, *dst;

      lhs = irfy_expr(instrs, expr->as.binary.lhs);
      rhs = irfy_expr(instrs, expr->as.binary.rhs);

      dst = make_ir_var();

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
        default:
          assert(0);
      }

      struct IRInstr_Binary bininstr = {
          .lhs = lhs, .rhs = rhs, .dst = dst, .kind = kind};
      struct IRInstr instr;

      instr.kind = IRInstr_BINARY;
      instr.as.binary = bininstr;

      vec_insert(instrs, instr);

      struct IRValue *ret;

      ret = malloc(sizeof(struct IRValue));

      ret->kind = IRValue_VAR;
      ret->as.var = malloc(strlen(dst->as.var) + 1);
      strcpy(ret->as.var, dst->as.var);

      return ret;
    }
    case EXPR_CALL: {
      struct IRValue *result =
          expr->type.kind == UNKNOWN_T ? NULL : make_ir_var();

      VecIRValuePtr args = {0};
      for (int i = 0; i < expr->as.call.arguments.len; i++) {
        vec_insert(&args, irfy_expr(instrs, &expr->as.call.arguments.data[i]));
      }

      struct IRInstr_Call call_instr;

      call_instr.target = *expr->as.call.target;
      call_instr.args = args;
      call_instr.dst = result;

      struct IRInstr i;

      i.kind = IRInstr_CALL;
      i.as.call = call_instr;

      vec_insert(instrs, i);

      if (!result) {
        return NULL;
      }

      struct IRValue *ret = malloc(sizeof(struct IRValue));
      ret->kind = IRValue_VAR;
      ret->as.var = malloc(strlen(result->as.var) + 1);
      strcpy(ret->as.var, result->as.var);

      return ret;
    }
    default:
      assert(0);
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

void irfy_stmt(VecIRInstr *instrs, struct Stmt *stmt)
{
  switch (stmt->kind) {
    case STMT_FN:
      assert(0);
    case STMT_LET: {
      struct IRValue *res, *dst;
      struct IRInstr cpy;

      res = irfy_expr(instrs, stmt->as.let.init);

      dst = malloc(sizeof(struct IRValue));
      dst->kind = IRValue_VAR;

      dst->as.var = malloc(strlen(stmt->as.let.name) + 1);
      strcpy(dst->as.var, stmt->as.let.name);

      cpy.kind = IRInstr_COPY;
      cpy.as.copy = (struct IRInstr_Copy){.src = res, .dst = dst};

      vec_insert(instrs, cpy);
      break;
    }
    case STMT_BLOCK: {
      for (int i = 0; i < stmt->as.block.stmts.len; i++) {
        irfy_stmt(instrs, &stmt->as.block.stmts.data[i]);
      }
      break;
    }
    case STMT_RET: {
      struct IRInstr i;

      i.kind = IRInstr_RET;
      i.as.ret.val =
          stmt->as.ret.val ? irfy_expr(instrs, stmt->as.ret.val) : NULL;

      vec_insert(instrs, i);
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

void free_ir_instr(struct IRInstr *instr)
{
  switch (instr->kind) {
    case IRInstr_BINARY: {
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
    case IRInstr_COPY: {
      free_ir_val(instr->as.copy.src);
      free_ir_val(instr->as.copy.dst);
      break;
    }
    case IRInstr_CALL: {
      for (int i = 0; i < instr->as.call.args.len; i++) {
        free_ir_val(instr->as.call.args.data[i]);
      }
      vec_free(&instr->as.call.args);
      free_ir_val(instr->as.call.dst);
      break;
    }
    default:
      assert(0);
  }
}

void free_ir_fn(struct IRFunction *func)
{
  for (int i = 0; i < func->body.len; i++) {
    free_ir_instr(&func->body.data[i]);
  }
  vec_free(&func->body);
  free(func);
}

void free_ir_prog(struct IRProgram *prog)
{
  for (int i = 0; i < prog->funcs.len; i++) {
    free_ir_fn(prog->funcs.data[i]);
  }
  vec_free(&prog->funcs);
}

void print_ir_binary_op(int kind)
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
    default:
      printf("???");
      break;
  }
}

void print_ir_val(struct IRValue *ir_val, int spaces)
{
  switch (ir_val->kind) {
    case IRValue_CONST: {
      printf("IRValue(\n");

      print_indent(spaces + 2);
      printf("type = CONST,\n");

      print_indent(spaces + 2);
      printf("value = %lld,\n", ir_val->as.konst);

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

void print_ir_instr(struct IRInstr *instr, int spaces)
{
  print_indent(spaces);

  switch (instr->kind) {
    case IRInstr_COPY: {
      printf("IRInstr_COPY(\n");

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
    case IRInstr_BINARY: {
      printf("IRInstr_BINARY(\n");

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

void print_ir(struct IRProgram *prog)
{
  for (int i = 0; i < prog->funcs.len; i++) {
    print_ir_fn(prog->funcs.data[i]);
  }
}

enum AsmInstrBinaryKind {
  AsmInstrBinary_ADD,
  AsmInstrBinary_SUB,
  AsmInstrBinary_MUL,
  AsmInstrBinary_DIV,
};

enum AsmInstrKind {
  AsmInstr_PUSH,
  AsmInstr_POP,
  AsmInstr_MOV,
  AsmInstr_BINARY,
  AsmInstr_RET,
  AsmInstr_CALL,
};

enum AsmOperandKind {
  AsmOperand_IMM,
  AsmOperand_PSEUDO,
  AsmOperand_REG,
  AsmOperand_STACK,
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
};

enum AsmType {
  AsmType_BYTE     = 1,
  AsmType_WORD     = 2,
  AsmType_LONGWORD = 4,
  AsmType_QUADWORD = 8,
};

struct AsmOperand {
  enum AsmOperandKind kind;
  enum AsmType asm_type;
  union {
    int imm;
    char *pseudo;
    enum AsmRegister reg;
    int stack_offset;
  } as;
};

struct AsmInstrRet {
  short __dummy;
};

struct AsmInstrCall {
  char *target;
};

struct AsmInstrBinary {
  enum AsmInstrBinaryKind kind;
  struct AsmOperand lhs;
  struct AsmOperand rhs;
};

struct AsmInstrMov {
  struct AsmOperand src;
  struct AsmOperand dst;
};

struct AsmInstrPush {
  struct AsmOperand op;
};

struct AsmInstrPop {
  struct AsmOperand op;
};

struct AsmInstr {
  enum AsmInstrKind kind;
  union {
    struct AsmInstrMov mov;
    struct AsmInstrBinary binary;
    struct AsmInstrRet ret;
    struct AsmInstrPush push;
    struct AsmInstrPop pop;
    struct AsmInstrCall call;
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

struct AsmResult {
  bool is_ok;
  char *msg;
  struct AsmProgram prog;
};

struct AsmOperand codegen_irvalue(struct IRValue *val)
{
  switch (val->kind) {
    case IRValue_CONST: {
      struct AsmOperand operand;
      operand.kind = AsmOperand_IMM;
      operand.as.imm = val->as.konst;

      return operand;
    }
    case IRValue_VAR: {
      struct AsmOperand operand;
      operand.kind = AsmOperand_PSEUDO;
      operand.as.pseudo = val->as.var;
      return operand;
    }
    default:
      assert(0);
  }
}

void codegen_instr(struct IRInstr *ir_instr, VecAsmInstr *instrs)
{
  switch (ir_instr->kind) {
    case IRInstr_CALL: {
      /* Since i64 is the only data type for now, we worry only about these
       * registers. The first six arguments to the callee are passed via these
       * six registers. The rest are passed on the stack, in reverse order.  */
      enum AsmRegister arg_regs[] = {DI, SI, DX, CX, R8, R9};

      int num_args = ir_instr->as.call.args.len;
      int num_reg_args = num_args > 6 ? 6 : num_args;
      int num_stack_args = num_args > 6 ? num_args - 6 : 0;

      /* System V AMD64 ABI requires that the stack be aligned to 16 bytes
       * before the call instruction.
       *
       * In case we pushed an odd number of stack arguments, the stack will be
       * misaligned by 8 bytes, since each argument on the stack (`pushq arg7`)
       * occupies 8 bytes.  The stack will NOT be misaligned if the number of
       * pushed arguments is even.  */
      int stack_padding = (num_stack_args % 2 != 0) ? 8 : 0;

      if (stack_padding != 0) {
        struct AsmInstr padding_instr;

        padding_instr.kind = AsmInstr_BINARY;
        padding_instr.as.binary.kind = AsmInstrBinary_SUB;
        padding_instr.as.binary.lhs = (struct AsmOperand){
            .kind = AsmOperand_IMM, .as.imm = stack_padding};
        padding_instr.as.binary.rhs =
            (struct AsmOperand){.kind = AsmOperand_REG, .as.reg = SP};

        vec_insert(instrs, padding_instr);
      }

      /* Move the argument IRValues into corresponding regs.  */
      for (int i = 0; i < num_reg_args; i++) {
        struct AsmOperand arg_op =
            codegen_irvalue(ir_instr->as.call.args.data[i]);
        struct AsmInstr mov_instr;
        mov_instr.kind = AsmInstr_MOV;
        mov_instr.as.mov.src = arg_op;
        mov_instr.as.mov.dst =
            (struct AsmOperand){.kind = AsmOperand_REG, .as.reg = arg_regs[i]};

        vec_insert(instrs, mov_instr);
      }

      /* ...the rest of the arguments goes on the stack, in reverse order. */
      for (int i = num_args - 1; i >= 6; i--) {
        struct AsmOperand arg_op =
            codegen_irvalue(ir_instr->as.call.args.data[i]);
        struct AsmInstr push_instr;
        push_instr.kind = AsmInstr_PUSH;
        push_instr.as.push.op = arg_op;
        vec_insert(instrs, push_instr);
      }

      struct AsmInstr call_instr;
      call_instr.kind = AsmInstr_CALL;

      call_instr.as.call.target = ir_instr->as.call.target.as.var.name;
      vec_insert(instrs, call_instr);

      /* The caller is responsible for cleaning up the stack after the call
       * instruction. Since the stack on x86 grows downward, we need to ADD (not
       * SUB).  */
      int bytes_to_remove = (num_stack_args * 8) + stack_padding;
      if (bytes_to_remove != 0) {
        struct AsmInstr cleanup_instr;
        cleanup_instr.kind = AsmInstr_BINARY;
        cleanup_instr.as.binary.kind = AsmInstrBinary_ADD;
        cleanup_instr.as.binary.lhs = (struct AsmOperand){
            .kind = AsmOperand_IMM, .as.imm = bytes_to_remove};
        cleanup_instr.as.binary.rhs =
            (struct AsmOperand){.kind = AsmOperand_REG, .as.reg = SP};
        vec_insert(instrs, cleanup_instr);
      }

      /* The caller is responsible for moving the return value from AX to a safe
       * destination if it wants to keep it, because AX is easily clobbered.  */
      if (ir_instr->as.call.dst) {
        struct AsmOperand dst_op = codegen_irvalue(ir_instr->as.call.dst);
        struct AsmInstr mov_instr;
        mov_instr.kind = AsmInstr_MOV;
        mov_instr.as.mov.src =
            (struct AsmOperand){.kind = AsmOperand_REG, .as.reg = AX};
        mov_instr.as.mov.dst = dst_op;
        vec_insert(instrs, mov_instr);
      }

      break;
    }
    case IRInstr_COPY: {
      struct AsmOperand src, dst;

      src = codegen_irvalue(ir_instr->as.copy.src);
      dst = codegen_irvalue(ir_instr->as.copy.dst);

      struct AsmInstr i;
      struct AsmInstrMov mov;

      mov.src = src;
      mov.dst = dst;

      i.kind = AsmInstr_MOV;
      i.as.mov = mov;

      vec_insert(instrs, i);
      break;
    }
    case IRInstr_BINARY: {
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

      struct AsmInstr i1, i2;

      i1.kind = AsmInstr_MOV;
      i1.as.mov.src = lhs;
      i1.as.mov.dst = dst;

      i2.kind = AsmInstr_BINARY;
      i2.as.binary.kind = kind;
      i2.as.binary.lhs = rhs;
      i2.as.binary.rhs = dst;

      vec_insert(instrs, i1);
      vec_insert(instrs, i2);

      break;
    }
    case IRInstr_RET: {
      struct AsmInstrPop pop;
      struct AsmInstrMov mov;
      struct AsmInstrRet ret;
      struct AsmOperand retval;
      struct AsmInstr i1, e1, e2, i2;

      ret.__dummy = 0;

      if (ir_instr->as.ret.val) {
        retval = codegen_irvalue(ir_instr->as.ret.val);
      } else {
        retval = (struct AsmOperand){.kind = AsmOperand_IMM, .as.imm = 0};
      }

      i1.kind = AsmInstr_MOV;
      i1.as.mov.src = retval;
      i1.as.mov.dst = (struct AsmOperand){.kind = AsmOperand_REG, .as.reg = AX};

      mov.src = (struct AsmOperand){.kind = AsmOperand_REG, .as.reg = BP};
      mov.dst = (struct AsmOperand){.kind = AsmOperand_REG, .as.reg = SP};

      pop.op = (struct AsmOperand){.kind = AsmOperand_REG, .as.reg = BP};

      e1.kind = AsmInstr_MOV;
      e1.as.mov = mov;

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
  push.op = (struct AsmOperand){.kind = AsmOperand_REG, .as.reg = BP};
  p1.kind = AsmInstr_PUSH;
  p1.as.push = push;

  /*  ...and place ours SP into BP.  */
  mov.src = (struct AsmOperand){.kind = AsmOperand_REG, .as.reg = SP};
  mov.dst = (struct AsmOperand){.kind = AsmOperand_REG, .as.reg = BP};
  p2.kind = AsmInstr_MOV;
  p2.as.mov = mov;

  /* We will need to reserve the stack space for our local variables.
   * NOTE: 0 for now is a placeholder that is patched up later on.  */
  sub.kind = AsmInstrBinary_SUB;
  sub.lhs = (struct AsmOperand){.kind = AsmOperand_IMM, .as.imm = 0};
  sub.rhs = (struct AsmOperand){.kind = AsmOperand_REG, .as.reg = SP};
  p3.kind = AsmInstr_BINARY;
  p3.as.binary = sub;

  vec_insert(&func.body, p1);
  vec_insert(&func.body, p2);
  vec_insert(&func.body, p3);

  enum AsmRegister arg_regs[] = {DI, SI, DX, CX, R8, R9};
  int num_params = ir_func->params.len;

  /* Move the values from the registers and from the stack that we had received
   * previously by the caller, into pseudo registers.  */
  for (int i = 0; i < num_params; i++) {
    struct AsmOperand dst;
    dst.kind = AsmOperand_PSEUDO;
    dst.as.pseudo = ir_func->params.data[i].name;

    struct AsmOperand src;
    if (i < 6) {
      src.kind = AsmOperand_REG;
      src.as.reg = arg_regs[i];
    } else {
      src.kind = AsmOperand_STACK;
      /* Upon executing the call instruction by the caller.
       * the CPU will push the return address on the stack,
       * and therefore subtract 8 from %rsp.  Then the next
       * thing the callee does is `push %rbp`, which subtracts yet 8`.
       * This is 16 totaled up.
       * Now, the callee executes `movq %rsp, %rbp`.
       * This means that the return address is at 8(%rbp), and first
       * stack argument is at 16(%rbp) (more backward in time).
       * The local variables are at -8(%rbp).  */
      src.as.stack_offset = 16 + ((i - 6) * 8);
    }

    struct AsmInstr param_mov;
    param_mov.kind = AsmInstr_MOV;
    param_mov.as.mov.src = src;
    param_mov.as.mov.dst = dst;

    vec_insert(&func.body, param_mov);
  }

  for (int i = 0; i < ir_func->body.len; i++) {
    codegen_instr(&ir_func->body.data[i], &func.body);
  }

  return func;
}

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

void print_asm_operand(struct AsmOperand *op)
{
  switch (op->kind) {
    case AsmOperand_IMM: {
      printf("AsmOperand_IMM(%d)", op->as.imm);
      break;
    }
    case AsmOperand_PSEUDO: {
      printf("AsmOperand_PSEUDO(%s)", op->as.pseudo);
      break;
    }
    case AsmOperand_REG: {
      printf("AsmOperand_REG(");
      switch (op->as.reg) {
        case AX: {
          printf("AX");
          break;
        }
        case DI: {
          printf("DI");
          break;
        }
        case SI: {
          printf("SI");
          break;
        }
        case DX: {
          printf("DX");
          break;
        }
        case CX: {
          printf("CX");
          break;
        }

        case R8: {
          printf("R8");
          break;
        }
        case R9: {
          printf("R9");
          break;
        }

        case R10: {
          printf("R10");
          break;
        }
        case BP: {
          printf("BP");
          break;
        }
        case SP: {
          printf("SP");
          break;
        }
        default:
          assert(0);
      }
      printf(")");
      break;
    }
    case AsmOperand_STACK: {
      printf("AsmOperand_STACK(offset = %d)", op->as.stack_offset);
      break;
    }
    default:
      assert(0);
  }
}

void print_asm_instr(struct AsmInstr *instr)
{
  switch (instr->kind) {
    case AsmInstr_CALL: {
      printf("AsmInstr_CALL(...)");
      break;
    }
    case AsmInstr_POP: {
      printf("AsmInstr_POP(op = ");
      print_asm_operand(&instr->as.pop.op);
      printf(")\n");
      break;
    }
    case AsmInstr_PUSH: {
      printf("AsmInstr_PUSH(op = ");
      print_asm_operand(&instr->as.push.op);
      printf(")\n");
      break;
    }
    case AsmInstr_MOV: {
      printf("AsmInstr_MOV(src = ");
      print_asm_operand(&instr->as.mov.src);
      printf(", dst = ");
      print_asm_operand(&instr->as.mov.dst);
      printf(")\n");
      break;
    }
    case AsmInstr_BINARY: {
      printf("AsmInstr_BINARY(kind = ");
      switch (instr->as.binary.kind) {
        case AsmInstrBinary_ADD: {
          printf("ADD");
          break;
        }
        case AsmInstrBinary_SUB: {
          printf("SUB");
          break;
        }
        case AsmInstrBinary_MUL: {
          printf("MUL");
          break;
        }
        case AsmInstrBinary_DIV: {
          printf("DIV");
          break;
        }
      }
      printf(", lhs = ");
      print_asm_operand(&instr->as.binary.lhs);
      printf(", rhs = ");
      print_asm_operand(&instr->as.binary.rhs);
      printf(")\n");
      break;
    }
    case AsmInstr_RET: {
      printf("AsmInstr_RET\n");
      break;
    }
  }
}

void print_asm_fn(struct AsmFunction *fn)
{
  printf("AsmFunction(name = %s, body: [\n", fn->name);
  for (int i = 0; i < fn->body.len; i++) {
    print_asm_instr(&fn->body.data[i]);
  }
}

void print_asm(struct AsmProgram *prog)
{
  for (int i = 0; i < prog->funcs.len; i++) {
    print_asm_fn(&prog->funcs.data[i]);
  }
}

void free_asm_instr(struct AsmInstr *instr)
{
  (void) instr;
  return;
}

void free_asm_fn(struct AsmFunction *fn)
{
  for (int i = 0; i < fn->body.len; i++) {
    free_asm_instr(&fn->body.data[i]);
  }
  vec_free(&fn->body);
}

void free_asm(struct AsmProgram *prog)
{
  for (int i = 0; i < prog->funcs.len; i++) {
    free_asm_fn(&prog->funcs.data[i]);
  }
  vec_free(&prog->funcs);
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

struct AsmProgram *replace_pseudo(struct AsmProgram *asmcode)
{
  struct Map *map;
  int offset, stack_size;

  offset = 0;
  stack_size = 0;

  map = malloc(sizeof(struct Map));
  memset(map, 0, sizeof(struct Map));
  map->next = NULL;

  for (int i = 0; i < asmcode->funcs.len; i++) {
    for (int j = 0; j < asmcode->funcs.data[i].body.len; j++) {
      struct AsmInstr *asminstr = &asmcode->funcs.data[i].body.data[j];
      switch (asminstr->kind) {
        case AsmInstr_MOV: {
          if (asminstr->as.mov.src.kind == AsmOperand_PSEUDO) {
            asminstr->as.mov.src.kind = AsmOperand_STACK;
            asminstr->as.mov.src.as.stack_offset =
                get_offset(map, asminstr->as.mov.src.as.pseudo, &offset);
          }
          if (asminstr->as.mov.dst.kind == AsmOperand_PSEUDO) {
            asminstr->as.mov.dst.kind = AsmOperand_STACK;
            asminstr->as.mov.dst.as.stack_offset =
                get_offset(map, asminstr->as.mov.dst.as.pseudo, &offset);
          }

          break;
        }
        case AsmInstr_BINARY: {
          if (asminstr->as.binary.lhs.kind == AsmOperand_PSEUDO) {
            asminstr->as.binary.lhs.kind = AsmOperand_STACK;
            asminstr->as.binary.lhs.as.stack_offset =
                get_offset(map, asminstr->as.binary.lhs.as.pseudo, &offset);
          }
          if (asminstr->as.binary.rhs.kind == AsmOperand_PSEUDO) {
            asminstr->as.binary.rhs.kind = AsmOperand_STACK;
            asminstr->as.binary.rhs.as.stack_offset =
                get_offset(map, asminstr->as.binary.rhs.as.pseudo, &offset);
          }

          break;
        }
        case AsmInstr_RET: {
          break;
        }
        default:
          break;
      }
    }
    stack_size = -offset;
    if (stack_size % 16 != 0) {
      stack_size = (stack_size / 16 + 1) * 16;
    }

    asmcode->funcs.data[i].body.data[2].as.binary.lhs.as.imm = stack_size;
  }

  struct Map *curr, *tmp;

  curr = map;
  while (curr) {
    tmp = curr;
    curr = tmp->next;
    free(tmp);
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
        case AsmInstr_MOV: {
          if (asminstr->as.mov.src.kind == AsmOperand_STACK &&
              asminstr->as.mov.dst.kind == AsmOperand_STACK) {
            enum AsmRegister scratch_reg;
            struct AsmOperand scratch_op;
            struct AsmInstrMov mov1, mov2;
            struct AsmInstr i1, i2;

            scratch_reg = R10;

            scratch_op.kind = AsmOperand_REG;
            scratch_op.as.reg = scratch_reg;

            i1.kind = AsmInstr_MOV;
            mov1.src = asminstr->as.mov.src;
            mov1.dst = scratch_op;
            i1.as.mov = mov1;

            i2.kind = AsmInstr_MOV;
            mov2.src = scratch_op;
            mov2.dst = asminstr->as.mov.dst;
            i2.as.mov = mov2;

            vec_insert(&instrs, i1);
            vec_insert(&instrs, i2);
          } else {
            vec_insert(&instrs, *asminstr);
          }

          break;
        }
        case AsmInstr_BINARY: {
          /* imul cannot use mem as dst */
          if (asminstr->as.binary.kind == AsmInstrBinary_MUL &&
              asminstr->as.binary.rhs.kind == AsmOperand_STACK) {
            enum AsmRegister scratch_reg = R10;
            struct AsmOperand scratch_op = {.kind = AsmOperand_REG,
                                            .as.reg = scratch_reg};
            struct AsmInstrMov mov1, mov2;
            struct AsmInstrBinary bin;
            struct AsmInstr i1, i2, i3;

            i1.kind = AsmInstr_MOV;
            mov1.src = asminstr->as.binary.rhs;
            mov1.dst = scratch_op;
            i1.as.mov = mov1;

            i2.kind = AsmInstr_BINARY;
            bin.kind = AsmInstrBinary_MUL;
            bin.lhs = asminstr->as.binary.lhs;
            bin.rhs = scratch_op;
            i2.as.binary = bin;

            i3.kind = AsmInstr_MOV;
            mov2.src = scratch_op;
            mov2.dst = asminstr->as.binary.rhs;
            i3.as.mov = mov2;

            vec_insert(&instrs, i1);
            vec_insert(&instrs, i2);
            vec_insert(&instrs, i3);

            break;
          }

          /* binary ops cannot use mem as both operands */
          if (asminstr->as.binary.lhs.kind == AsmOperand_STACK &&
              asminstr->as.binary.rhs.kind == AsmOperand_STACK) {
            enum AsmRegister scratch_reg;
            struct AsmOperand scratch_op;
            struct AsmInstrMov mov;
            struct AsmInstrBinary bin;
            struct AsmInstr i1, i2;

            scratch_reg = R10;

            scratch_op.kind = AsmOperand_REG;
            scratch_op.as.reg = scratch_reg;

            i1.kind = AsmInstr_MOV;
            mov.src = asminstr->as.binary.lhs;
            mov.dst = scratch_op;
            i1.as.mov = mov;

            i2.kind = AsmInstr_BINARY;
            bin.kind = asminstr->as.binary.kind;
            bin.lhs = scratch_op;
            bin.rhs = asminstr->as.binary.rhs;
            i2.as.binary = bin;

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
    case AsmOperand_IMM: {
      fprintf(f, "$%d", op->as.imm);
      break;
    }
    case AsmOperand_PSEUDO: {
      assert(0 && "not implemented");
      break;
    }
    case AsmOperand_REG: {
      switch (op->as.reg) {
        case AX: {
          fprintf(f, "%%rax");
          break;
        }
        case DI: {
          fprintf(f, "%%rdi");
          break;
        }
        case SI: {
          fprintf(f, "%%rsi");
          break;
        }
        case DX: {
          fprintf(f, "%%rdx");
          break;
        }
        case CX: {
          fprintf(f, "%%rcx");
          break;
        }
        case R8: {
          fprintf(f, "%%r8");
          break;
        }
        case R9: {
          fprintf(f, "%%r9");
          break;
        }
        case R10: {
          fprintf(f, "%%r10");
          break;
        }
        case BP: {
          fprintf(f, "%%rbp");
          break;
        }
        case SP: {
          fprintf(f, "%%rsp");
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

void emit(struct AsmProgram *prog)
{
  FILE *f;

  f = fopen("spam.s", "w");

  for (int i = 0; i < prog->funcs.len; i++) {
    fprintf(f, ".global %s\n", prog->funcs.data[i].name);
    fprintf(f, "%s:\n", prog->funcs.data[i].name);
    for (int j = 0; j < prog->funcs.data[i].body.len; j++) {
      struct AsmInstr *instr = &prog->funcs.data[i].body.data[j];
      fprintf(f, "\t");
      switch (instr->kind) {
        case AsmInstr_CALL: {
          fprintf(f, "call %s\n", instr->as.call.target);
          break;
        }
        case AsmInstr_POP: {
          fprintf(f, "popq ");
          emit_operand(f, &instr->as.pop.op);
          fprintf(f, "\n");
          break;
        }
        case AsmInstr_PUSH: {
          fprintf(f, "pushq ");
          emit_operand(f, &instr->as.push.op);
          fprintf(f, "\n");
          break;
        }
        case AsmInstr_MOV: {
          fprintf(f, "movq ");
          emit_operand(f, &instr->as.mov.src);
          fprintf(f, ", ");
          emit_operand(f, &instr->as.mov.dst);
          fprintf(f, "\n");
          break;
        }
        case AsmInstr_BINARY: {
          switch (instr->as.binary.kind) {
            case AsmInstrBinary_ADD: {
              fprintf(f, "addq ");
              break;
            }
            case AsmInstrBinary_SUB: {
              fprintf(f, "subq ");
              break;
            }
            case AsmInstrBinary_MUL: {
              fprintf(f, "imulq ");
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
      // gcc -c path -o out_path
      execlp("gcc", "gcc", "-c", path, "-o", out_path, NULL);
    } else {
      // gcc path -o out_path
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

void insert_symbol(struct Symbol **sym, char *name, Type type)
{
  struct Symbol *node;

  node = malloc(sizeof(struct Symbol));

  node->name = name;
  node->type = type;
  node->next = *sym;

  *sym = node;
}

void free_type(Type *t)
{
  switch (t->kind) {
    case I8_T:
    case I16_T:
    case I32_T:
    case I64_T:
    case STR_T:
    case UNKNOWN_T:
      break;
    case FN_T: {
      vec_free(&t->as.func.params);
      break;
    }
  }
}

struct Symbol *lookup_symbol(struct Symbol *sym, char *name)
{
  while (sym) {
    if (strcmp(sym->name, name) == 0) {
      return sym;
    }
    sym = sym->next;
  }
  return NULL;
}

void print_type(Type *type)
{
  switch (type->kind) {
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
    case STR_T:
      printf("str");
      break;
    case FN_T:
      printf("fn\n");
      printf("args: [\n");
      for (int i = 0; i < type->as.func.params.len; i++) {
        print_type(&type->as.func.params.data[i]);
      }
      printf("retval: ");
      print_type(type->as.func.retval);
      printf(")");
      break;
    case UNKNOWN_T:
      printf("uknown");
      break;
    default:
      assert(0);
  }
}

void print_symbol(struct Symbol *sym)
{
  printf("Symbol(");
  printf("name: %s,", sym->name);
  printf("type: ");
  print_type(&sym->type);
  printf(")");
}

struct TypecheckResult typecheck_expr(struct Expr *expr,
                                      struct Symbol *sym_table)
{
  struct TypecheckResult res = {.is_ok = true, .msg = NULL, .ast = NULL};

  switch (expr->kind) {
    case EXPR_LITERAL: {
      if (expr->as.literal.kind == LITERAL_NUM) {
        expr->type = (Type){.kind = I64_T};
      } else {
        expr->type = (Type){.kind = STR_T};
      }
      break;
    }
    case EXPR_VARIABLE: {
      struct Symbol *sym = lookup_symbol(sym_table, expr->as.var.name);

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

      if (expr->as.binary.lhs->type.kind != I64_T ||
          expr->as.binary.rhs->type.kind != I64_T) {
        return (struct TypecheckResult){
            .is_ok = false,
            .msg = "Binary operations require i64 operands",
            .ast = NULL};
      }

      expr->type = (Type){.kind = I64_T};
      break;
    }
    case EXPR_CALL: {
      struct Symbol *callee_sym =
          lookup_symbol(sym_table, expr->as.call.target->as.var.name);

      if (!callee_sym) {
        return (struct TypecheckResult){
            .is_ok = false, .msg = "Called an undefined function", .ast = NULL};
      }

      if (callee_sym->type.as.func.params.len != expr->as.call.arguments.len) {
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

        if (!types_equal(expr->as.call.arguments.data[i].type,
                         callee_sym->type.as.func.params.data[i])) {
          return (struct TypecheckResult){
              .is_ok = false,
              .msg = "Called with an arg of wrong type",
              .ast = NULL};
        }
      }

      expr->type = *callee_sym->type.as.func.retval;

      break;
    }
  }

  return res;
}

void free_symbol(struct Symbol *sym)
{
  free_type(&sym->type);
  free(sym);
}

struct TypecheckResult typecheck_stmt(struct Stmt *stmt,
                                      struct Symbol **sym_table)
{
  struct TypecheckResult res = {.is_ok = true, .msg = NULL, .ast = NULL};

  switch (stmt->kind) {
    case STMT_FN: {
      if (sym_table) {
        Type t;

        VecType types = {0};
        for (int i = 0; i < stmt->as.fn.params.len; i++) {
          vec_insert(&types, stmt->as.fn.params.data[i].type);
        }

        t.kind = FN_T;
        t.as.func.retval = &stmt->as.fn.retval;
        t.as.func.params = types;

        insert_symbol(sym_table, stmt->as.fn.name, t);
      }

      struct Symbol *fn_sym_table = sym_table ? *sym_table : NULL;
      struct Symbol *outer_sym = fn_sym_table;

      for (int i = 0; i < stmt->as.fn.params.len; i++) {
        insert_symbol(&fn_sym_table, stmt->as.fn.params.data[i].name,
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

      if (stmt->as.let.type.kind != stmt->as.let.init->type.kind) {
        return (struct TypecheckResult){
            .is_ok = false,
            .msg = "Type mismatch in let statement assignment",
            .ast = NULL};
      }

      insert_symbol(sym_table, stmt->as.let.name, stmt->as.let.type);
      break;
    }
    case STMT_RET: {
      if (stmt->as.ret.val) {
        res = typecheck_expr(stmt->as.ret.val, *sym_table);
        if (!res.is_ok) {
          return res;
        }

        if (stmt->as.ret.val->type.kind != stmt->as.ret.expected_retval.kind) {
          return (struct TypecheckResult){
              .is_ok = false,
              .msg = "Return value type does not match function signature",
              .ast = NULL};
        }
      } else {
        if (stmt->as.ret.expected_retval.kind != UNKNOWN_T) {
          return (struct TypecheckResult){
              .is_ok = false,
              .msg = "Missing return value in non-void function",
              .ast = NULL};
        }
      }
      break;
    }
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
  char *unique_name;
  bool current_scope;
};

struct VariableMap {
  struct VariableMap *next;
  char *name;
  struct Variable value;
};

void insert_var_into_varmap(struct VariableMap **varmap, char *name,
                            char *uniq_name, bool current_scope)
{
  struct Variable v;
  struct VariableMap *node;

  v.unique_name = uniq_name;
  v.current_scope = current_scope;

  node = malloc(sizeof(struct VariableMap));

  node->name = malloc(strlen(name) + 1);
  strcpy(node->name, name);

  node->value = v;
  node->next = *varmap;

  *varmap = node;
}

char *lookup_varmap(struct VariableMap *varmap, char *name)
{
  while (varmap) {
    if (strcmp(varmap->name, name) == 0) {
      return varmap->value.unique_name;
    }
    varmap = varmap->next;
  }
  return NULL;
}

struct ResolveResult resolve_expr(struct VariableMap **varmap,
                                  struct Expr *expr)
{
  switch (expr->kind) {
    case EXPR_LITERAL:
      break;
    case EXPR_VARIABLE: {
      char *resolved_name = lookup_varmap(*varmap, expr->as.var.name);
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
    default:
      assert(0);
  }
  return (struct ResolveResult){.is_ok = true, .msg = NULL, .as.expr = expr};
}

char *mkuniq(char *s)
{
  int digit_len, total_len, i;
  char *uniq;

  i = mktmp();

  digit_len = snprintf(NULL, 0, "%d", i);
  total_len = strlen("var.") + strlen(s) + strlen(".") + digit_len + 1;

  uniq = malloc(total_len);
  snprintf(uniq, total_len, "var.%s.%d", s, i);

  return uniq;
}

struct ResolveResult resolve_param(struct VariableMap **varmap,
                                   struct Parameter *param)
{
  int digit_len, total_len, i;
  char *uniq_name;

  uniq_name = mkuniq(param->name);

  insert_var_into_varmap(varmap, param->name, uniq_name, true);

  free(param->name);
  char *new_name = malloc(strlen(uniq_name) + 1);
  strcpy(new_name, uniq_name);
  param->name = new_name;

  return (struct ResolveResult){.is_ok = true, .msg = NULL, .as.param = param};
}

struct ResolveResult resolve_stmt(struct VariableMap **varmap,
                                  struct Stmt *stmt)
{
  switch (stmt->kind) {
    case STMT_FN: {
      struct VariableMap *variable_map, *outer_map;
      char *cpy;

      variable_map = varmap ? *varmap : NULL;
      outer_map = variable_map;

      cpy = malloc(strlen(stmt->as.fn.name) + 1);
      strcpy(cpy, stmt->as.fn.name);

      if (varmap) {
        insert_var_into_varmap(varmap, stmt->as.fn.name, cpy, true);
      }

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
        free(tmp->value.unique_name);
        free(tmp);
      }

      break;
    }
    case STMT_BLOCK: {
      for (int i = 0; i < stmt->as.block.stmts.len; i++) {
        struct ResolveResult r;

        r = resolve_stmt(varmap, &stmt->as.block.stmts.data[i]);
        if (!r.is_ok) {
          return r;
        }
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

      insert_var_into_varmap(varmap, stmt->as.let.name, uniq_name, true);

      free(stmt->as.let.name);
      char *new_name = malloc(strlen(uniq_name) + 1);
      strcpy(new_name, uniq_name);
      stmt->as.let.name = new_name;

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
        free(tmp->value.unique_name);
        free(tmp);
      }
      return r;
    }
  }

  while (global_map) {
    struct VariableMap *tmp = global_map;
    global_map = global_map->next;
    free(tmp->name);
    free(tmp->value.unique_name);
    free(tmp);
  }

  return (struct ResolveResult){.is_ok = true, .msg = NULL, .as.ast = ast};
}

typedef enum {
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
} TargetStage;

typedef struct {
  TargetStage target_stage;
  const char *path;
} CompilerOptions;

CompilerOptions parse_args(int argc, char **argv)
{
  CompilerOptions opts;
  opts.target_stage = STAGE_FULL;
  opts.path = "spam.x";

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

int main(int argc, char **argv)
{
  CompilerOptions opts = parse_args(argc, argv);
  TargetStage target_stage = opts.target_stage;
  const char *path = opts.path;

  struct ReadFileResult read_file_result;
  char *src;
  struct Tokenizer tokenizer;
  struct TokenizeResult tokenize_result;
  VecToken tokens;
  struct Parser parser;
  struct ParseResult parse_result;
  struct AST *ast, *resolved_ast, *typechecked_ast;
  struct ResolveResult resolve_result;
  struct TypecheckResult typecheck_result;
  struct IrfyResult irfy_result;
  struct IRProgram ir_prog;
  struct AsmResult asm_result;
  struct AsmProgram asm_prog;

  read_file_result = read_file(path);
  if (!read_file_result.is_ok) {
    fprintf(stderr, "Couldn't read file: %s\n", path);
    return 1;
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

  if (target_stage == STAGE_TOKENIZE) {
    goto free_up2_tokenize;
  }

  init_parser(&parser, &tokens);
  parse_result = parse(&parser);

  if (!parse_result.is_ok) {
    fprintf(stderr, "err: %s\n", parse_result.msg);
    goto free_up2_parse;
  }

  ast = parse_result.ast;
  print_ast(ast);

  if (target_stage == STAGE_PARSE) {
    goto free_up2_parse;
  }

  resolve_result = resolve(ast);
  if (!resolve_result.is_ok) {
    fprintf(stderr, "err: %s\n", resolve_result.msg);
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
    fprintf(stderr, "err: %s\n", typecheck_result.msg);
    goto free_up2_parse;
  }

  if (target_stage == STAGE_TYPECHECK) {
    goto free_up2_parse;
  }

  irfy_result = irfy_ast(ast);
  if (!irfy_result.is_ok) {
    fprintf(stderr, "err: %s\n", irfy_result.msg);
    goto free_up2_irfy;
  }

  ir_prog = irfy_result.prog;
  print_ir(&ir_prog);

  if (target_stage == STAGE_IR) {
    goto free_up2_irfy;
  }

  asm_result = codegen(&ir_prog);
  if (!asm_result.is_ok) {
    fprintf(stderr, "err: %s\n", asm_result.msg);
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

  emit(&asm_prog);
  if (target_stage == STAGE_EMIT) {
    goto free_up2_asm;
  }

  if (target_stage == STAGE_ASM) {
    assemble_and_link("spam.s", "spam.o", true);
  } else if (target_stage == STAGE_LINK || target_stage == STAGE_FULL) {
    assemble_and_link("spam.s", "spam", false);
  }

free_up2_asm:
  free_asm(&asm_result.prog);

free_up2_irfy:
  free_ir_prog(&irfy_result.prog);

free_up2_parse:
  free_ast(parse_result.ast);

free_up2_tokenize:
  vec_free(&tokenize_result.tokens);

free_up2_fread:
  free(read_file_result.contents);
  return 0;
}
