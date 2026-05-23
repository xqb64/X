#ifndef MINI_COMPILER_TOKENIZER_H
#define MINI_COMPILER_TOKENIZER_H

#include "common.h"

/* tokenizer */

void init_tokenizer(struct Tokenizer *tokenizer, char *src);
bool is_alpha(char c);
bool is_digit(char c);
bool is_space(char c);
bool is_underscore(char c);
bool is_dot(char c);
bool is_at_end(struct Tokenizer *tokenizer);
void advance(struct Tokenizer *tokenizer);
int lookahead(struct Tokenizer *tokenizer, int n, char *target);
struct Token mktoken(struct Tokenizer *tokenizer, enum TokenKind kind, int len);
struct Token number(struct Tokenizer *tokenizer);
struct Token identifier(struct Tokenizer *tokenizer);
struct Token string(struct Tokenizer *tokenizer);
struct TokenizeResult tokenize(struct Tokenizer *tokenizer);
void print_token(struct Token *token);
void print_tokens(VecToken *tokens);

#endif /* MINI_COMPILER_TOKENIZER_H */
