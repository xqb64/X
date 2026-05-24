#ifndef X_RESOLVER_H
#define X_RESOLVER_H

#include <stdbool.h>

#include "parser.h"

struct ResolveResult {
  bool is_ok;
  char *msg;
  union {
    struct AST *ast;
    struct Stmt *stmt;
    struct Expr *expr;
    struct Parameter *param;
    struct Decl *decl;
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

struct ResolveResult resolve(struct AST *ast);

#endif
