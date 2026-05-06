#include <assert.h>
#include <stdarg.h>
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
  TOKEN_MINUS,
  TOKEN_STAR,
  TOKEN_SLASH,
  TOKEN_SEMICOLON,
  TOKEN_ARROW,
  TOKEN_I32,
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
      case 'i': {
        if (lookahead(tokenizer, 2, "32") == 0) {
          vec_insert(&tokens, mktoken(tokenizer, TOKEN_I32, 3));
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
      case ';': {
        vec_insert(&tokens, mktoken(tokenizer, TOKEN_SEMICOLON, 1));
        break;
      }
      default:

        printf("tokenizer->src: %s\n", tokenizer->src);

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
    case TOKEN_MINUS:
      printf("minus");
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
    case TOKEN_ARROW:
      printf("arrow");
      break;
    case TOKEN_I32:
      printf("i32");
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

enum LiteralKind {
  LITERAL_NUM,
  LITERAL_STR,
};

struct Literal {
  enum LiteralKind kind;
  union {
    char *str;
    int num;
  } as;
};

enum ExprKind {
  EXPR_LITERAL,
  EXPR_BINARY,
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

struct Expr {
  enum ExprKind kind;
  union {
    struct Literal literal;
    struct ExprBin binary;
  } as;
};

typedef Vector(struct Stmt) VecStmt;

struct StmtRet {
  struct Expr *val;
};

struct StmtBlock {
  VecStmt stmts;
};

enum StmtKind {
  STMT_FN,
  STMT_BLOCK,
  STMT_RET,
};

struct StmtFn {
  char *name;
  char **params;
  char *retval;
  VecStmt body;
};

struct Stmt {
  enum StmtKind kind;
  union {
    struct StmtRet ret;
    struct StmtFn fn;
    struct StmtBlock block;
  } as;
};

struct AST {
  VecStmt stmts;
};

struct ParseResult {
  bool is_ok;
  char *msg;
  struct AST *ast;
};

struct Parser {
  struct Token *curr;
  struct Token *prev;
  VecToken *tokens;
  int idx;
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
  char *s = malloc(strlen(string) + 1);
  snprintf(s, n + 1, "%s", string);
  return s;
}

struct ParseFnResult parse_fn_stmt(struct Parser *parser)
{
  struct ParseFnResult result;
  struct Token *token_fn, *token_id, *token_lparen, *token_void, *token_rparen,
      *token_arrow, *token_retval, *token_lbrace, *token_rbrace;

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

  token_void = consume(parser, TOKEN_VOID);
  if (!token_void) {
    return (struct ParseFnResult){.is_ok = false,
                                  .as.stmt = {0},
                                  .msg = "Expected token 'void' after '('"};
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

  token_retval = consume(parser, TOKEN_I32);
  if (!token_retval) {
    return (struct ParseFnResult){.is_ok = false,
                                  .as.stmt = {0},
                                  .msg = "Expected token 'i32' after '->'"};
  }

  token_lbrace = consume(parser, TOKEN_LBRACE);
  if (!token_lbrace) {
    return (struct ParseFnResult){.is_ok = false,
                                  .as.stmt = {0},
                                  .msg = "Expected token 'void' after '('"};
  }

  struct ParseFnResult block_result = block(parser);
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

  struct StmtFn stmt_fn;
  stmt_fn.body = body.as.block.stmts;
  stmt_fn.name = own_string_n(token_id->start, token_id->len);
  stmt_fn.params = NULL;
  stmt_fn.retval = own_string_n(token_retval->start, token_retval->len);

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

#define ALLOC(obj) (memcpy(malloc(sizeof((obj))), &(obj), sizeof(obj)))

struct ParseFnResult parse_literal(struct Parser *parser)
{
  struct ParseFnResult res;
  struct Literal literal;

  res.is_ok = true;
  res.msg = NULL;

  if (!consume(parser, TOKEN_NUMBER)) {
    res.is_ok = false;
    res.msg = "Expected number";
    return res;
  }

  literal.kind = LITERAL_NUM;
  literal.as.num = strtol(parser->prev->start, NULL, 10);

  res.as.expr = (struct Expr){.kind = EXPR_LITERAL, .as.literal = literal};

  return res;
}

struct ParseFnResult factor(struct Parser *parser)
{
  struct ParseFnResult left_res, right_res;
  struct Expr left, right;

  left_res = parse_literal(parser);
  if (!left_res.is_ok) {
    return left_res;
  }

  left = left_res.as.expr;
  while (match(parser, 2, TOKEN_STAR, TOKEN_SLASH)) {
    char *op = parser->prev->start;

    right_res = parse_literal(parser);
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

  struct Stmt s;
  s.kind = STMT_RET;
  s.as.ret = ret_stmt;

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

void print_expr(struct Expr *expr)
{
  switch (expr->kind) {
    case EXPR_LITERAL: {
      printf("Literal(%d)", expr->as.literal.as.num);
      break;
    }
    case EXPR_BINARY: {
      printf("Binary(lhs = ");
      print_expr(expr->as.binary.lhs);
      printf(", rhs = ");
      print_expr(expr->as.binary.rhs);

      switch (expr->as.binary.kind) {
        case EXPR_BIN_ADD: {
          printf(", kind = ADD)");
          break;
        }
        case EXPR_BIN_SUB: {
          printf(", kind = SUB)");
          break;
        }
        case EXPR_BIN_MUL: {
          printf(", kind = MUL)");
          break;
        }
        case EXPR_BIN_DIV: {
          printf(", kind = DIV)");
          break;
        }
      }
    }
  }
}

void print_stmt(struct Stmt *stmt)
{
  switch (stmt->kind) {
    case STMT_FN: {
      printf("STMT_FN(name = %s, ", stmt->as.fn.name);
      for (int i = 0; i < stmt->as.fn.body.len; i++) {
        print_stmt(&stmt->as.fn.body.data[i]);
      }
      printf(", retval = %s)\n", stmt->as.fn.retval);
      break;
    }
    case STMT_BLOCK: {
      for (int i = 0; i < stmt->as.block.stmts.len; i++) {
        print_stmt(&stmt->as.block.stmts.data[i]);
      }
      break;
    }
    case STMT_RET: {
      printf("STMT_RET(val = ");
      if (stmt->as.ret.val) {
        print_expr(stmt->as.ret.val);
      }
      printf("\n");
      break;
    }
    default:
      assert(0);
  }
}

void print_ast(struct AST *ast)
{
  for (int i = 0; i < ast->stmts.len; i++) {
    print_stmt(&ast->stmts.data[i]);
  }
}

void free_expr(struct Expr *expr)
{
  switch (expr->kind) {
    case EXPR_LITERAL: {
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
    case STMT_FN: {
      free(stmt->as.fn.name);
      free(stmt->as.fn.params);
      free(stmt->as.fn.retval);
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
      }
      break;
    }
  }
}

enum IRInstrKind {
  IRInstr_BINARY,
  IRInstr_RET,
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
    int konst;
  } as;
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

struct IRInstr {
  enum IRInstrKind kind;
  union {
    struct IRInstr_Binary binary;
    struct IRInstr_Ret ret;
  } as;
};

typedef Vector(struct IRInstr) VecIRInstr;

struct IRFunction {
  char *name;
  char **params;
  char *retval;
  VecIRInstr body;
};

typedef Vector(struct IRFunction *) VecIRFunctionPtr;

struct IRProgram {
  VecIRFunctionPtr funcs;
};

struct IRValue *make_ir_var(void)
{
  static int i = 0;
  struct IRValue *var;

  var = malloc(sizeof(struct IRValue));
  memset(var, 0, sizeof(struct IRValue));

  var->kind = IRValue_VAR;

  int digit_len = snprintf(NULL, 0, "%d", i);
  int total_len = strlen("tmp") + strlen(".") + digit_len + 1;
  var->as.var = malloc(total_len);

  snprintf(var->as.var, total_len, "tmp.%d", i);

  i++;

  return var;
}

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

void print_ir_val(struct IRValue *ir_val)
{
  switch (ir_val->kind) {
    case IRValue_CONST: {
      printf("IRValue(type = CONST, value = %d)", ir_val->as.konst);
      break;
    }
    case IRValue_VAR: {
      printf("IRValue(type = VAR, name = %s)", ir_val->as.var);
      break;
    }
  }
}

void print_ir_instr(struct IRInstr *instr)
{
  switch (instr->kind) {
    case IRInstr_BINARY: {
      printf("IRInstr_BINARY(type = ");
      switch (instr->as.binary.kind) {
        case IRInstrBinary_ADD: {
          printf("ADD");
          break;
        }
        case IRInstrBinary_SUB: {
          printf("SUB");
          break;
        }
        case IRInstrBinary_MUL: {
          printf("MUL");
          break;
        }
        case IRInstrBinary_DIV: {
          printf("DIV");
          break;
        }
      }
      printf(", lhs = ");
      print_ir_val(instr->as.binary.lhs);
      printf(", rhs = ");
      print_ir_val(instr->as.binary.rhs);
      printf(", dst = ");
      print_ir_val(instr->as.binary.dst);

      break;
    }
    case IRInstr_RET: {
      printf("IRInstr_RET(val = ");
      if (instr->as.ret.val) {
        print_ir_val(instr->as.ret.val);
      }
      break;
    }
  }
}

void print_ir_fn(struct IRFunction *func)
{
  printf("IRFunction(name = %s, retval = %s", func->name, func->retval);
  for (int i = 0; i < func->body.len; i++) {
    print_ir_instr(&func->body.data[i]);
  }
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
};

enum AsmOperandKind {
  AsmOperand_IMM,
  AsmOperand_PSEUDO,
  AsmOperand_REG,
  AsmOperand_STACK,
};

enum AsmRegister {
  AX,
  R10,
  BP,
  SP,
};

struct AsmOperand {
  enum AsmOperandKind kind;
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
  struct AsmInstrMov mov1, mov2;
  struct AsmInstrBinary sub;

  func.name = ir_func->name;

  push.op = (struct AsmOperand){.kind = AsmOperand_REG, .as.reg = BP};

  mov1.src = (struct AsmOperand){.kind = AsmOperand_REG, .as.reg = SP};
  mov1.dst = (struct AsmOperand){.kind = AsmOperand_REG, .as.reg = BP};

  sub.kind = AsmInstrBinary_SUB;
  sub.lhs = (struct AsmOperand){.kind = AsmOperand_IMM, .as.imm = 0};
  sub.rhs = (struct AsmOperand){.kind = AsmOperand_REG, .as.reg = SP};

  p1.kind = AsmInstr_PUSH;
  p1.as.push = push;

  p2.kind = AsmInstr_MOV;
  p2.as.mov = mov1;

  p3.kind = AsmInstr_BINARY;
  p3.as.binary = sub;

  vec_insert(&func.body, p1);
  vec_insert(&func.body, p2);
  vec_insert(&func.body, p3);

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
      printf("AsmInstr_RET");
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
          } else {
            vec_insert(&instrs, *asminstr);
          }
          break;
        }
        default:
          vec_insert(&instrs, *asminstr);
          break;
      }
    }

    prog->funcs.data[i].body = instrs;
  }

  return prog;
}

void emit_operand(struct AsmOperand *op)
{
  switch (op->kind) {
    case AsmOperand_IMM: {
      printf("$%d", op->as.imm);
      break;
    }
    case AsmOperand_PSEUDO: {
      assert(0 && "not implemented");
      break;
    }
    case AsmOperand_REG: {
      switch (op->as.reg) {
        case AX: {
          printf("%%rax");
          break;
        }
        case R10: {
          printf("%%r10");
          break;
        }
        case BP: {
          printf("%%rbp");
          break;
        }
        case SP: {
          printf("%%rsp");
          break;
        }
      }
      break;
    }
    case AsmOperand_STACK: {
      printf("%d(%%rbp)", op->as.stack_offset);
      break;
    }
  }
}

void emit(struct AsmProgram *prog)
{
  for (int i = 0; i < prog->funcs.len; i++) {
    for (int j = 0; j < prog->funcs.data[i].body.len; j++) {
      struct AsmInstr *instr = &prog->funcs.data[i].body.data[j];
      switch (instr->kind) {
        case AsmInstr_POP: {
          printf("popq ");
          emit_operand(&instr->as.pop.op);
          printf("\n");
          break;
        }
        case AsmInstr_PUSH: {
          printf("pushq ");
          emit_operand(&instr->as.push.op);
          printf("\n");
          break;
        }
        case AsmInstr_MOV: {
          printf("movq ");
          emit_operand(&instr->as.mov.src);
          printf(", ");
          emit_operand(&instr->as.mov.dst);
          printf("\n");
          break;
        }
        case AsmInstr_BINARY: {
          switch (instr->as.binary.kind) {
            case AsmInstrBinary_ADD: {
              printf("addq ");
              break;
            }
            case AsmInstrBinary_SUB: {
              printf("subq ");
              break;
            }
            case AsmInstrBinary_MUL: {
              printf("imulq ");
              break;
            }
            default:
              assert(0 && "not implemented");
              break;
          }
          emit_operand(&instr->as.binary.lhs);
          printf(", ");
          emit_operand(&instr->as.binary.rhs);
          printf("\n");
          break;
        }
        case AsmInstr_RET: {
          printf("ret\n");
          break;
        }
        default:
          break;
      }
    }
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
  struct Parser parser;
  struct ParseResult parse_result;
  struct AST *ast;
  struct IrfyResult irfy_result;
  struct IRProgram ir_prog;
  struct AsmResult asm_result;
  struct AsmProgram asm_prog;

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

  init_parser(&parser, &tokens);

  parse_result = parse(&parser);
  if (!parse_result.is_ok) {
    fprintf(stderr, "err: %s\n", parse_result.msg);
    goto free_up2_parse;
  }

  ast = parse_result.ast;
  print_ast(ast);

  irfy_result = irfy_ast(ast);
  if (!irfy_result.is_ok) {
    fprintf(stderr, "err: %s\n", irfy_result.msg);
    goto free_up2_irfy;
  }

  ir_prog = irfy_result.prog;
  print_ir(&ir_prog);

  asm_result = codegen(&ir_prog);
  if (!asm_result.is_ok) {
    fprintf(stderr, "err: %s\n", asm_result.msg);
    goto free_up2_asm;
  }

  asm_prog = asm_result.prog;
  print_asm(&asm_prog);

  printf("replacing pseudo...");
  asm_prog = *replace_pseudo(&asm_prog);
  print_asm(&asm_prog);

  printf("fixup...");
  asm_prog = *fixup(&asm_prog);
  print_asm(&asm_prog);

  emit(&asm_prog);

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
