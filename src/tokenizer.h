#ifndef X_TOKENIZER_H
#define X_TOKENIZER_H

#include <stdbool.h>

#include "vector.h"

enum TokenKind {
  /* keywords */
  TOKEN_FN,
  TOKEN_LET,
  TOKEN_MUT,
  TOKEN_AS,
  TOKEN_IF,
  TOKEN_ELSE,
  TOKEN_DO,
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

struct TokenizeResult {
  bool is_ok;
  char *msg;
  VecToken tokens;
};

void init_tokenizer(struct Tokenizer *tokenizer, char *src);
void print_token(struct Token *token);
void print_tokens(VecToken *tokens);
struct TokenizeResult tokenize(struct Tokenizer *tokenizer);

#endif
