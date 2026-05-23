#ifndef MINI_COMPILER_RESOLVER_H
#define MINI_COMPILER_RESOLVER_H

#include "common.h"

/* resolver */

void varmap_insert(struct VariableMap **varmap, char *name, char *uniq_name);
char *varmap_get(struct VariableMap *varmap, char *name);
struct ResolveResult resolve_expr(struct VariableMap **varmap,
                                  struct Expr *expr);
struct ResolveResult resolve_param(struct VariableMap **varmap,
                                   struct Parameter *param);
struct ResolveResult resolve_stmt(struct VariableMap **varmap,
                                  struct Stmt *stmt);
struct ResolveResult resolve(struct AST *ast);

#endif /* MINI_COMPILER_RESOLVER_H */
