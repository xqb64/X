#include "resolver.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "typechecker.h"
#include "util.h"

static void varmap_insert(struct VariableMap **varmap, char *name,
                          char *uniq_name)
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

static char *varmap_get(struct VariableMap *varmap, char *name)
{
  while (varmap) {
    if (strcmp(varmap->name, name) == 0) {
      return varmap->value.uniq_name;
    }
    varmap = varmap->next;
  }
  return NULL;
}

static struct ResolveResult resolve_expr(struct VariableMap **varmap,
                                         struct Expr *expr)
{
  switch (expr->kind) {
    case EXPR_STRUCT_INIT: {
      for (int i = 0; i < expr->as.struct_init.values.len; i++) {
        struct ResolveResult r;

        r = resolve_expr(varmap, expr->as.struct_init.values.data[i].expr);
        if (!r.is_ok) {
          return r;
        }
      }
      break;
    }
    case EXPR_MEMBER: {
      struct ResolveResult r;

      r = resolve_expr(varmap, expr->as.member.target);
      if (!r.is_ok) {
        return r;
      }
      break;
    }
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
      if (resolved_name) {
        free(expr->as.var.name);
        expr->as.var.name = strdup(resolved_name);
      } else {
        int val;
        if (enum_variant_get(expr->as.var.name, &val)) {
          free(expr->as.var.name);

          expr->kind = EXPR_LITERAL;
          expr->as.literal.kind = LITERAL_NUM;
          expr->as.literal.type = (Type){.kind = I32_T};
          expr->as.literal.as.i32 = val;
          expr->type = (Type){.kind = I32_T};
        } else {
          return (struct ResolveResult){.is_ok = false,
                                        .msg = "Undefined variable"};
        }
      }
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
    case EXPR_COMPOUND_ASSIGN: {
      struct ResolveResult rlhs, rrhs;

      rlhs = resolve_expr(varmap, expr->as.compound_assign.lhs);
      if (!rlhs.is_ok) {
        return rlhs;
      }

      rrhs = resolve_expr(varmap, expr->as.compound_assign.rhs);
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
    case EXPR_SIZEOF: {
      struct ResolveResult r;

      r = resolve_expr(varmap, expr->as.sizeof_expr.expr);
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

static struct ResolveResult resolve_param(struct VariableMap **varmap,
                                          struct Parameter *param)
{
  char *uniq_name;

  uniq_name = mkuniq(param->name);
  varmap_insert(varmap, param->name, uniq_name);
  free(param->name);
  param->name = strdup(uniq_name);

  return (struct ResolveResult){.is_ok = true, .msg = NULL, .as.param = param};
}

static struct ResolveResult resolve_stmt(struct VariableMap **varmap,
                                         struct Stmt *stmt)
{
  switch (stmt->kind) {
    case STMT_LOOP: {
      struct ResolveResult r;

      r = resolve_stmt(varmap, stmt->as.loop.body);
      if (!r.is_ok) {
        return r;
      }

      break;
    }
    case STMT_LABELED: {
      struct ResolveResult r;

      r = resolve_stmt(varmap, stmt->as.labeled.stmt);
      if (!r.is_ok) {
        return r;
      }

      break;
    }
    case STMT_BREAK:
    case STMT_CONTINUE:
    case STMT_GOTO:
      break;
    case STMT_EXPR: {
      struct ResolveResult r;

      r = resolve_expr(varmap, &stmt->as.expr_stmt.expr);
      if (!r.is_ok) {
        return r;
      }
      break;
    }
    case STMT_DO_WHILE: {
      struct ResolveResult cond_res, body_res;

      cond_res = resolve_expr(varmap, &stmt->as.do_while_stmt.cond);
      if (!cond_res.is_ok) {
        return cond_res;
      }

      body_res = resolve_stmt(varmap, stmt->as.do_while_stmt.body);
      if (!body_res.is_ok) {
        return body_res;
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
      if (stmt->as.ret.val) {
        struct ResolveResult r;

        r = resolve_expr(varmap, stmt->as.ret.val);
        if (!r.is_ok) {
          return r;
        }
      }

      break;
    }
    default:
      assert(0);
  }

  return (struct ResolveResult){.is_ok = true, .msg = NULL, .as.stmt = stmt};
}

static struct ResolveResult resolve_fn_decl(struct VariableMap **varmap,
                                            struct DeclFn *fn)
{
  struct VariableMap *variable_map, *outer_map;
  char *cpy;

  cpy = strdup(fn->name);

  if (varmap) {
    varmap_insert(varmap, fn->name, cpy);
  }

  if (fn->is_extern) {
    return (struct ResolveResult){.is_ok = true, .msg = NULL};
  }

  variable_map = varmap ? *varmap : NULL;
  outer_map = variable_map;

  for (int i = 0; i < fn->params.len; i++) {
    struct ResolveResult r;

    r = resolve_param(&variable_map, &fn->params.data[i]);
    if (!r.is_ok) {
      return r;
    }
  }

  for (int i = 0; i < fn->body.len; i++) {
    struct ResolveResult r;

    r = resolve_stmt(&variable_map, &fn->body.data[i]);
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

  return (struct ResolveResult){.is_ok = true, .msg = NULL};
}

static struct ResolveResult resolve_variable_decl(struct VariableMap **varmap,
                                                  struct DeclVariable *variable)
{
  char *resolved_name;

  if (variable->init) {
    struct ResolveResult r = resolve_expr(varmap, variable->init);
    if (!r.is_ok) {
      return r;
    }
  }

  resolved_name = strdup(variable->name);
  varmap_insert(varmap, variable->name, resolved_name);

  return (struct ResolveResult){.is_ok = true, .msg = NULL};
}

static struct ResolveResult resolve_decl(struct VariableMap **varmap,
                                         struct Decl *decl)
{
  switch (decl->kind) {
    case DECL_FN:
      return resolve_fn_decl(varmap, &decl->as.fn);
    case DECL_VARIABLE:
      return resolve_variable_decl(varmap, &decl->as.variable);
    case DECL_STRUCT:
    case DECL_UNION:
    case DECL_ENUM:
      break;
    default:
      assert(0);
  }

  return (struct ResolveResult){.is_ok = true, .msg = NULL, .as.decl = decl};
}

struct ResolveResult resolve(struct AST *ast)
{
  struct VariableMap *global_map = NULL;
  for (int i = 0; i < ast->decls.len; i++) {
    struct ResolveResult r;

    r = resolve_decl(&global_map, &ast->decls.data[i]);
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
