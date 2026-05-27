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

enum ResolveTagKind {
  RESOLVE_TAG_UNKNOWN,
  RESOLVE_TAG_STRUCT,
  RESOLVE_TAG_UNION,
};

struct ResolveTagMap {
  struct ResolveTagMap *next;
  char *name;
  char *uniq_name;
  enum ResolveTagKind kind;
};

static struct ResolveResult ok_type(Type t)
{
  return (struct ResolveResult){.is_ok = true, .msg = NULL, .as.type = t};
}

static struct ResolveResult err_type(char *msg)
{
  return (struct ResolveResult){.is_ok = false, .msg = msg, .as.type = {0}};
}

static struct ResolveTagMap *tagmap_get(struct ResolveTagMap *tagmap,
                                        char *name)
{
  while (tagmap) {
    if (strcmp(tagmap->name, name) == 0) {
      return tagmap;
    }
    tagmap = tagmap->next;
  }
  return NULL;
}

static struct ResolveTagMap *tagmap_insert(struct ResolveTagMap **tagmap,
                                           char *name,
                                           enum ResolveTagKind kind)
{
  struct ResolveTagMap *node;

  node = malloc(sizeof(*node));
  node->name = strdup(name);
  node->uniq_name = mkstr("record.%s.%d", name, mktmp());
  node->kind = kind;
  node->next = *tagmap;

  *tagmap = node;
  return node;
}

static struct ResolveResult resolve_type(struct ResolveTagMap **tagmap, Type t)
{
  switch (t.kind) {
    case STRUCT_T: {
      struct ResolveTagMap *entry;

      entry = tagmap_get(*tagmap, t.as.struct_name);
      if (!entry) {
        /*
         * A named record type can be mentioned before its declaration.  Keep
         * the AST valid by creating an incomplete resolver entry now; if a
         * matching struct/union declaration appears later, it will reuse this
         * unique name.
         */
        entry = tagmap_insert(tagmap, t.as.struct_name, RESOLVE_TAG_UNKNOWN);
      }

      free(t.as.struct_name);
      t.as.struct_name = strdup(entry->uniq_name);
      return ok_type(t);
    }

    case PTR_T: {
      struct ResolveResult base_res;

      if (!t.as.base) {
        return err_type("Pointer type has no base type");
      }

      base_res = resolve_type(tagmap, *t.as.base);
      if (!base_res.is_ok) {
        return base_res;
      }

      *t.as.base = base_res.as.type;
      return ok_type(t);
    }

    case FN_T: {
      for (int i = 0; i < t.as.func.params.len; i++) {
        struct ResolveResult param_res;

        param_res = resolve_type(tagmap, t.as.func.params.data[i]);
        if (!param_res.is_ok) {
          return param_res;
        }

        t.as.func.params.data[i] = param_res.as.type;
      }

      if (t.as.func.retval) {
        struct ResolveResult ret_res;

        ret_res = resolve_type(tagmap, *t.as.func.retval);
        if (!ret_res.is_ok) {
          return ret_res;
        }

        *t.as.func.retval = ret_res.as.type;
      }

      return ok_type(t);
    }

    case I8_T:
    case I16_T:
    case I32_T:
    case I64_T:
    case U8_T:
    case U16_T:
    case U32_T:
    case U64_T:
    case F32_T:
    case F64_T:
    case BOOL_T:
    case STR_T:
    case TASK_T:
    case VOID_T:
      return ok_type(t);

    case UNKNOWN_T:
      return err_type("Unknown type");

    default:
      assert(0 && "unhandled type kind in resolve_type");
  }
}

static void free_tagmap(struct ResolveTagMap *tagmap)
{
  while (tagmap) {
    struct ResolveTagMap *tmp;

    tmp = tagmap;
    tagmap = tagmap->next;

    free(tmp->name);
    free(tmp->uniq_name);
    free(tmp);
  }
}

static struct ResolveResult resolve_expr(struct VariableMap **varmap,
                                         struct ResolveTagMap **tagmap,
                                         struct Expr *expr)
{
  switch (expr->kind) {
    case EXPR_STRUCT_INIT: {
      struct ResolveTagMap *entry;

      entry = tagmap_get(*tagmap, expr->as.struct_init.struct_name);
      if (!entry) {
        entry = tagmap_insert(tagmap, expr->as.struct_init.struct_name,
                              RESOLVE_TAG_UNKNOWN);
      }

      free(expr->as.struct_init.struct_name);
      expr->as.struct_init.struct_name = strdup(entry->uniq_name);

      for (int i = 0; i < expr->as.struct_init.values.len; i++) {
        struct ResolveResult r;

        r = resolve_expr(varmap, tagmap, expr->as.struct_init.values.data[i].expr);
        if (!r.is_ok) {
          return r;
        }
      }
      break;
    }
    case EXPR_MEMBER: {
      struct ResolveResult r;

      r = resolve_expr(varmap, tagmap, expr->as.member.target);
      if (!r.is_ok) {
        return r;
      }
      break;
    }
    case EXPR_AWAIT: {
      struct ResolveResult r = resolve_expr(varmap, tagmap, expr->as.await_expr.expr);
      if (!r.is_ok) {
        return r;
      }
      break;
    }
    case EXPR_CAST: {
      struct ResolveResult r;

      r = resolve_expr(varmap, tagmap, expr->as.cast.expr);
      if (!r.is_ok) {
        return r;
      }

      r = resolve_type(tagmap, expr->as.cast.target_type);
      if (!r.is_ok) {
        return r;
      }
      expr->as.cast.target_type = r.as.type;

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
          return (struct ResolveResult){
              .is_ok = false,
              .msg = mkstr("Undefined variable '%s'", expr->as.var.name)};
        }
      }
      break;
    }
    case EXPR_BINARY: {
      struct ResolveResult r1, r2;

      r1 = resolve_expr(varmap, tagmap, expr->as.binary.lhs);
      if (!r1.is_ok) {
        return r1;
      }

      r2 = resolve_expr(varmap, tagmap, expr->as.binary.rhs);
      if (!r2.is_ok) {
        return r2;
      }

      break;
    }
    case EXPR_CALL: {
      struct ResolveResult r;

      r = resolve_expr(varmap, tagmap, expr->as.call.target);
      if (!r.is_ok) {
        return r;
      }

      for (int i = 0; i < expr->as.call.arguments.len; i++) {
        r = resolve_expr(varmap, tagmap, &expr->as.call.arguments.data[i]);
        if (!r.is_ok) {
          return r;
        }
      }
      break;
    }
    case EXPR_ASSIGN: {
      struct ResolveResult rlhs, rrhs;

      rlhs = resolve_expr(varmap, tagmap, expr->as.assign.lhs);
      if (!rlhs.is_ok) {
        return rlhs;
      }

      rrhs = resolve_expr(varmap, tagmap, expr->as.assign.rhs);
      if (!rrhs.is_ok) {
        return rrhs;
      }

      break;
    }
    case EXPR_COMPOUND_ASSIGN: {
      struct ResolveResult rlhs, rrhs;

      rlhs = resolve_expr(varmap, tagmap, expr->as.compound_assign.lhs);
      if (!rlhs.is_ok) {
        return rlhs;
      }

      rrhs = resolve_expr(varmap, tagmap, expr->as.compound_assign.rhs);
      if (!rrhs.is_ok) {
        return rrhs;
      }

      break;
    }

    case EXPR_UNARY: {
      struct ResolveResult r;

      r = resolve_expr(varmap, tagmap, expr->as.unary.expr);
      if (!r.is_ok) {
        return r;
      }

      break;
    }
    case EXPR_DEREF: {
      struct ResolveResult r;

      r = resolve_expr(varmap, tagmap, expr->as.deref.expr);
      if (!r.is_ok) {
        return r;
      }

      break;
    }
    case EXPR_ADDROF: {
      struct ResolveResult r;

      r = resolve_expr(varmap, tagmap, expr->as.addrof.expr);
      if (!r.is_ok) {
        return r;
      }

      break;
    }
    case EXPR_SIZEOF: {
      struct ResolveResult r;

      r = resolve_expr(varmap, tagmap, expr->as.sizeof_expr.expr);
      if (!r.is_ok) {
        return r;
      }

      break;
    }
    case EXPR_SIZEOF_T: {
      struct ResolveResult r;

      r = resolve_type(tagmap, expr->as.sizeoft_expr.target_type);
      if (!r.is_ok) {
        return r;
      }
      expr->as.sizeoft_expr.target_type = r.as.type;

      break;
    }
    default:
      assert(0);
  }
  return (struct ResolveResult){.is_ok = true, .msg = NULL, .as.expr = expr};
}

static struct ResolveResult resolve_param(struct VariableMap **varmap,
                                          struct ResolveTagMap **tagmap,
                                          struct Parameter *param)
{
  char *uniq_name;
  struct ResolveResult type_res;

  type_res = resolve_type(tagmap, param->type);
  if (!type_res.is_ok) {
    return type_res;
  }
  param->type = type_res.as.type;

  uniq_name = mkuniq(param->name);
  varmap_insert(varmap, param->name, uniq_name);
  free(param->name);
  param->name = strdup(uniq_name);

  return (struct ResolveResult){.is_ok = true, .msg = NULL, .as.param = param};
}

static struct ResolveResult resolve_stmt(struct VariableMap **varmap,
                                         struct ResolveTagMap **tagmap,
                                         struct Stmt *stmt)
{
  switch (stmt->kind) {
    case STMT_LOOP: {
      struct ResolveResult r;

      r = resolve_stmt(varmap, tagmap, stmt->as.loop.body);
      if (!r.is_ok) {
        return r;
      }

      break;
    }
    case STMT_LABELED: {
      struct ResolveResult r;

      r = resolve_stmt(varmap, tagmap, stmt->as.labeled.stmt);
      if (!r.is_ok) {
        return r;
      }

      break;
    }
    case STMT_BREAK:
    case STMT_CONTINUE:
    case STMT_GOTO:
    case STMT_YIELD:
      break;
    case STMT_EXPR: {
      struct ResolveResult r;

      r = resolve_expr(varmap, tagmap, &stmt->as.expr_stmt.expr);
      if (!r.is_ok) {
        return r;
      }
      break;
    }
    case STMT_DO_WHILE: {
      struct ResolveResult cond_res, body_res;

      cond_res = resolve_expr(varmap, tagmap, &stmt->as.do_while_stmt.cond);
      if (!cond_res.is_ok) {
        return cond_res;
      }

      body_res = resolve_stmt(varmap, tagmap, stmt->as.do_while_stmt.body);
      if (!body_res.is_ok) {
        return body_res;
      }

      break;
    }
    case STMT_WHILE: {
      struct ResolveResult cond_res, body_res;

      cond_res = resolve_expr(varmap, tagmap, &stmt->as.while_stmt.cond);
      if (!cond_res.is_ok) {
        return cond_res;
      }

      body_res = resolve_stmt(varmap, tagmap, stmt->as.while_stmt.body);
      if (!body_res.is_ok) {
        return body_res;
      }

      break;
    }
    case STMT_IF: {
      struct ResolveResult cond_res, then_res, else_res;

      cond_res = resolve_expr(varmap, tagmap, &stmt->as.if_stmt.cond);
      if (!cond_res.is_ok) {
        return cond_res;
      }

      then_res = resolve_stmt(varmap, tagmap, stmt->as.if_stmt.then_block);
      if (!then_res.is_ok) {
        return then_res;
      }

      if (stmt->as.if_stmt.else_block) {
        else_res = resolve_stmt(varmap, tagmap, stmt->as.if_stmt.else_block);
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

        r = resolve_stmt(varmap, tagmap, &stmt->as.block.stmts.data[i]);
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

      r = resolve_type(tagmap, stmt->as.let.type);
      if (!r.is_ok) {
        return r;
      }
      stmt->as.let.type = r.as.type;

      r = resolve_expr(varmap, tagmap, stmt->as.let.init);
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

        r = resolve_expr(varmap, tagmap, stmt->as.ret.val);
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
                                            struct ResolveTagMap **tagmap,
                                            struct DeclFn *fn)
{
  struct VariableMap *variable_map, *outer_map;
  char *cpy;

  cpy = strdup(fn->name);

  if (varmap) {
    varmap_insert(varmap, fn->name, cpy);
  }

  {
    struct ResolveResult r;

    r = resolve_type(tagmap, fn->retval);
    if (!r.is_ok) {
      return r;
    }
    fn->retval = r.as.type;

    if (fn->is_extern) {
      for (int i = 0; i < fn->params.len; i++) {
        r = resolve_type(tagmap, fn->params.data[i].type);
        if (!r.is_ok) {
          return r;
        }
        fn->params.data[i].type = r.as.type;
      }

      return (struct ResolveResult){.is_ok = true, .msg = NULL};
    }
  }

  variable_map = varmap ? *varmap : NULL;
  outer_map = variable_map;

  for (int i = 0; i < fn->params.len; i++) {
    struct ResolveResult r;

    r = resolve_param(&variable_map, tagmap, &fn->params.data[i]);
    if (!r.is_ok) {
      return r;
    }
  }

  for (int i = 0; i < fn->body.len; i++) {
    struct ResolveResult r;

    r = resolve_stmt(&variable_map, tagmap, &fn->body.data[i]);
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
                                                  struct ResolveTagMap **tagmap,
                                                  struct DeclVariable *variable)
{
  char *resolved_name;

  {
    struct ResolveResult r;

    r = resolve_type(tagmap, variable->type);
    if (!r.is_ok) {
      return r;
    }
    variable->type = r.as.type;

    if (variable->init) {
      r = resolve_expr(varmap, tagmap, variable->init);
      if (!r.is_ok) {
        return r;
      }
    }
  }

  resolved_name = strdup(variable->name);
  varmap_insert(varmap, variable->name, resolved_name);

  return (struct ResolveResult){.is_ok = true, .msg = NULL};
}

static struct ResolveResult resolve_record_decl(struct ResolveTagMap **tagmap,
                                               char **name,
                                               VecStructField *fields,
                                               bool is_union)
{
  struct ResolveTagMap *entry;
  enum ResolveTagKind want_kind;

  want_kind = is_union ? RESOLVE_TAG_UNION : RESOLVE_TAG_STRUCT;

  entry = tagmap_get(*tagmap, *name);
  if (!entry) {
    entry = tagmap_insert(tagmap, *name, want_kind);
  } else if (entry->kind == RESOLVE_TAG_UNKNOWN) {
    entry->kind = want_kind;
  } else if (entry->kind != want_kind) {
    return err_type(mkstr("Record tag '%s' was declared with a different kind",
                          *name));
  }

  free(*name);
  *name = strdup(entry->uniq_name);

  for (int i = 0; i < fields->len; i++) {
    struct ResolveResult r;

    r = resolve_type(tagmap, fields->data[i].type);
    if (!r.is_ok) {
      return r;
    }
    fields->data[i].type = r.as.type;
  }

  return (struct ResolveResult){.is_ok = true, .msg = NULL};
}

static struct ResolveResult resolve_decl(struct VariableMap **varmap,
                                         struct ResolveTagMap **tagmap,
                                         struct Decl *decl)
{
  switch (decl->kind) {
    case DECL_FN:
      return resolve_fn_decl(varmap, tagmap, &decl->as.fn);
    case DECL_VARIABLE:
      return resolve_variable_decl(varmap, tagmap, &decl->as.variable);
    case DECL_STRUCT:
      return resolve_record_decl(tagmap, &decl->as.struct_decl.name,
                                 &decl->as.struct_decl.fields, false);
    case DECL_UNION:
      return resolve_record_decl(tagmap, &decl->as.union_decl.name,
                                 &decl->as.union_decl.fields, true);
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
  struct ResolveTagMap *tag_map = NULL;

  for (int i = 0; i < ast->decls.len; i++) {
    struct ResolveResult r;

    r = resolve_decl(&global_map, &tag_map, &ast->decls.data[i]);
    if (!r.is_ok) {
      while (global_map) {
        struct VariableMap *tmp;

        tmp = global_map;
        global_map = global_map->next;

        free(tmp->name);
        free(tmp->value.uniq_name);
        free(tmp);
      }
      free_tagmap(tag_map);
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
  free_tagmap(tag_map);

  return (struct ResolveResult){.is_ok = true, .msg = NULL, .as.ast = ast};
}
