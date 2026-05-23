#ifndef MINI_COMPILER_PARSER_H
#define MINI_COMPILER_PARSER_H

#include "common.h"

/* parser */

void print_binary_op(enum ExprBinKind kind);
void print_expr(struct Expr *expr, int spaces);
void free_expr(struct Expr *expr);
void print_stmt(struct Stmt *stmt, int spaces);
void free_stmt(struct Stmt *stmt);
void print_ast(struct AST *ast);
void free_ast(struct AST *ast);
void init_parser(struct Parser *parser, VecToken *tokens);
struct Token *next_token(struct Parser *parser);
bool check(struct Parser *parser, enum TokenKind kind);
bool check2(struct Parser *parser, enum TokenKind kind1, enum TokenKind kind2);
bool check_next(struct Parser *parser, enum TokenKind kind);
bool match(struct Parser *parser, int size, ...);
struct Token *consume(struct Parser *parser, enum TokenKind kind);
struct Token *consume_any(struct Parser *parser, int n, ...);
struct ParseFnResult primary(struct Parser *parser);
struct ParseFnResult finish_call(struct Parser *parser, struct Expr callee);
bool parse_enum_body(struct Parser *parser, VecEnumVariant *out_variants);
Type parse_type(struct Parser *parser);
struct ParseFnResult postfix(struct Parser *parser);
struct ParseFnResult unary(struct Parser *parser);
struct ParseFnResult factor(struct Parser *parser);
struct ParseFnResult term(struct Parser *parser);
struct ParseFnResult shift(struct Parser *parser);
struct ParseFnResult comparison(struct Parser *parser);
struct ParseFnResult bitwise_and(struct Parser *parser);
struct ParseFnResult bitwise_xor(struct Parser *parser);
struct ParseFnResult bitwise_or(struct Parser *parser);
struct ParseFnResult logical_and(struct Parser *parser);
struct ParseFnResult logical_or(struct Parser *parser);
struct ParseFnResult assignment(struct Parser *parser);
struct ParseFnResult block(struct Parser *parser);
struct ParseFnResult parse_fn_stmt(struct Parser *parser);
struct ParseFnResult parse_let_stmt(struct Parser *parser);
struct ParseFnResult parse_ret_stmt(struct Parser *parser);
struct ParseFnResult parse_if_stmt(struct Parser *parser);
struct ParseFnResult parse_while_stmt(struct Parser *parser);
struct ParseFnResult parse_loop_stmt(struct Parser *parser);
struct ParseFnResult parse_break_stmt(struct Parser *parser);
struct ParseFnResult parse_continue_stmt(struct Parser *parser);
struct ParseFnResult parse_extern_stmt(struct Parser *parser);
struct ParseFnResult parse_expr_stmt(struct Parser *parser);
struct ParseFnResult parse_struct_stmt(struct Parser *parser);
struct ParseFnResult parse_goto_stmt(struct Parser *parser);
struct ParseFnResult parse_enum_stmt(struct Parser *parser);
struct ParseResult parse(struct Parser *parser);

#endif /* MINI_COMPILER_PARSER_H */
