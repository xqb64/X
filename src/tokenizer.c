#include "tokenizer.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static bool is_alpha(char c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool is_digit(char c)
{
  return c >= '0' && c <= '9';
}

static bool is_space(char c)
{
  return c == ' ' || c == '\t' || c == '\n';
}

static bool is_underscore(char c)
{
  return c == '_';
}

static bool is_dot(char c)
{
  return c == '.';
}

static bool is_ident_char(char c)
{
  return is_alpha(c) || is_digit(c) || is_underscore(c);
}

static bool is_at_end(struct Tokenizer *tokenizer)
{
  return *tokenizer->src == '\0';
}

static void advance(struct Tokenizer *tokenizer)
{
  tokenizer->src++;
}

static int lookahead(struct Tokenizer *tokenizer, int n, const char *target)
{
  return memcmp(tokenizer->src + 1, target, n);
}

static bool match_keyword(struct Tokenizer *tokenizer, int rest_len,
                          const char *rest)
{
  if (lookahead(tokenizer, rest_len, rest) == 0) {
    if (!is_ident_char(tokenizer->src[rest_len + 1])) {
      return true;
    }
  }
  return false;
}

static struct Token mktoken(struct Tokenizer *tokenizer, enum TokenKind kind,
                            int len)
{
  struct Token token;

  token.kind = kind;
  token.start = tokenizer->src;
  token.len = len;

  tokenizer->src += len;

  return token;
}

static struct Token number(struct Tokenizer *tokenizer)
{
  int len = 0;
  char *start = tokenizer->src;
  bool is_float = false;

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

static struct Token identifier(struct Tokenizer *tokenizer)
{
  int len = 0;
  char *start = tokenizer->src;

  while (is_ident_char(*tokenizer->src)) {
    len++;
    advance(tokenizer);
  }

  return (struct Token){.kind = TOKEN_IDENTIFIER, .len = len, .start = start};
}

static struct Token string(struct Tokenizer *tokenizer)
{
  advance(tokenizer);

  int len = 0;
  char *start = tokenizer->src;

  while (*tokenizer->src != '"') {
    if (is_at_end(tokenizer)) {
      return (struct Token){.kind = TOKEN_ERROR, .len = len, .start = start};
    }
    len++;
    advance(tokenizer);
  }

  advance(tokenizer);

  return (struct Token){.kind = TOKEN_STRING, .len = len, .start = start};
}

void init_tokenizer(struct Tokenizer *tokenizer, char *src)
{
  tokenizer->src = src;
}

struct Token next_token(struct Tokenizer *tokenizer)
{
  while (is_space(*tokenizer->src)) {
    advance(tokenizer);
  }

  if (is_at_end(tokenizer)) {
    return (struct Token){.kind = TOKEN_EOF, .start = tokenizer->src, .len = 0};
  }

  if (is_digit(*tokenizer->src)) {
    return number(tokenizer);
  }

  switch (*tokenizer->src) {
    case 'a': {
      if (match_keyword(tokenizer, 1, "s")) {
        return mktoken(tokenizer, TOKEN_AS, 2);
      }
      return identifier(tokenizer);
    }
    case 'b': {
      if (match_keyword(tokenizer, 3, "ool")) {
        return mktoken(tokenizer, TOKEN_BOOL, 4);
      }
      if (match_keyword(tokenizer, 4, "reak")) {
        return mktoken(tokenizer, TOKEN_BREAK, 5);
      }
      return identifier(tokenizer);
    }
    case 'c': {
      if (match_keyword(tokenizer, 7, "ontinue")) {
        return mktoken(tokenizer, TOKEN_CONTINUE, 8);
      }
      return identifier(tokenizer);
    }
    case 'd': {
      if (match_keyword(tokenizer, 1, "o")) {
        return mktoken(tokenizer, TOKEN_DO, 2);
      }
      return identifier(tokenizer);
    }
    case 'e': {
      if (match_keyword(tokenizer, 3, "num")) {
        return mktoken(tokenizer, TOKEN_ENUM, 4);
      }
      if (match_keyword(tokenizer, 3, "lse")) {
        return mktoken(tokenizer, TOKEN_ELSE, 4);
      }
      if (match_keyword(tokenizer, 5, "xtern")) {
        return mktoken(tokenizer, TOKEN_EXTERN, 6);
      }
      return identifier(tokenizer);
    }
    case 'f': {
      if (match_keyword(tokenizer, 4, "alse")) {
        return mktoken(tokenizer, TOKEN_FALSE, 5);
      }
      if (match_keyword(tokenizer, 2, "32")) {
        return mktoken(tokenizer, TOKEN_F32, 3);
      }
      if (match_keyword(tokenizer, 2, "64")) {
        return mktoken(tokenizer, TOKEN_F64, 3);
      }
      if (match_keyword(tokenizer, 1, "n")) {
        return mktoken(tokenizer, TOKEN_FN, 2);
      }
      return identifier(tokenizer);
    }
    case 'g': {
      if (match_keyword(tokenizer, 3, "oto")) {
        return mktoken(tokenizer, TOKEN_GOTO, 4);
      }
      return identifier(tokenizer);
    }
    case 'i': {
      if (match_keyword(tokenizer, 1, "8")) {
        return mktoken(tokenizer, TOKEN_I8, 2);
      }
      if (match_keyword(tokenizer, 1, "f")) {
        return mktoken(tokenizer, TOKEN_IF, 2);
      }
      if (match_keyword(tokenizer, 2, "16")) {
        return mktoken(tokenizer, TOKEN_I16, 3);
      }
      if (match_keyword(tokenizer, 2, "32")) {
        return mktoken(tokenizer, TOKEN_I32, 3);
      }
      if (match_keyword(tokenizer, 2, "64")) {
        return mktoken(tokenizer, TOKEN_I64, 3);
      }
      return identifier(tokenizer);
    }
    case 'l': {
      if (match_keyword(tokenizer, 2, "et")) {
        return mktoken(tokenizer, TOKEN_LET, 3);
      }
      if (match_keyword(tokenizer, 3, "oop")) {
        return mktoken(tokenizer, TOKEN_LOOP, 4);
      }
      return identifier(tokenizer);
    }
    case 'm': {
      if (match_keyword(tokenizer, 2, "ut")) {
        return mktoken(tokenizer, TOKEN_MUT, 3);
      }
      return identifier(tokenizer);
    }
    case 'r': {
      if (match_keyword(tokenizer, 2, "et")) {
        return mktoken(tokenizer, TOKEN_RET, 3);
      }
      return identifier(tokenizer);
    }
    case 's': {
      if (match_keyword(tokenizer, 5, "izeof")) {
        return mktoken(tokenizer, TOKEN_SIZEOF, 6);
      }
      if (match_keyword(tokenizer, 5, "truct")) {
        return mktoken(tokenizer, TOKEN_STRUCT, 6);
      }
      if (match_keyword(tokenizer, 2, "tr")) {
        return mktoken(tokenizer, TOKEN_STR, 3);
      }
      return identifier(tokenizer);
    }
    case 't': {
      if (match_keyword(tokenizer, 3, "rue")) {
        return mktoken(tokenizer, TOKEN_TRUE, 4);
      }
      return identifier(tokenizer);
    }
    case 'u': {
      if (match_keyword(tokenizer, 4, "nion")) {
        return mktoken(tokenizer, TOKEN_UNION, 5);
      }
      if (match_keyword(tokenizer, 1, "8")) {
        return mktoken(tokenizer, TOKEN_U8, 2);
      }
      if (match_keyword(tokenizer, 2, "16")) {
        return mktoken(tokenizer, TOKEN_U16, 3);
      }
      if (match_keyword(tokenizer, 2, "32")) {
        return mktoken(tokenizer, TOKEN_U32, 3);
      }
      if (match_keyword(tokenizer, 2, "64")) {
        return mktoken(tokenizer, TOKEN_U64, 3);
      }
      return identifier(tokenizer);
    }
    case 'v': {
      if (match_keyword(tokenizer, 3, "oid")) {
        return mktoken(tokenizer, TOKEN_VOID, 4);
      }
      return identifier(tokenizer);
    }
    case 'w': {
      if (match_keyword(tokenizer, 4, "hile")) {
        return mktoken(tokenizer, TOKEN_WHILE, 5);
      }
      return identifier(tokenizer);
    }
    case '+': {
      if (lookahead(tokenizer, 1, "=") == 0) {
        return mktoken(tokenizer, TOKEN_PLUS_EQUAL, 2);
      }
      return mktoken(tokenizer, TOKEN_PLUS, 1);
    }
    case '-': {
      if (lookahead(tokenizer, 1, ">") == 0) {
        return mktoken(tokenizer, TOKEN_ARROW, 2);
      }
      if (lookahead(tokenizer, 1, "=") == 0) {
        return mktoken(tokenizer, TOKEN_MINUS_EQUAL, 2);
      }
      return mktoken(tokenizer, TOKEN_MINUS, 1);
    }
    case '*': {
      if (lookahead(tokenizer, 1, "=") == 0) {
        return mktoken(tokenizer, TOKEN_STAR_EQUAL, 2);
      }
      return mktoken(tokenizer, TOKEN_STAR, 1);
    }
    case '/': {
      if (lookahead(tokenizer, 1, "=") == 0) {
        return mktoken(tokenizer, TOKEN_SLASH_EQUAL, 2);
      }
      return mktoken(tokenizer, TOKEN_SLASH, 1);
    }
    case '.': {
      if (lookahead(tokenizer, 2, "..") == 0) {
        return mktoken(tokenizer, TOKEN_ELLIPSIS, 3);
      }
      return mktoken(tokenizer, TOKEN_DOT, 1);
    }
    case '(':
      return mktoken(tokenizer, TOKEN_LPAREN, 1);
    case ')':
      return mktoken(tokenizer, TOKEN_RPAREN, 1);
    case '{':
      return mktoken(tokenizer, TOKEN_LBRACE, 1);
    case '}':
      return mktoken(tokenizer, TOKEN_RBRACE, 1);
    case '!': {
      if (lookahead(tokenizer, 1, "=") == 0) {
        return mktoken(tokenizer, TOKEN_BANG_EQUAL, 2);
      }
      return mktoken(tokenizer, TOKEN_BANG, 1);
    }
    case '=': {
      if (lookahead(tokenizer, 1, "=") == 0) {
        return mktoken(tokenizer, TOKEN_EQUAL_EQUAL, 2);
      }
      return mktoken(tokenizer, TOKEN_EQUAL, 1);
    }
    case '|': {
      if (lookahead(tokenizer, 1, "|") == 0) {
        return mktoken(tokenizer, TOKEN_PIPE_PIPE, 2);
      }
      if (lookahead(tokenizer, 1, "=") == 0) {
        return mktoken(tokenizer, TOKEN_PIPE_EQUAL, 2);
      }
      return mktoken(tokenizer, TOKEN_PIPE, 1);
    }
    case '&': {
      if (lookahead(tokenizer, 1, "&") == 0) {
        return mktoken(tokenizer, TOKEN_AMPERSAND_AMPERSAND, 2);
      }
      if (lookahead(tokenizer, 1, "=") == 0) {
        return mktoken(tokenizer, TOKEN_AMPERSAND_EQUAL, 2);
      }
      return mktoken(tokenizer, TOKEN_AMPERSAND, 1);
    }
    case '^': {
      if (lookahead(tokenizer, 1, "=") == 0) {
        return mktoken(tokenizer, TOKEN_CARET_EQUAL, 2);
      }
      return mktoken(tokenizer, TOKEN_CARET, 1);
    }
    case '~':
      return mktoken(tokenizer, TOKEN_TILDE, 1);
    case ',':
      return mktoken(tokenizer, TOKEN_COMMA, 1);
    case ':':
      return mktoken(tokenizer, TOKEN_COLON, 1);
    case ';':
      return mktoken(tokenizer, TOKEN_SEMICOLON, 1);
    case '"':
      return string(tokenizer);
    case '>': {
      if (lookahead(tokenizer, 2, ">=") == 0) {
        return mktoken(tokenizer, TOKEN_GREATER_GREATER_EQUAL, 3);
      }
      if (lookahead(tokenizer, 1, ">") == 0) {
        return mktoken(tokenizer, TOKEN_GREATER_GREATER, 2);
      }
      if (lookahead(tokenizer, 1, "=") == 0) {
        return mktoken(tokenizer, TOKEN_GREATER_EQUAL, 2);
      }
      return mktoken(tokenizer, TOKEN_GREATER, 1);
    }
    case '<': {
      if (lookahead(tokenizer, 2, "<=") == 0) {
        return mktoken(tokenizer, TOKEN_LESS_LESS_EQUAL, 3);
      }
      if (lookahead(tokenizer, 1, "<") == 0) {
        return mktoken(tokenizer, TOKEN_LESS_LESS, 2);
      }
      if (lookahead(tokenizer, 1, "=") == 0) {
        return mktoken(tokenizer, TOKEN_LESS_EQUAL, 2);
      }
      return mktoken(tokenizer, TOKEN_LESS, 1);
    }
    default: {
      if (is_ident_char(*tokenizer->src)) {
        return identifier(tokenizer);
      }
      struct Token err = {
          .kind = TOKEN_ERROR, .len = 1, .start = tokenizer->src};
      advance(tokenizer);
      return err;
    }
  }
}


#ifdef DEBUG_TOKENIZER
struct TokenizeResult tokenize(struct Tokenizer *tokenizer)
{
  VecToken tokens = {0};

  for (;;) {
    struct Token token;

    token = next_token(tokenizer);
    if (token.kind == TOKEN_ERROR) {
      return (struct TokenizeResult){
          .is_ok = false,
          .msg = "Encountered error while tokenizing",
          .tokens = {0}};
    }

    if (token.kind == TOKEN_EOF) {
      break;
    }

    vec_insert(&tokens, token);
  }

  return (struct TokenizeResult){.is_ok = true, .msg = NULL, .tokens = tokens};
}
#endif
