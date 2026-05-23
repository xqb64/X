#include "tokenizer.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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

static bool is_at_end(struct Tokenizer *tokenizer)
{
  return *tokenizer->src == '\0';
}

static void advance(struct Tokenizer *tokenizer)
{
  tokenizer->src++;
}

static int lookahead(struct Tokenizer *tokenizer, int n, char *target)
{
  return memcmp(tokenizer->src + 1, target, n);
}

static struct Token mktoken(struct Tokenizer *tokenizer, enum TokenKind kind, int len)
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

static struct Token identifier(struct Tokenizer *tokenizer)
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

static struct Token string(struct Tokenizer *tokenizer)
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

void init_tokenizer(struct Tokenizer *tokenizer, char *src)
{
  tokenizer->src = src;
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

