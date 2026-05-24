#include "parser.h"

#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tokenizer.h"
#include "typechecker.h"
#include "util.h"

void free_expr(struct Expr *expr)
{
  switch (expr->kind) {
    case EXPR_LITERAL: {
      switch (expr->as.literal.kind) {
        case LITERAL_NUM:
        case LITERAL_BOOL:
          break;
        case LITERAL_STR:
          free(expr->as.literal.as.str);
          break;
        default:
          assert(0);
      }
      break;
    }
    case EXPR_VARIABLE: {
      free(expr->as.var.name);
      break;
    }
    case EXPR_UNARY: {
      free_expr(expr->as.unary.expr);
      free(expr->as.unary.expr);
      free(expr->as.unary.op);
      break;
    }
    case EXPR_BINARY: {
      free_expr(expr->as.binary.lhs);
      free_expr(expr->as.binary.rhs);
      free(expr->as.binary.lhs);
      free(expr->as.binary.rhs);
      break;
    }
    case EXPR_ASSIGN: {
      free_expr(expr->as.assign.lhs);
      free_expr(expr->as.assign.rhs);
      free(expr->as.assign.lhs);
      free(expr->as.assign.rhs);
      break;
    }
    case EXPR_COMPOUND_ASSIGN: {
      free_expr(expr->as.compound_assign.lhs);
      free_expr(expr->as.compound_assign.rhs);
      free(expr->as.compound_assign.lhs);
      free(expr->as.compound_assign.rhs);
      break;
    }
    case EXPR_CALL: {
      free_expr(expr->as.call.target);
      free(expr->as.call.target);
      for (int i = 0; i < expr->as.call.arguments.len; i++) {
        free_expr(&expr->as.call.arguments.data[i]);
      }
      vec_free(&expr->as.call.arguments);
      break;
    }
    case EXPR_ADDROF: {
      free_expr(expr->as.addrof.expr);
      free(expr->as.addrof.expr);
      break;
    }
    case EXPR_SIZEOF: {
      free_expr(expr->as.sizeof_expr.expr);
      free(expr->as.sizeof_expr.expr);
      break;
    }
    case EXPR_DEREF: {
      free_expr(expr->as.deref.expr);
      free(expr->as.deref.expr);
      break;
    }
    case EXPR_CAST: {
      free_expr(expr->as.cast.expr);
      free(expr->as.cast.expr);
      break;
    }
    case EXPR_STRUCT_INIT: {
      free(expr->as.struct_init.struct_name);
      for (int i = 0; i < expr->as.struct_init.values.len; i++) {
        if (expr->as.struct_init.values.data[i].designator) {
          free(expr->as.struct_init.values.data[i].designator);
        }
        free_expr(expr->as.struct_init.values.data[i].expr);
      }
      vec_free(&expr->as.struct_init.values);
      break;
    }
    case EXPR_MEMBER: {
      free(expr->as.member.field_name);
      free_expr(expr->as.member.target);
      break;
    }
    default:
      assert(0);
  }
}

static void free_params(VecParam *params)
{
  for (int i = 0; i < params->len; i++) {
    free(params->data[i].name);
    free_type(&params->data[i].type);
  }
  vec_free(params);
}

static void free_struct_fields(VecStructField *fields)
{
  for (int i = 0; i < fields->len; i++) {
    free(fields->data[i].name);
    free_type(&fields->data[i].type);
  }
  vec_free(fields);
}

void free_stmt(struct Stmt *stmt)
{
  switch (stmt->kind) {
    case STMT_LOOP: {
      free(stmt->as.loop.label);
      free_stmt(stmt->as.loop.body);
      free(stmt->as.loop.body);
      break;
    }
    case STMT_BREAK:
    case STMT_CONTINUE:
      break;
    case STMT_GOTO: {
      free(stmt->as.goto_stmt.label);
      break;
    }
    case STMT_LABELED: {
      free(stmt->as.labeled.label);
      free_stmt(stmt->as.labeled.stmt);
      free(stmt->as.labeled.stmt);
      break;
    }
    case STMT_LET: {
      free(stmt->as.let.name);
      free_expr(stmt->as.let.init);
      free(stmt->as.let.init);
      free_type(&stmt->as.let.type);
      break;
    }
    case STMT_DO_WHILE: {
      free_expr(&stmt->as.do_while_stmt.cond);
      free_stmt(stmt->as.do_while_stmt.body);
      free(stmt->as.do_while_stmt.body);
      if (stmt->as.do_while_stmt.label) {
        free(stmt->as.do_while_stmt.label);
      }
      break;
    }
    case STMT_WHILE: {
      free_expr(&stmt->as.while_stmt.cond);
      free_stmt(stmt->as.while_stmt.body);
      free(stmt->as.while_stmt.body);
      if (stmt->as.while_stmt.label) {
        free(stmt->as.while_stmt.label);
      }
      break;
    }
    case STMT_IF: {
      free_expr(&stmt->as.if_stmt.cond);
      free_stmt(stmt->as.if_stmt.then_block);
      free(stmt->as.if_stmt.then_block);
      if (stmt->as.if_stmt.else_block) {
        free_stmt(stmt->as.if_stmt.else_block);
        free(stmt->as.if_stmt.else_block);
      }
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
        free(stmt->as.ret.val);
      }
      break;
    }
    case STMT_EXPR: {
      free_expr(&stmt->as.expr_stmt.expr);
      break;
    }
    default:
      assert(0);
  }
}

void free_decl(struct Decl *decl)
{
  switch (decl->kind) {
    case DECL_FN: {
      free(decl->as.fn.name);
      free_params(&decl->as.fn.params);
      for (int i = 0; i < decl->as.fn.body.len; i++) {
        free_stmt(&decl->as.fn.body.data[i]);
      }
      vec_free(&decl->as.fn.body);
      free_type(&decl->as.fn.retval);
      break;
    }
    case DECL_STRUCT: {
      free(decl->as.struct_decl.name);
      free_struct_fields(&decl->as.struct_decl.fields);
      break;
    }
    case DECL_UNION: {
      free(decl->as.union_decl.name);
      free_struct_fields(&decl->as.union_decl.fields);
      break;
    }
    case DECL_VARIABLE: {
      free(decl->as.variable.name);
      free_type(&decl->as.variable.type);
      if (decl->as.variable.init) {
        free_expr(decl->as.variable.init);
        free(decl->as.variable.init);
      }
      break;
    }
    case DECL_ENUM: {
      free(decl->as.enum_decl.name);
      for (int i = 0; i < decl->as.enum_decl.variants.len; i++) {
        free(decl->as.enum_decl.variants.data[i].name);
      }
      vec_free(&decl->as.enum_decl.variants);
      break;
    }
    default:
      assert(0);
  }
}

void free_ast(struct AST *ast)
{
  if (!ast) {
    return;
  }
  for (int i = 0; i < ast->decls.len; i++) {
    free_decl(&ast->decls.data[i]);
  }
  vec_free(&ast->decls);
  free(ast);
}

void init_parser(struct Tokenizer *tokenizer, struct Parser *parser)
{
  parser->tokenizer = tokenizer;
  parser->has_peek = false;

  parser->prev = &parser->prev_tok;
  parser->curr = &parser->curr_tok;

  parser->current_fn = NULL;
  parser->global_decls = NULL;

  parser->curr_tok = next_token(parser->tokenizer);
}

static struct Token fetch_next(struct Parser *parser)
{
  if (parser->has_peek) {
    parser->has_peek = false;
    return parser->peek_tok;
  }
  return next_token(parser->tokenizer);
}

static struct Token *advance_parser(struct Parser *parser)
{
  parser->prev_tok = parser->curr_tok;
  parser->curr_tok = fetch_next(parser);

  return parser->prev;
}

static bool check_next(struct Parser *parser, enum TokenKind kind)
{
  if (!parser->has_peek) {
    parser->peek_tok = next_token(parser->tokenizer);
    parser->has_peek = true;
  }
  return parser->peek_tok.kind == kind;
}

static bool check(struct Parser *parser, enum TokenKind kind)
{
  return parser->curr->kind == kind;
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

static struct Token *consume(struct Parser *parser, enum TokenKind kind)
{
  if (parser->curr && parser->curr->kind == kind) {
    return advance_parser(parser);
  }
#if defined(DEBUG_ENABLE_DUMPS) && defined(DEBUG_TOKENIZER)
  printf("Encountered wrong token.  Prev is: ");
  print_token(parser->prev);
  printf("\n");
  printf("Encountered wrong token.  Curr is: ");
  print_token(parser->curr);
  printf("\n");
#endif
  return NULL;
}

static struct Token *consume_any(struct Parser *parser, int n, ...)
{
  va_list ap;
  va_start(ap, n);

  for (int i = 0; i < n; i++) {
    enum TokenKind kind;

    kind = va_arg(ap, enum TokenKind);
    if (parser->curr && parser->curr->kind == kind) {
      va_end(ap);
      return advance_parser(parser);
    }
  }

  va_end(ap);
  return NULL;
}

static struct ParseFnResult parse_expr(struct Parser *parser);

static struct ParseFnResult primary(struct Parser *parser)
{
  struct ParseFnResult res;

  res.is_ok = true;
  res.msg = NULL;

  if (check(parser, TOKEN_TRUE) || check(parser, TOKEN_FALSE)) {
    bool is_true;
    struct Token *token_literal;
    struct Literal literal = {0};

    is_true = check(parser, TOKEN_TRUE);
    token_literal = consume(parser, is_true ? TOKEN_TRUE : TOKEN_FALSE);
    if (!token_literal) {
      return (struct ParseFnResult){
          .is_ok = false, .as.expr = {0}, .msg = "Expected 'true' or 'false'"};
    }

    literal.kind = LITERAL_BOOL;
    literal.as.boolean = (bool) is_true;
    literal.type = (Type){.kind = BOOL_T};

    res.as.expr = (struct Expr){
        .kind = EXPR_LITERAL, .as.literal = literal, .type = literal.type};
  } else if (check(parser, TOKEN_NUMBER) || check(parser, TOKEN_FP_NUMBER)) {
    struct Literal literal = {0};
    struct Token *token_literal;
    unsigned long long val;
    bool is_float;

    is_float = check(parser, TOKEN_FP_NUMBER);

    token_literal = consume(parser, is_float ? TOKEN_FP_NUMBER : TOKEN_NUMBER);
    if (!token_literal) {
      return (struct ParseFnResult){
          .is_ok = false, .as.expr = {0}, .msg = "Expected number"};
    }

    literal.kind = LITERAL_NUM;

    if (!is_float) {
      val = strtoull(parser->prev->start, NULL, 10);

      if (val <= UCHAR_MAX) {
        literal.type = (Type){.kind = U8_T};
        literal.as.u8 = val;
      } else if (val <= USHRT_MAX) {
        literal.type = (Type){.kind = U16_T};
        literal.as.u16 = val;
      } else if (val <= UINT_MAX) {
        literal.type = (Type){.kind = U32_T};
        literal.as.u32 = val;
      } else if (val <= ULLONG_MAX) {
        literal.type = (Type){.kind = U64_T};
        literal.as.u64 = val;
      } else {
        return (struct ParseFnResult){
            .is_ok = false, .msg = "Can't parse literal", .as.expr = {0}};
      }
    } else {
      double fval;

      fval = strtod(parser->prev->start, NULL);

      literal.type = (Type){.kind = F64_T};
      literal.as.f64 = fval;
    }
    res.as.expr = (struct Expr){
        .kind = EXPR_LITERAL, .as.literal = literal, .type = literal.type};
  } else if (check(parser, TOKEN_STRING)) {
    struct Literal literal = {0};
    struct Token *token_literal;

    token_literal = consume(parser, TOKEN_STRING);
    if (!token_literal) {
      return (struct ParseFnResult){
          .is_ok = false, .as.expr = {0}, .msg = "Expected string"};
    }

    literal.kind = LITERAL_STR;
    literal.as.str = strndup(token_literal->start, token_literal->len);
    literal.type = (Type){.kind = STR_T};

    res.as.expr = (struct Expr){.kind = EXPR_LITERAL, .as.literal = literal};
  } else if (check(parser, TOKEN_IDENTIFIER)) {
    struct Token *token_id = consume(parser, TOKEN_IDENTIFIER);
    if (!token_id) {
      return (struct ParseFnResult){
          .is_ok = false, .as.expr = {0}, .msg = "Expected identifier"};
    }

    char *extracted_id;

    extracted_id = strndup(token_id->start, token_id->len);

    if (match(parser, 1, TOKEN_LBRACE)) {
      VecStructInitItem values = {0};

      while (!check(parser, TOKEN_RBRACE)) {
        char *designator = NULL;

        /* Parse designated initializer chain: .a.b: expr */
        if (check(parser, TOKEN_DOT)) {
          advance_parser(parser);
          char buf[256] = {0};

          while (true) {
            struct Token *id = consume(parser, TOKEN_IDENTIFIER);
            if (!id) {
              return (struct ParseFnResult){.is_ok = false,
                                            .msg = "Expected identifier"};
            }

            if (buf[0] != '\0') {
              strcat(buf, ".");
            }
            strncat(buf, id->start, id->len);

            if (check(parser, TOKEN_DOT)) {
              advance_parser(parser);
            } else {
              break;
            }
          }
          consume(parser, TOKEN_COLON);
          designator = strdup(buf);
        } else if (check(parser, TOKEN_IDENTIFIER) &&
                   check_next(parser, TOKEN_COLON)) {
          struct Token *id = consume(parser, TOKEN_IDENTIFIER);
          consume(parser, TOKEN_COLON);
          designator = strndup(id->start, id->len);
        }

        struct ParseFnResult val_res = parse_expr(parser);
        if (!val_res.is_ok) {
          return val_res;
        }

        struct StructInitItem item;
        item.designator = designator;
        item.expr = ALLOC(val_res.as.expr);
        item.resolved_offset = 0;
        vec_insert(&values, item);

        if (check(parser, TOKEN_COMMA)) {
          advance_parser(parser);
        }
      }

      consume(parser, TOKEN_RBRACE);

      struct ExprStructInit init;
      init.struct_name = extracted_id;
      init.values = values;
      res.as.expr =
          (struct Expr){.kind = EXPR_STRUCT_INIT, .as.struct_init = init};
    } else {
      struct ExprVar var = {0};
      var.name = strndup(token_id->start, token_id->len);
      var.type = (Type){.kind = UNKNOWN_T};

      res.is_ok = true;
      res.as.expr.kind = EXPR_VARIABLE;
      res.as.expr.as.var = var;
    }
  } else if (match(parser, 1, TOKEN_LPAREN)) {
    struct ParseFnResult expr_res;
    struct Token *token_rparen;

    expr_res = parse_expr(parser);
    if (!expr_res.is_ok) {
      return expr_res;
    }

    token_rparen = consume(parser, TOKEN_RPAREN);
    if (!token_rparen) {
      return (struct ParseFnResult){
          .is_ok = false, .msg = "Expected ')'", .as.stmt = {0}};
    }

    res = expr_res;
  } else {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected primary expression", .as.expr = {0}};
  }

  return res;
}

static struct ParseFnResult finish_call(struct Parser *parser,
                                        struct Expr callee)
{
  VecExpr arguments = {0};
  if (!check(parser, TOKEN_RPAREN)) {
    do {
      struct ParseFnResult r;

      r = parse_expr(parser);
      if (!r.is_ok) {
        for (int i = 0; i < arguments.len; i++) {
          free_expr(&arguments.data[i]);
        }
        free_expr(&callee);
        vec_free(&arguments);
        return r;
      }
      vec_insert(&arguments, r.as.expr);
    } while (match(parser, 1, TOKEN_COMMA));
  }

  struct Token *token_rparen;

  token_rparen = consume(parser, TOKEN_RPAREN);
  if (!token_rparen) {
    for (int i = 0; i < arguments.len; i++) {
      free_expr(&arguments.data[i]);
    }
    free_expr(&callee);
    vec_free(&arguments);
    return (struct ParseFnResult){.is_ok = false,
                                  .as.expr = {0},
                                  .msg = "Expected ')' after expression."};
  }

  struct ExprCall call_expr = {
      .target = ALLOC(callee),
      .arguments = arguments,
  };

  struct Expr e;

  e.kind = EXPR_CALL;
  e.as.call = call_expr;

  return (struct ParseFnResult){.as.expr = e, .is_ok = true, .msg = NULL};
}

static bool parse_enum_body(struct Parser *parser, VecEnumVariant *out_variants)
{
  int current_val = 0;

  while (!check(parser, TOKEN_RBRACE)) {
    struct Token *var_id = consume(parser, TOKEN_IDENTIFIER);
    if (!var_id) {
      return false;
    }
    char *var_name = strndup(var_id->start, var_id->len);

    if (check(parser, TOKEN_EQUAL)) {
      advance_parser(parser);

      struct ParseFnResult expr_res = parse_expr(parser);
      if (!expr_res.is_ok) {
        return false;
      }

      struct Expr *e = &expr_res.as.expr;
      int sign = 1;

      if (e->kind == EXPR_UNARY && strncmp(e->as.unary.op, "-", 1) == 0) {
        sign = -1;
        e = e->as.unary.expr;
      }

      if (e->kind == EXPR_LITERAL && e->as.literal.kind == LITERAL_NUM) {
        unsigned long long raw_val = 0;
        switch (e->as.literal.type.kind) {
          case U8_T:
            raw_val = e->as.literal.as.u8;
            break;
          case I8_T:
            raw_val = e->as.literal.as.i8;
            break;
          case U16_T:
            raw_val = e->as.literal.as.u16;
            break;
          case I16_T:
            raw_val = e->as.literal.as.i16;
            break;
          case U32_T:
            raw_val = e->as.literal.as.u32;
            break;
          case I32_T:
            raw_val = e->as.literal.as.i32;
            break;
          case U64_T:
            raw_val = e->as.literal.as.u64;
            break;
          case I64_T:
            raw_val = e->as.literal.as.i64;
            break;
          default:
            break;
        }
        current_val = sign * (int) raw_val;
      } else {
        free_expr(&expr_res.as.expr);
        return false;
      }
      free_expr(&expr_res.as.expr);
    }

    struct EnumVariant var;
    var.name = var_name;
    var.value = current_val;
    vec_insert(out_variants, var);

    enum_variant_insert(var_name, current_val);

    current_val++;

    if (check(parser, TOKEN_COMMA)) {
      advance_parser(parser);
    }
  }
  return true;
}

static bool is_enum_type(char *name)
{
  struct EnumTypeItem *curr;

  curr = enum_types;
  while (curr) {
    if (strcmp(curr->name, name) == 0) {
      return true;
    }
    curr = curr->next;
  }
  return false;
}

static Type parse_type(struct Parser *parser)
{
  if (match(parser, 1, TOKEN_STAR)) {
    Type *base = malloc(sizeof(Type));
    *base = parse_type(parser);
    return (Type){.kind = PTR_T, .as.base = base};
  }

  if (check(parser, TOKEN_VOID)) {
    advance_parser(parser);
    return (Type){.kind = VOID_T};
  }

  if (check(parser, TOKEN_UNION) || check(parser, TOKEN_STRUCT)) {
    bool is_union = check(parser, TOKEN_UNION);
    advance_parser(parser);

    char *anon_name = mkuniq(is_union ? "anon_union" : "anon_struct");

    consume(parser, TOKEN_LBRACE);
    VecStructField fields = {0};

    while (!check(parser, TOKEN_RBRACE)) {
      char *field_name = NULL;
      Type field_type;

      if (check(parser, TOKEN_IDENTIFIER) && check_next(parser, TOKEN_COLON)) {
        struct Token *name_tok = consume(parser, TOKEN_IDENTIFIER);
        consume(parser, TOKEN_COLON);
        field_type = parse_type(parser);
        field_name = strndup(name_tok->start, name_tok->len);
      } else {
        field_type = parse_type(parser);
        struct Token *name_tok = consume(parser, TOKEN_IDENTIFIER);
        if (name_tok) {
          field_name = strndup(name_tok->start, name_tok->len);
        } else {
          field_name = strdup("");
        }
      }

      struct StructField field;
      field.name = field_name;
      field.type = field_type;
      field.offset = 0;
      vec_insert(&fields, field);

      if (check(parser, TOKEN_COMMA)) {
        advance_parser(parser);
      }
    }
    consume(parser, TOKEN_RBRACE);

    struct Decl decl;
    if (is_union) {
      struct DeclUnion union_decl;
      union_decl.name = strdup(anon_name);
      union_decl.fields = fields;
      decl = (struct Decl){.kind = DECL_UNION, .as.union_decl = union_decl};
    } else {
      struct DeclStruct struct_decl;
      struct_decl.name = strdup(anon_name);
      struct_decl.fields = fields;
      decl = (struct Decl){.kind = DECL_STRUCT, .as.struct_decl = struct_decl};
    }

    if (parser->global_decls) {
      vec_insert(parser->global_decls, decl);
    }

    Type custom_type;
    custom_type.kind = STRUCT_T;
    custom_type.as.struct_name = anon_name;
    return custom_type;
  }

  if (check(parser, TOKEN_ENUM)) {
    advance_parser(parser);

    if (check(parser, TOKEN_LBRACE)) {
      char *anon_name = mkuniq("anon_enum");
      enum_type_insert(anon_name);

      consume(parser, TOKEN_LBRACE);
      VecEnumVariant variants = {0};

      if (!parse_enum_body(parser, &variants)) {
        return (Type){.kind = UNKNOWN_T};
      }
      consume(parser, TOKEN_RBRACE);

      struct DeclEnum enum_decl;
      enum_decl.name = anon_name;
      enum_decl.variants = variants;

      struct Decl decl;
      decl.kind = DECL_ENUM;
      decl.as.enum_decl = enum_decl;

      if (parser->global_decls) {
        vec_insert(parser->global_decls, decl);
      }

      return (Type){.kind = I32_T};
    }

    struct Token *id_token = consume(parser, TOKEN_IDENTIFIER);
    if (id_token) {
      return (Type){.kind = I32_T};
    }
  }

  struct Token *type_token =
      consume_any(parser, 12, TOKEN_U8, TOKEN_U16, TOKEN_U32, TOKEN_U64,
                  TOKEN_I8, TOKEN_I16, TOKEN_I32, TOKEN_I64, TOKEN_F32,
                  TOKEN_F64, TOKEN_BOOL, TOKEN_STR);

  if (type_token) {
    if (strncmp(type_token->start, "i8", type_token->len) == 0) {
      return (Type){.kind = I8_T};
    } else if (strncmp(type_token->start, "i16", type_token->len) == 0) {
      return (Type){.kind = I16_T};
    } else if (strncmp(type_token->start, "i32", type_token->len) == 0) {
      return (Type){.kind = I32_T};
    } else if (strncmp(type_token->start, "i64", type_token->len) == 0) {
      return (Type){.kind = I64_T};
    } else if (strncmp(type_token->start, "u8", type_token->len) == 0) {
      return (Type){.kind = U8_T};
    } else if (strncmp(type_token->start, "u16", type_token->len) == 0) {
      return (Type){.kind = U16_T};
    } else if (strncmp(type_token->start, "u32", type_token->len) == 0) {
      return (Type){.kind = U32_T};
    } else if (strncmp(type_token->start, "u64", type_token->len) == 0) {
      return (Type){.kind = U64_T};
    } else if (strncmp(type_token->start, "f32", type_token->len) == 0) {
      return (Type){.kind = F32_T};
    } else if (strncmp(type_token->start, "f64", type_token->len) == 0) {
      return (Type){.kind = F64_T};
    } else if (strncmp(type_token->start, "bool", type_token->len) == 0) {
      return (Type){.kind = BOOL_T};
    } else if (strncmp(type_token->start, "str", type_token->len) == 0) {
      return (Type){.kind = STR_T};
    }
  } else if (check(parser, TOKEN_IDENTIFIER)) {
    struct Token *id_token = consume(parser, TOKEN_IDENTIFIER);
    char *name = strndup(id_token->start, id_token->len);

    if (is_enum_type(name)) {
      free(name);
      return (Type){.kind = I32_T};
    } else {
      Type custom_type;
      custom_type.kind = STRUCT_T;
      custom_type.as.struct_name = name;
      return custom_type;
    }
  }

  if (check(parser, TOKEN_ENUM)) {
    advance_parser(parser);
    struct Token *id_token = consume(parser, TOKEN_IDENTIFIER);
    if (id_token) {
      return (Type){.kind = I32_T};
    }
  }
  return (Type){.kind = UNKNOWN_T};
}

static struct ParseFnResult postfix(struct Parser *parser)
{
  struct ParseFnResult expr_result;

  expr_result = primary(parser);
  if (!expr_result.is_ok) {
    return expr_result;
  }

  struct Expr expr = expr_result.as.expr;

  for (;;) {
    if (match(parser, 1, TOKEN_LPAREN)) {
      struct ParseFnResult finish_call_result;

      finish_call_result = finish_call(parser, expr);
      if (!finish_call_result.is_ok) {
        return finish_call_result;
      }
      expr = finish_call_result.as.expr;
    } else if (match(parser, 2, TOKEN_DOT, TOKEN_ARROW)) {
      bool is_arrow;
      struct Token *field;

      is_arrow = (parser->prev->kind == TOKEN_ARROW);
      field = consume(parser, TOKEN_IDENTIFIER);

      struct ExprMember member_expr = {
          .target = ALLOC(expr),
          .field_name = strndup(field->start, field->len),
          .is_arrow = is_arrow,
      };

      expr.kind = EXPR_MEMBER;
      expr.as.member = member_expr;
    } else if (match(parser, 1, TOKEN_AS)) {
      Type t;

      t = parse_type(parser);

      struct ExprCast cast_expr = {
          .expr = ALLOC(expr),
          .target_type = t,
      };

      expr.kind = EXPR_CAST;
      expr.as.cast = cast_expr;
    } else {
      break;
    }
  }

  return (struct ParseFnResult){.as.expr = expr, .is_ok = true, .msg = NULL};
}

static struct ParseFnResult unary(struct Parser *parser)
{
  if (match(parser, 5, TOKEN_MINUS, TOKEN_BANG, TOKEN_TILDE, TOKEN_AMPERSAND,
            TOKEN_STAR)) {
    char *op = strndup(parser->prev->start, parser->prev->len);

    if (strncmp(op, "-", 1) == 0 || strncmp(op, "!", 1) == 0 ||
        strncmp(op, "~", 1) == 0) {
      struct ParseFnResult right_result;

      right_result = unary(parser);
      if (!right_result.is_ok) {
        free(op);
        return right_result;
      }

      struct Expr right, e;

      right = right_result.as.expr;

      struct ExprUnary unary_expr = {.expr = ALLOC(right), .op = op};

      e.kind = EXPR_UNARY;
      e.as.unary = unary_expr;

      return (struct ParseFnResult){.as.expr = e, .is_ok = true, .msg = NULL};
    } else if (strncmp(op, "*", 1) == 0) {
      struct ParseFnResult right_result;

      right_result = unary(parser);
      if (!right_result.is_ok) {
        free(op);
        return right_result;
      }

      struct Expr right, e;

      right = right_result.as.expr;

      struct ExprDeref deref_expr = {.expr = ALLOC(right)};

      e.kind = EXPR_DEREF;
      e.as.deref = deref_expr;

      free(op);

      return (struct ParseFnResult){.as.expr = e, .is_ok = true, .msg = NULL};

    } else if (strncmp(op, "&", 1) == 0) {
      struct ParseFnResult right_result;

      right_result = unary(parser);
      if (!right_result.is_ok) {
        free(op);
        return right_result;
      }

      struct Expr right, e;

      right = right_result.as.expr;

      struct ExprAddrOf addrof_expr = {.expr = ALLOC(right)};

      e.kind = EXPR_ADDROF;
      e.as.addrof = addrof_expr;

      free(op);

      return (struct ParseFnResult){.as.expr = e, .is_ok = true, .msg = NULL};
    } else {
      assert(0);
    }
  } else if (match(parser, 1, TOKEN_SIZEOF)) {
    struct ParseFnResult right_result;
    struct Token *token_lparen, *token_rparen;

    token_lparen = consume(parser, TOKEN_LPAREN);
    if (!token_lparen) {
      return (struct ParseFnResult){
          .is_ok = false, .msg = "Expected '(' after sizeoef", .as.stmt = {0}};
    }

    right_result = unary(parser);
    if (!right_result.is_ok) {
      return right_result;
    }

    token_rparen = consume(parser, TOKEN_RPAREN);
    if (!token_rparen) {
      return (struct ParseFnResult){.is_ok = false,
                                    .msg = "Expected ')' after sizeof expr",
                                    .as.stmt = {0}};
    }

    struct Expr right, e;

    right = right_result.as.expr;

    struct ExprSizeof sizeof_expr = {.expr = ALLOC(right)};

    e.kind = EXPR_SIZEOF;
    e.as.sizeof_expr = sizeof_expr;
    return (struct ParseFnResult){.as.expr = e, .is_ok = true, .msg = NULL};
  }

  return postfix(parser);
}

static struct ParseFnResult factor(struct Parser *parser)
{
  struct ParseFnResult left_res, right_res;
  struct Expr left, right;

  left_res = unary(parser);
  if (!left_res.is_ok) {
    return left_res;
  }

  left = left_res.as.expr;
  while (match(parser, 2, TOKEN_STAR, TOKEN_SLASH)) {
    char *op = parser->prev->start;

    right_res = unary(parser);
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

static struct ParseFnResult term(struct Parser *parser)
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

static struct ParseFnResult shift(struct Parser *parser)
{
  struct ParseFnResult left_res, right_res;
  struct Expr left, right;

  left_res = term(parser);
  if (!left_res.is_ok) {
    return left_res;
  }

  left = left_res.as.expr;
  while (match(parser, 2, TOKEN_LESS_LESS, TOKEN_GREATER_GREATER)) {
    enum TokenKind op = parser->prev->kind;

    right_res = term(parser);
    if (!right_res.is_ok) {
      return right_res;
    }

    right = right_res.as.expr;

    struct ExprBin binexpr;
    binexpr.kind =
        (op == TOKEN_LESS_LESS) ? EXPR_BIN_SHIFT_LEFT : EXPR_BIN_SHIFT_RIGHT;
    binexpr.lhs = ALLOC(left);
    binexpr.rhs = ALLOC(right);

    left = (struct Expr){.kind = EXPR_BINARY, .as.binary = binexpr};
    left_res.as.expr = left;
  }

  return left_res;
}

static struct ParseFnResult comparison(struct Parser *parser)
{
  struct ParseFnResult left_res, right_res;
  struct Expr left, right;

  left_res = shift(parser);
  if (!left_res.is_ok) {
    return left_res;
  }

  left = left_res.as.expr;
  while (match(parser, 6, TOKEN_LESS, TOKEN_LESS_EQUAL, TOKEN_GREATER,
               TOKEN_GREATER_EQUAL, TOKEN_EQUAL_EQUAL, TOKEN_BANG_EQUAL)) {
    char *op = parser->prev->start;

    right_res = shift(parser);
    if (!right_res.is_ok) {
      return right_res;
    }

    right = right_res.as.expr;

    struct ExprBin binexpr;

    if (strncmp(op, "<=", 2) == 0) {
      binexpr.kind = EXPR_BIN_LESS_EQUAL;
    } else if (strncmp(op, ">=", 2) == 0) {
      binexpr.kind = EXPR_BIN_GREATER_EQUAL;
    } else if (strncmp(op, "==", 2) == 0) {
      binexpr.kind = EXPR_BIN_EQUAL_EQUAL;
    } else if (strncmp(op, "!=", 2) == 0) {
      binexpr.kind = EXPR_BIN_BANG_EQUAL;
    } else if (strncmp(op, "<", 1) == 0) {
      binexpr.kind = EXPR_BIN_LESS;
    } else if (strncmp(op, ">", 1) == 0) {
      binexpr.kind = EXPR_BIN_GREATER;
    }

    binexpr.lhs = ALLOC(left);
    binexpr.rhs = ALLOC(right);

    left = (struct Expr){.kind = EXPR_BINARY, .as.binary = binexpr};
    left_res.as.expr = left;
  }

  return left_res;
}

static struct ParseFnResult bitwise_and(struct Parser *parser)
{
  struct ParseFnResult left_res, right_res;
  struct Expr left, right;

  left_res = comparison(parser);
  if (!left_res.is_ok) {
    return left_res;
  }

  left = left_res.as.expr;
  while (match(parser, 1, TOKEN_AMPERSAND)) {
    right_res = comparison(parser);
    if (!right_res.is_ok) {
      return right_res;
    }

    right = right_res.as.expr;

    struct ExprBin binexpr;
    binexpr.kind = EXPR_BIN_BITWISE_AND;
    binexpr.lhs = ALLOC(left);
    binexpr.rhs = ALLOC(right);

    left = (struct Expr){.kind = EXPR_BINARY, .as.binary = binexpr};
    left_res.as.expr = left;
  }

  return left_res;
}

static struct ParseFnResult bitwise_xor(struct Parser *parser)
{
  struct ParseFnResult left_res, right_res;
  struct Expr left, right;

  left_res = bitwise_and(parser);
  if (!left_res.is_ok) {
    return left_res;
  }

  left = left_res.as.expr;
  while (match(parser, 1, TOKEN_CARET)) {
    right_res = bitwise_and(parser);
    if (!right_res.is_ok) {
      return right_res;
    }

    right = right_res.as.expr;

    struct ExprBin binexpr;
    binexpr.kind = EXPR_BIN_BITWISE_XOR;
    binexpr.lhs = ALLOC(left);
    binexpr.rhs = ALLOC(right);

    left = (struct Expr){.kind = EXPR_BINARY, .as.binary = binexpr};
    left_res.as.expr = left;
  }

  return left_res;
}

static struct ParseFnResult bitwise_or(struct Parser *parser)
{
  struct ParseFnResult left_res, right_res;
  struct Expr left, right;

  left_res = bitwise_xor(parser);
  if (!left_res.is_ok) {
    return left_res;
  }

  left = left_res.as.expr;
  while (match(parser, 1, TOKEN_PIPE)) {
    right_res = bitwise_xor(parser);
    if (!right_res.is_ok) {
      return right_res;
    }

    right = right_res.as.expr;

    struct ExprBin binexpr;
    binexpr.kind = EXPR_BIN_BITWISE_OR;
    binexpr.lhs = ALLOC(left);
    binexpr.rhs = ALLOC(right);

    left = (struct Expr){.kind = EXPR_BINARY, .as.binary = binexpr};
    left_res.as.expr = left;
  }

  return left_res;
}

static struct ParseFnResult logical_and(struct Parser *parser)
{
  struct ParseFnResult left_res, right_res;
  struct Expr left, right;

  left_res = bitwise_or(parser);
  if (!left_res.is_ok) {
    return left_res;
  }

  left = left_res.as.expr;
  while (match(parser, 1, TOKEN_AMPERSAND_AMPERSAND)) {
    right_res = bitwise_or(parser);
    if (!right_res.is_ok) {
      return right_res;
    }

    right = right_res.as.expr;

    struct ExprBin binexpr;
    binexpr.kind = EXPR_BIN_LOGICAL_AND;
    binexpr.lhs = ALLOC(left);
    binexpr.rhs = ALLOC(right);

    left = (struct Expr){.kind = EXPR_BINARY, .as.binary = binexpr};
    left_res.as.expr = left;
  }

  return left_res;
}

static struct ParseFnResult logical_or(struct Parser *parser)
{
  struct ParseFnResult left_res, right_res;
  struct Expr left, right;

  left_res = logical_and(parser);
  if (!left_res.is_ok) {
    return left_res;
  }

  left = left_res.as.expr;
  while (match(parser, 1, TOKEN_PIPE_PIPE)) {
    right_res = logical_and(parser);
    if (!right_res.is_ok) {
      return right_res;
    }

    right = right_res.as.expr;

    struct ExprBin binexpr;
    binexpr.kind = EXPR_BIN_LOGICAL_OR;
    binexpr.lhs = ALLOC(left);
    binexpr.rhs = ALLOC(right);

    left = (struct Expr){.kind = EXPR_BINARY, .as.binary = binexpr};
    left_res.as.expr = left;
  }

  return left_res;
}

static struct ParseFnResult assignment(struct Parser *parser)
{
  struct ParseFnResult expr_result, right_result;
  struct Expr expr, right;

  expr_result = logical_or(parser);
  if (!expr_result.is_ok) {
    return expr_result;
  }

  expr = expr_result.as.expr;
  if (match(parser, 10, TOKEN_EQUAL, TOKEN_PLUS_EQUAL, TOKEN_MINUS_EQUAL,
            TOKEN_STAR_EQUAL, TOKEN_SLASH_EQUAL, TOKEN_AMPERSAND_EQUAL,
            TOKEN_PIPE_EQUAL, TOKEN_CARET_EQUAL, TOKEN_LESS_LESS_EQUAL,
            TOKEN_GREATER_GREATER_EQUAL)) {
    enum TokenKind op_kind = parser->prev->kind;

    right_result = assignment(parser);
    if (!right_result.is_ok) {
      free_expr(&expr);
      return right_result;
    }

    right = right_result.as.expr;

    if (op_kind == TOKEN_EQUAL) {
      struct ExprAssign assignexp = {.lhs = ALLOC(expr), .rhs = ALLOC(right)};
      expr = (struct Expr){.kind = EXPR_ASSIGN, .as.assign = assignexp};
    } else {
      enum ExprBinKind bin_kind;

      if (op_kind == TOKEN_PLUS_EQUAL) {
        bin_kind = EXPR_BIN_ADD;
      } else if (op_kind == TOKEN_MINUS_EQUAL) {
        bin_kind = EXPR_BIN_SUB;
      } else if (op_kind == TOKEN_STAR_EQUAL) {
        bin_kind = EXPR_BIN_MUL;
      } else if (op_kind == TOKEN_SLASH_EQUAL) {
        bin_kind = EXPR_BIN_DIV;
      } else if (op_kind == TOKEN_AMPERSAND_EQUAL) {
        bin_kind = EXPR_BIN_BITWISE_AND;
      } else if (op_kind == TOKEN_PIPE_EQUAL) {
        bin_kind = EXPR_BIN_BITWISE_OR;
      } else if (op_kind == TOKEN_CARET_EQUAL) {
        bin_kind = EXPR_BIN_BITWISE_XOR;
      } else if (op_kind == TOKEN_LESS_LESS_EQUAL) {
        bin_kind = EXPR_BIN_SHIFT_LEFT;
      } else {
        bin_kind = EXPR_BIN_SHIFT_RIGHT;
      }

      struct ExprCompoundAssign comp;

      comp.kind = bin_kind;
      comp.lhs = ALLOC(expr);
      comp.rhs = ALLOC(right);

      expr = (struct Expr){.kind = EXPR_COMPOUND_ASSIGN,
                           .as.compound_assign = comp};
    }
  }

  return (struct ParseFnResult){.as.expr = expr, .is_ok = true, .msg = NULL};
}

static struct ParseFnResult parse_expr(struct Parser *parser)
{
  struct ParseFnResult res;
  struct Expr expr;

  res.is_ok = false;
  res.msg = NULL;

  res = assignment(parser);
  if (!res.is_ok) {
    return res;
  }

  expr = res.as.expr;

  return (struct ParseFnResult){.is_ok = true, .msg = NULL, .as.expr = expr};
}

static struct ParseFnResult parse_stmt(struct Parser *parser);

static struct ParseFnResult block(struct Parser *parser)
{
  VecStmt stmts = {0};
  struct ParseFnResult result;
  struct Token *token_lbrace, *token_rbrace;

  result.is_ok = true;
  result.msg = NULL;

  token_lbrace = consume(parser, TOKEN_LBRACE);
  if (!token_lbrace) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected '{'", .as.stmt = {0}};
  }

  while (!check(parser, TOKEN_RBRACE)) {
    struct ParseFnResult r;

    r = parse_stmt(parser);
    if (!r.is_ok) {
      return r;
    }

    vec_insert(&stmts, r.as.stmt);
  }

  token_rbrace = consume(parser, TOKEN_RBRACE);
  if (!token_rbrace) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected '}'", .as.stmt = {0}};
  }

  struct Stmt block_stmt;
  block_stmt.kind = STMT_BLOCK;
  block_stmt.as.block.stmts = stmts;

  result.as.stmt = block_stmt;

  return result;
}

static struct ParseFnResult parse_param_list(struct Parser *parser,
                                             VecParam *parameters,
                                             bool allow_variadic,
                                             bool *is_variadic)
{
  struct Token *token_lparen, *token_void, *token_rparen;

  token_lparen = consume(parser, TOKEN_LPAREN);
  if (!token_lparen) {
    return (struct ParseFnResult){
        .is_ok = false, .as.decl = {0}, .msg = "Expected token '(' after 'id'"};
  }

  if (check(parser, TOKEN_VOID)) {
    token_void = consume(parser, TOKEN_VOID);
    if (!token_void) {
      return (struct ParseFnResult){.is_ok = false,
                                    .as.decl = {0},
                                    .msg = "Expected token 'void' after '('"};
    }
  } else {
    while (!check(parser, TOKEN_RPAREN)) {
      if (allow_variadic && check(parser, TOKEN_ELLIPSIS)) {
        consume(parser, TOKEN_ELLIPSIS);
        *is_variadic = true;
        break;
      }

      struct Token *name_token, *colon_token;
      char *name;
      Type type;

      bool is_mut = match(parser, 1, TOKEN_MUT);

      name_token = consume(parser, TOKEN_IDENTIFIER);
      if (!name_token) {
        return (struct ParseFnResult){
            .is_ok = false,
            .as.decl = {0},
            .msg = "Expected `name: type` format for parameters"};
      }

      colon_token = consume(parser, TOKEN_COLON);
      if (!colon_token) {
        return (struct ParseFnResult){
            .is_ok = false,
            .as.decl = {0},
            .msg = "Expected `name: type` format for parameters"};
      }

      type = parse_type(parser);
      name = strndup(name_token->start, name_token->len);

      struct Parameter p;
      p.name = name;
      p.type = type;
      p.is_mut = is_mut;

      vec_insert(parameters, p);

      if (check(parser, TOKEN_COMMA)) {
        consume(parser, TOKEN_COMMA);
      }
    }
  }

  token_rparen = consume(parser, TOKEN_RPAREN);
  if (!token_rparen) {
    return (struct ParseFnResult){.is_ok = false,
                                  .as.decl = {0},
                                  .msg = "Expected token ')' after params"};
  }

  return (struct ParseFnResult){.is_ok = true, .msg = NULL, .as.decl = {0}};
}

static struct ParseFnResult parse_fn_signature(struct Parser *parser,
                                               bool is_extern)
{
  struct Token *token_fn, *token_id, *token_arrow;
  VecParam parameters = {0};
  bool is_variadic = false;

  token_fn = consume(parser, TOKEN_FN);
  if (!token_fn) {
    return (struct ParseFnResult){
        .is_ok = false, .as.decl = {0}, .msg = "Expected token 'fn'"};
  }

  token_id = consume(parser, TOKEN_IDENTIFIER);
  if (!token_id) {
    return (struct ParseFnResult){.is_ok = false,
                                  .as.decl = {0},
                                  .msg = "Expected token 'id' after 'fn'"};
  }

  char *fn_name = strndup(token_id->start, token_id->len);

  struct ParseFnResult params_res =
      parse_param_list(parser, &parameters, is_extern, &is_variadic);
  if (!params_res.is_ok) {
    free(fn_name);
    return params_res;
  }

  token_arrow = consume(parser, TOKEN_ARROW);
  if (!token_arrow) {
    free(fn_name);
    return (struct ParseFnResult){
        .is_ok = false, .as.decl = {0}, .msg = "Expected token '->' after ')'"};
  }

  Type retval = parse_type(parser);

  struct DeclFn fn;
  fn.name = fn_name;
  fn.params = parameters;
  fn.retval = retval;
  fn.body = (VecStmt){0};
  fn.is_extern = is_extern;
  fn.is_variadic = is_variadic;

  return (struct ParseFnResult){
      .is_ok = true,
      .msg = NULL,
      .as.decl = ((struct Decl){.kind = DECL_FN, .as.fn = fn})};
}

static struct ParseFnResult parse_fn_decl(struct Parser *parser)
{
  struct ParseFnResult result = parse_fn_signature(parser, false);
  if (!result.is_ok) {
    return result;
  }

  struct DeclFn *fn = &result.as.decl.as.fn;
  struct DeclFn *prev_fn = parser->current_fn;
  parser->current_fn = fn;

  struct ParseFnResult block_result = block(parser);

  parser->current_fn = prev_fn;

  if (!block_result.is_ok) {
    return block_result;
  }

  struct Stmt body = block_result.as.stmt;
  fn->body = body.as.block.stmts;

  return result;
}

static struct ParseFnResult parse_let_stmt(struct Parser *parser)
{
  struct ParseFnResult result, init_res;
  struct Token *token_let, *token_id, *token_colon, *token_equal,
      *token_semicolon;
  Type type;
  struct Expr init;

  result.is_ok = true;
  result.msg = NULL;

  token_let = consume(parser, TOKEN_LET);
  if (!token_let) {
    return (struct ParseFnResult){
        .is_ok = false, .as.stmt = {0}, .msg = "Expected token 'let'"};
  }

  bool is_mut = match(parser, 1, TOKEN_MUT);

  token_id = consume(parser, TOKEN_IDENTIFIER);
  if (!token_id) {
    return (struct ParseFnResult){.is_ok = false,
                                  .as.stmt = {0},
                                  .msg = "Expected identifier after 'let'"};
  }

  char *name = strndup(token_id->start, token_id->len);

  token_colon = consume(parser, TOKEN_COLON);
  if (!token_colon) {
    free(name);
    return (struct ParseFnResult){
        .is_ok = false,
        .as.stmt = {0},
        .msg = "Expected token ':' after identifier in let stmt"};
  }

  type = parse_type(parser);

  token_equal = consume(parser, TOKEN_EQUAL);
  if (!token_equal) {
    free(name);
    return (struct ParseFnResult){
        .is_ok = false,
        .as.stmt = {0},
        .msg = "Expected token '=' after type in let stmt"};
  }

  init_res = parse_expr(parser);
  if (!init_res.is_ok) {
    free(name);
    return (struct ParseFnResult){
        .is_ok = false,
        .as.stmt = {0},
        .msg = "Expected expr after equal in let stmt"};
  }

  init = init_res.as.expr;

  token_semicolon = consume(parser, TOKEN_SEMICOLON);
  if (!token_semicolon) {
    free(name);
    return (struct ParseFnResult){
        .is_ok = false,
        .as.stmt = {0},
        .msg = "Expected semicolon after init expr in let stmt"};
  }

  struct StmtLet let_stmt;

  let_stmt.name = name;
  let_stmt.type = type;
  let_stmt.init = ALLOC(init);
  let_stmt.is_mut = is_mut;
  result.as.stmt = (struct Stmt){.kind = STMT_LET, .as.let = let_stmt};

  return result;
}

static struct ParseFnResult parse_ret_stmt(struct Parser *parser)
{
  struct ParseFnResult result;
  struct Token *token_ret, *token_semicolon;
  struct Expr *expr;

  expr = NULL;

  result.is_ok = true;
  result.msg = NULL;

  token_ret = consume(parser, TOKEN_RET);
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
  ret_stmt.expected_retval = parser->current_fn->retval;

  struct Stmt s;
  s.kind = STMT_RET;
  s.as.ret = ret_stmt;

  result.as.stmt = s;

  return result;
}

static struct ParseFnResult parse_if_stmt(struct Parser *parser)
{
  struct ParseFnResult result, cond_res, then_res, else_res;
  struct Token *token_if, *token_lparen, *token_rparen, *token_else;
  struct Expr cond;
  struct Stmt *then_block, *else_block;

  result.is_ok = true;
  result.msg = NULL;

  token_if = consume(parser, TOKEN_IF);
  if (!token_if) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected 'if'", .as.stmt = {0}};
  }

  token_lparen = consume(parser, TOKEN_LPAREN);
  if (!token_lparen) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected '(' after 'if'", .as.stmt = {0}};
  }

  cond_res = parse_expr(parser);
  if (!cond_res.is_ok) {
    return cond_res;
  }

  cond = cond_res.as.expr;

  token_rparen = consume(parser, TOKEN_RPAREN);
  if (!token_rparen) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected ')' after 'if' cond", .as.stmt = {0}};
  }

  then_res = block(parser);
  if (!then_res.is_ok) {
    return then_res;
  }

  then_block = ALLOC(then_res.as.stmt);

  else_block = NULL;
  if (check(parser, TOKEN_ELSE)) {
    token_else = consume(parser, TOKEN_ELSE);
    if (!token_else) {
      return (struct ParseFnResult){
          .is_ok = false, .msg = "Expected 'else'", .as.stmt = {0}};
    }

    else_res = block(parser);
    if (!else_res.is_ok) {
      return else_res;
    }

    else_block = ALLOC(else_res.as.stmt);
  }

  struct StmtIf if_stmt;
  if_stmt.cond = cond;
  if_stmt.then_block = then_block;
  if_stmt.else_block = else_block;

  struct Stmt stmt;
  stmt.kind = STMT_IF;
  stmt.as.if_stmt = if_stmt;

  result.as.stmt = stmt;

  return result;
}

static struct ParseFnResult parse_do_while_stmt(struct Parser *parser)
{
  struct ParseFnResult result, body_result, cond_result;
  struct Token *token_do, *token_while, *token_lparen, *token_rparen,
      *token_semicolon;
  struct Stmt body;
  struct Expr cond;

  result.is_ok = true;
  result.msg = NULL;

  /* 1. Consume 'do' */
  token_do = consume(parser, TOKEN_DO);
  if (!token_do) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected 'do'", .as.stmt = {0}};
  }

  body_result = block(parser);
  if (!body_result.is_ok) {
    return body_result;
  }
  body = body_result.as.stmt;

  token_while = consume(parser, TOKEN_WHILE);
  if (!token_while) {
    return (struct ParseFnResult){.is_ok = false,
                                  .msg = "Expected 'while' after do block",
                                  .as.stmt = {0}};
  }

  token_lparen = consume(parser, TOKEN_LPAREN);
  if (!token_lparen) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected '(' after 'while'", .as.stmt = {0}};
  }

  cond_result = parse_expr(parser);
  if (!cond_result.is_ok) {
    return cond_result;
  }
  cond = cond_result.as.expr;

  token_rparen = consume(parser, TOKEN_RPAREN);
  if (!token_rparen) {
    return (struct ParseFnResult){
        .is_ok = false,
        .msg = "Expected ')' after do-while condition",
        .as.stmt = {0}};
  }

  token_semicolon = consume(parser, TOKEN_SEMICOLON);
  if (!token_semicolon) {
    return (struct ParseFnResult){
        .is_ok = false,
        .msg = "Expected ';' after do-while statement",
        .as.stmt = {0}};
  }

  struct StmtDoWhile do_while_stmt;
  do_while_stmt.body = ALLOC(body);
  do_while_stmt.cond = cond;
  do_while_stmt.label = NULL;

  struct Stmt s;
  s.kind = STMT_DO_WHILE;
  s.as.do_while_stmt = do_while_stmt;

  result.as.stmt = s;

  return result;
}

static struct ParseFnResult parse_while_stmt(struct Parser *parser)
{
  struct ParseFnResult result, cond_result, body_result;
  struct Token *token_while, *token_lparen, *token_rparen;
  struct Expr cond;
  struct Stmt body;

  result.is_ok = true;
  result.msg = NULL;

  token_while = consume(parser, TOKEN_WHILE);
  if (!token_while) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected 'while'", .as.stmt = {0}};
  }

  token_lparen = consume(parser, TOKEN_LPAREN);
  if (!token_lparen) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected '(' after while", .as.stmt = {0}};
  }

  cond_result = parse_expr(parser);
  if (!cond_result.is_ok) {
    return cond_result;
  }

  cond = cond_result.as.expr;

  token_rparen = consume(parser, TOKEN_RPAREN);
  if (!token_rparen) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected '(' after while cond", .as.stmt = {0}};
  }

  body_result = block(parser);
  if (!body_result.is_ok) {
    return body_result;
  }

  body = body_result.as.stmt;

  struct StmtWhile while_stmt;
  while_stmt.cond = cond;
  while_stmt.body = ALLOC(body);
  while_stmt.label = NULL;

  struct Stmt s;
  s.kind = STMT_WHILE;
  s.as.while_stmt = while_stmt;

  result.as.stmt = s;

  return result;
}

static struct ParseFnResult parse_loop_stmt(struct Parser *parser)
{
  struct ParseFnResult result, body_res;
  struct Token *token_loop;

  result.is_ok = true;
  result.msg = NULL;

  token_loop = consume(parser, TOKEN_LOOP);
  if (!token_loop) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected 'loop'", .as.stmt = {0}};
  }

  body_res = block(parser);
  if (!body_res.is_ok) {
    return body_res;
  }

  struct StmtLoop loop_stmt;
  loop_stmt.body = ALLOC(body_res.as.stmt);
  loop_stmt.label = NULL;

  struct Stmt s;
  s.kind = STMT_LOOP;
  s.as.loop = loop_stmt;

  result.as.stmt = s;

  return result;
}

static struct ParseFnResult parse_break_stmt(struct Parser *parser)
{
  struct ParseFnResult result;
  struct Token *token_break, *token_semicolon;

  result.is_ok = true;
  result.msg = NULL;

  token_break = consume(parser, TOKEN_BREAK);
  if (!token_break) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected 'break'", .as.stmt = {0}};
  }

  token_semicolon = consume(parser, TOKEN_SEMICOLON);
  if (!token_semicolon) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected ';' after 'break'", .as.stmt = {0}};
  }

  struct StmtBreak break_stmt = {0};
  break_stmt.label = "";

  result.as.stmt =
      (struct Stmt){.kind = STMT_BREAK, .as.break_stmt = break_stmt};

  return result;
}

static struct ParseFnResult parse_continue_stmt(struct Parser *parser)
{
  struct ParseFnResult result;
  struct Token *token_continue, *token_semicolon;

  result.is_ok = true;
  result.msg = NULL;

  token_continue = consume(parser, TOKEN_CONTINUE);
  if (!token_continue) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected 'break'", .as.stmt = {0}};
  }

  token_semicolon = consume(parser, TOKEN_SEMICOLON);
  if (!token_semicolon) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected ';' after 'continue'", .as.stmt = {0}};
  }

  struct StmtContinue continue_stmt = {0};
  continue_stmt.label = "";

  result.as.stmt =
      (struct Stmt){.kind = STMT_CONTINUE, .as.continue_stmt = continue_stmt};

  return result;
}

static struct ParseFnResult parse_variable_decl_after_let(struct Parser *parser,
                                                         bool is_extern)
{
  struct Token *token_id, *token_colon, *token_equal, *token_semicolon;
  bool is_mut = match(parser, 1, TOKEN_MUT);

  token_id = consume(parser, TOKEN_IDENTIFIER);
  if (!token_id) {
    return (struct ParseFnResult){.is_ok = false,
                                  .as.decl = {0},
                                  .msg = "Expected identifier after 'let'"};
  }

  char *name = strndup(token_id->start, token_id->len);

  token_colon = consume(parser, TOKEN_COLON);
  if (!token_colon) {
    free(name);
    return (struct ParseFnResult){
        .is_ok = false,
        .as.decl = {0},
        .msg = "Expected token ':' after identifier in variable declaration"};
  }

  Type type = parse_type(parser);
  struct Expr *init = NULL;

  if (!is_extern) {
    token_equal = consume(parser, TOKEN_EQUAL);
    if (!token_equal) {
      free(name);
      return (struct ParseFnResult){
          .is_ok = false,
          .as.decl = {0},
          .msg = "Expected token '=' after type in variable declaration"};
    }

    struct ParseFnResult init_res = parse_expr(parser);
    if (!init_res.is_ok) {
      free(name);
      return init_res;
    }
    init = ALLOC(init_res.as.expr);
  }

  token_semicolon = consume(parser, TOKEN_SEMICOLON);
  if (!token_semicolon) {
    free(name);
    if (init) {
      free_expr(init);
      free(init);
    }
    return (struct ParseFnResult){
        .is_ok = false,
        .as.decl = {0},
        .msg = "Expected ';' at the end of variable declaration"};
  }

  struct DeclVariable variable;
  variable.name = name;
  variable.type = type;
  variable.init = init;
  variable.is_mut = is_mut;
  variable.is_extern = is_extern;

  return (struct ParseFnResult){
      .is_ok = true,
      .msg = NULL,
      .as.decl = ((struct Decl){.kind = DECL_VARIABLE, .as.variable = variable})};
}

static struct ParseFnResult parse_variable_decl(struct Parser *parser)
{
  struct Token *token_let = consume(parser, TOKEN_LET);
  if (!token_let) {
    return (struct ParseFnResult){
        .is_ok = false, .as.decl = {0}, .msg = "Expected token 'let'"};
  }

  return parse_variable_decl_after_let(parser, false);
}

static struct ParseFnResult parse_extern_decl(struct Parser *parser)
{
  struct Token *token_extern, *token_semicolon;

  token_extern = consume(parser, TOKEN_EXTERN);
  if (!token_extern) {
    return (struct ParseFnResult){
        .is_ok = false, .as.decl = {0}, .msg = "Expected 'extern'"};
  }

  if (check(parser, TOKEN_FN)) {
    struct ParseFnResult result = parse_fn_signature(parser, true);
    if (!result.is_ok) {
      return result;
    }

    token_semicolon = consume(parser, TOKEN_SEMICOLON);
    if (!token_semicolon) {
      return (struct ParseFnResult){
          .is_ok = false,
          .msg = "Expected ';' at the end of extern fn declaration",
          .as.decl = {0}};
    }

    return result;
  }

  if (check(parser, TOKEN_LET)) {
    consume(parser, TOKEN_LET);
    return parse_variable_decl_after_let(parser, true);
  }

  return (struct ParseFnResult){
      .is_ok = false,
      .as.decl = {0},
      .msg = "Expected 'fn' or 'let' after 'extern'"};
}

static struct ParseFnResult parse_expr_stmt(struct Parser *parser)
{
  struct ParseFnResult result, expr_res;
  struct Token *token_semicolon;
  struct Expr expr;

  result.is_ok = true;
  result.msg = NULL;

  expr_res = parse_expr(parser);
  if (!expr_res.is_ok) {
    return expr_res;
  }

  expr = expr_res.as.expr;

  token_semicolon = consume(parser, TOKEN_SEMICOLON);
  if (!token_semicolon) {
    return (struct ParseFnResult){.is_ok = false,
                                  .msg = "Expected ';' at the end of expr stmt",
                                  .as.stmt = {0}};
  }

  struct StmtExpr expr_stmt;
  expr_stmt.expr = expr;

  struct Stmt s;
  s.kind = STMT_EXPR;
  s.as.expr_stmt = expr_stmt;

  result.as.stmt = s;

  return result;
}

static struct ParseFnResult parse_record_decl(struct Parser *parser)
{
  struct ParseFnResult result;
  struct Token *token_struct_or_union, *token_id, *token_lbrace, *token_rbrace;
  VecStructField fields = {0};

  result.is_ok = true;
  result.msg = NULL;

  token_struct_or_union = consume_any(parser, 2, TOKEN_STRUCT, TOKEN_UNION);
  if (!token_struct_or_union) {
    return (struct ParseFnResult){
        .is_ok = false, .as.decl = {0}, .msg = "Expected token 'struct'"};
  }

  bool is_union = (token_struct_or_union->kind == TOKEN_UNION);

  token_id = consume(parser, TOKEN_IDENTIFIER);
  if (!token_id) {
    return (struct ParseFnResult){.is_ok = false,
                                  .as.decl = {0},
                                  .msg = "Expected identifier after record"};
  }

  char *name = strndup(token_id->start, token_id->len);

  token_lbrace = consume(parser, TOKEN_LBRACE);
  if (!token_lbrace) {
    free(name);
    return (struct ParseFnResult){.is_ok = false,
                                  .as.decl = {0},
                                  .msg = "Expected '{' in record declaration"};
  }

  while (!check(parser, TOKEN_RBRACE)) {
    char *field_name = NULL;
    Type field_type;

    /* Handle both `a: i8` and `union { ... } as;` gracefully */
    if (check(parser, TOKEN_IDENTIFIER) && check_next(parser, TOKEN_COLON)) {
      struct Token *name_tok = consume(parser, TOKEN_IDENTIFIER);
      consume(parser, TOKEN_COLON);
      field_type = parse_type(parser);
      field_name = strndup(name_tok->start, name_tok->len);
    } else {
      field_type = parse_type(parser);
      struct Token *name_tok = consume(parser, TOKEN_IDENTIFIER);
      if (name_tok) {
        field_name = strndup(name_tok->start, name_tok->len);
      } else {
        field_name = strdup("");
      }
    }

    struct StructField field;
    field.name = field_name;
    field.type = field_type;
    field.offset = 0;
    vec_insert(&fields, field);

    if (check(parser, TOKEN_COMMA)) {
      advance_parser(parser);
    }
  }

  token_rbrace = consume(parser, TOKEN_RBRACE);
  if (!token_rbrace) {
    return (struct ParseFnResult){.is_ok = false,
                                  .as.decl = {0},
                                  .msg = "Expected '}' after record fields"};
  }

  if (is_union) {
    struct DeclUnion union_decl;
    union_decl.name = name;
    union_decl.fields = fields;
    result.as.decl =
        (struct Decl){.kind = DECL_UNION, .as.union_decl = union_decl};
  } else {
    struct DeclStruct struct_decl;
    struct_decl.name = name;
    struct_decl.fields = fields;
    result.as.decl =
        (struct Decl){.kind = DECL_STRUCT, .as.struct_decl = struct_decl};
  }

  return result;
}

static struct ParseFnResult parse_goto_stmt(struct Parser *parser)
{
  struct ParseFnResult result = {.is_ok = true, .msg = NULL};

  struct Token *token_goto, *token_label, *token_semicolon;

  token_goto = consume(parser, TOKEN_GOTO);
  if (!token_goto) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected 'goto'", .as.stmt = {0}};
  }

  token_label = consume(parser, TOKEN_IDENTIFIER);
  if (!token_label) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Expected ident after goto", .as.stmt = {0}};
  }

  token_semicolon = consume(parser, TOKEN_SEMICOLON);
  if (!token_semicolon) {
    return (struct ParseFnResult){
        .is_ok = false,
        .msg = "Expected ';' at the end of 'goto' stmt",
        .as.stmt = {0}};
  }

  struct StmtGoto goto_stmt;
  goto_stmt.label = strndup(token_label->start, token_label->len);

  struct Stmt s;
  s.kind = STMT_GOTO;
  s.as.goto_stmt = goto_stmt;

  result.as.stmt = s;

  return result;
}

static struct ParseFnResult parse_enum_decl(struct Parser *parser)
{
  struct ParseFnResult result = {.is_ok = true, .msg = NULL};

  consume(parser, TOKEN_ENUM);

  char *name;
  if (check(parser, TOKEN_IDENTIFIER)) {
    struct Token *id = consume(parser, TOKEN_IDENTIFIER);
    name = strndup(id->start, id->len);
  } else {
    name = mkuniq("anon_enum");
  }
  enum_type_insert(name);

  consume(parser, TOKEN_LBRACE);

  VecEnumVariant variants = {0};
  if (!parse_enum_body(parser, &variants)) {
    return (struct ParseFnResult){
        .is_ok = false, .msg = "Failed to parse enum body", .as.decl = {0}};
  }

  consume(parser, TOKEN_RBRACE);

  struct DeclEnum enum_decl;
  enum_decl.name = name;
  enum_decl.variants = variants;

  result.as.decl =
      (struct Decl){.kind = DECL_ENUM, .as.enum_decl = enum_decl};
  return result;
}

static struct ParseFnResult parse_stmt(struct Parser *parser)
{
  struct ParseFnResult result;

  result.is_ok = true;
  result.msg = NULL;

  if (check(parser, TOKEN_IDENTIFIER) && check_next(parser, TOKEN_COLON)) {
    struct Token *token_label, *token_colon;

    token_label = consume(parser, TOKEN_IDENTIFIER);
    if (!token_label) {
      return (struct ParseFnResult){
          .is_ok = false, .msg = "Expected label", .as.stmt = {0}};
    }

    token_colon = consume(parser, TOKEN_COLON);
    if (!token_colon) {
      return (struct ParseFnResult){
          .is_ok = false, .msg = "Expected ':' after label", .as.stmt = {0}};
    }

    char *label;

    label = strndup(token_label->start, token_label->len);

    struct ParseFnResult labeled_res;

    labeled_res = parse_stmt(parser);
    if (!labeled_res.is_ok) {
      return labeled_res;
    }

    struct StmtLabeled labeled;

    labeled.label = label;
    labeled.stmt = ALLOC(labeled_res.as.stmt);

    return (struct ParseFnResult){
        .is_ok = true,
        .msg = NULL,
        .as.stmt =
            ((struct Stmt){.kind = STMT_LABELED, .as.labeled = labeled})};
  }

  switch (parser->curr->kind) {
    case TOKEN_LET: {
      struct ParseFnResult let_res = parse_let_stmt(parser);
      if (!let_res.is_ok) {
        return let_res;
      }
      result.as.stmt = let_res.as.stmt;
      break;
    }
    case TOKEN_RET: {
      struct ParseFnResult ret_res = parse_ret_stmt(parser);
      if (!ret_res.is_ok) {
        return ret_res;
      }
      result.as.stmt = ret_res.as.stmt;
      break;
    }
    case TOKEN_IF: {
      struct ParseFnResult if_res = parse_if_stmt(parser);
      if (!if_res.is_ok) {
        return if_res;
      }
      result.as.stmt = if_res.as.stmt;
      break;
    }
    case TOKEN_DO: {
      struct ParseFnResult do_res = parse_do_while_stmt(parser);
      if (!do_res.is_ok) {
        return do_res;
      }
      result.as.stmt = do_res.as.stmt;
      break;
    }
    case TOKEN_WHILE: {
      struct ParseFnResult while_res = parse_while_stmt(parser);
      if (!while_res.is_ok) {
        return while_res;
      }
      result.as.stmt = while_res.as.stmt;
      break;
    }
    case TOKEN_LOOP: {
      struct ParseFnResult loop_res = parse_loop_stmt(parser);
      if (!loop_res.is_ok) {
        return loop_res;
      }
      result.as.stmt = loop_res.as.stmt;
      break;
    }
    case TOKEN_BREAK: {
      struct ParseFnResult break_res = parse_break_stmt(parser);
      if (!break_res.is_ok) {
        return break_res;
      }
      result.as.stmt = break_res.as.stmt;
      break;
    }
    case TOKEN_CONTINUE: {
      struct ParseFnResult continue_res = parse_continue_stmt(parser);
      if (!continue_res.is_ok) {
        return continue_res;
      }
      result.as.stmt = continue_res.as.stmt;
      break;
    }
    case TOKEN_GOTO: {
      struct ParseFnResult goto_res = parse_goto_stmt(parser);
      if (!goto_res.is_ok) {
        return goto_res;
      }
      result.as.stmt = goto_res.as.stmt;
      break;
    }
    case TOKEN_LBRACE: {
      struct ParseFnResult block_res = block(parser);
      if (!block_res.is_ok) {
        return block_res;
      }
      result.as.stmt = block_res.as.stmt;
      break;
    }
    default: {
      struct ParseFnResult expr_stmt_res = parse_expr_stmt(parser);
      if (!expr_stmt_res.is_ok) {
        return expr_stmt_res;
      }
      result.as.stmt = expr_stmt_res.as.stmt;
      break;
    }
  }

  return result;
}

static struct ParseFnResult parse_decl(struct Parser *parser)
{
  switch (parser->curr->kind) {
    case TOKEN_FN:
      return parse_fn_decl(parser);
    case TOKEN_EXTERN:
      return parse_extern_decl(parser);
    case TOKEN_STRUCT:
    case TOKEN_UNION:
      return parse_record_decl(parser);
    case TOKEN_ENUM:
      return parse_enum_decl(parser);
    case TOKEN_LET:
      return parse_variable_decl(parser);
    default:
      return (struct ParseFnResult){
          .is_ok = false,
          .msg = "Expected declaration",
          .as.decl = {0},
      };
  }
}

struct ParseResult parse(struct Parser *parser)
{
  struct ParseResult result;

  result.is_ok = true;
  result.msg = NULL;

  result.ast = malloc(sizeof(struct AST));
  result.ast->decls = (VecDecl){0};

  parser->global_decls = &result.ast->decls;

  while (parser->curr->kind != TOKEN_EOF) {
    struct ParseFnResult r;

    r = parse_decl(parser);
    if (!r.is_ok) {
      return (struct ParseResult){.is_ok = false, .msg = r.msg, .ast = NULL};
    }
    vec_insert(&result.ast->decls, r.as.decl);
  }

  return result;
}

#if defined(DEBUG_PARSER) || defined(DEBUG_RESOLVER) || defined(DEBUG_TYPECHECKER) || defined(DEBUG_LABELER)
void print_type(Type *type, int spaces)
{
  switch (type->kind) {
    case STRUCT_T:
      printf("%s", type->as.struct_name);
      break;
    case VOID_T:
      printf("void");
      break;
    case PTR_T:
      printf("*");
      print_type(type->as.base, spaces);
      break;
    case U8_T:
      printf("u8");
      break;
    case U16_T:
      printf("u16");
      break;
    case U32_T:
      printf("u32");
      break;
    case U64_T:
      printf("u64");
      break;
    case I8_T:
      printf("i8");
      break;
    case I16_T:
      printf("i16");
      break;
    case I32_T:
      printf("i32");
      break;
    case I64_T:
      printf("i64");
      break;
    case F32_T:
      printf("f32");
      break;
    case F64_T:
      printf("f64");
      break;
    case BOOL_T:
      printf("bool");
      break;
    case STR_T:
      printf("str");
      break;
    case FN_T: {
      printf("fn(\n");

      print_indent(spaces + 2);
      printf("args: [\n");
      for (int i = 0; i < type->as.func.params.len; i++) {
        print_indent(spaces + 4);
        print_type(&type->as.func.params.data[i], spaces + 4);
        printf(",\n");
      }

      if (type->as.func.is_variadic) {
        print_indent(spaces + 4);
        printf("...\n");
      }

      print_indent(spaces + 2);
      printf("],\n");

      print_indent(spaces + 2);
      printf("retval: ");
      if (type->as.func.retval) {
        print_type(type->as.func.retval, spaces + 2);
      } else {
        printf("void");
      }
      printf("\n");

      print_indent(spaces);
      printf(")");
      break;
    }
    case UNKNOWN_T:
      printf("unknown");
      break;
    default:
      assert(0);
  }
}

static void print_binary_op(enum ExprBinKind kind)
{
  switch (kind) {
    case EXPR_BIN_ADD:
      printf("ADD");
      break;
    case EXPR_BIN_SUB:
      printf("SUB");
      break;
    case EXPR_BIN_MUL:
      printf("MUL");
      break;
    case EXPR_BIN_DIV:
      printf("DIV");
      break;
    case EXPR_BIN_LESS:
      printf("LESS");
      break;
    case EXPR_BIN_GREATER:
      printf("GREATER");
      break;
    case EXPR_BIN_LESS_EQUAL:
      printf("LESS EQUAL");
      break;
    case EXPR_BIN_GREATER_EQUAL:
      printf("GREATER EQUAL");
      break;
    case EXPR_BIN_EQUAL_EQUAL:
      printf("EQUAL EQUAL");
      break;
    case EXPR_BIN_BANG_EQUAL:
      printf("BANG EQUAL");
      break;
    case EXPR_BIN_BITWISE_AND:
      printf("BITWISE AND");
      break;
    case EXPR_BIN_BITWISE_XOR:
      printf("BITWISE XOR");
      break;
    case EXPR_BIN_BITWISE_OR:
      printf("BITWISE OR");
      break;
    case EXPR_BIN_SHIFT_LEFT:
      printf("SHIFT LEFT");
      break;
    case EXPR_BIN_SHIFT_RIGHT:
      printf("SHIFT RIGHT");
      break;
    case EXPR_BIN_LOGICAL_AND:
      printf("LOGICAL AND");
      break;
    case EXPR_BIN_LOGICAL_OR:
      printf("LOGICAL OR");
      break;
    default:
      assert(0);
  }
}

void print_expr(struct Expr *expr, int spaces)
{
  switch (expr->kind) {
    case EXPR_LITERAL: {
      if (expr->as.literal.kind == LITERAL_BOOL) {
        printf("Literal(\n");
        print_indent(spaces + 2);
        printf("v: %s,\n", expr->as.literal.as.boolean ? "true" : "false");
        printf("\n");
        print_indent(spaces);
        printf(")");
      } else if (expr->as.literal.kind == LITERAL_NUM) {
        printf("Literal(\n");
        print_indent(spaces + 2);

        switch (expr->as.literal.type.kind) {
          case I8_T: {
            printf("v: %d,\n", expr->as.literal.as.i8);
            break;
          }
          case U8_T: {
            printf("v: %d,\n", expr->as.literal.as.u8);
            break;
          }
          case I16_T: {
            printf("v: %d,\n", expr->as.literal.as.i16);
            break;
          }
          case U16_T: {
            printf("v: %d,\n", expr->as.literal.as.u16);
            break;
          }
          case I32_T: {
            printf("v: %d,\n", expr->as.literal.as.i32);
            break;
          }
          case U32_T: {
            printf("v: %d,\n", expr->as.literal.as.u32);
            break;
          }
          case I64_T: {
            printf("v: %lld,\n", expr->as.literal.as.i64);
            break;
          }
          case U64_T: {
            printf("v: %llu,\n", expr->as.literal.as.u64);
            break;
          }
          case F32_T: {
            printf("v: %f,\n", expr->as.literal.as.f32);
            break;
          }
          case F64_T: {
            printf("v: %f,\n", expr->as.literal.as.f64);
            break;
          }
          default:
            assert(0);
        }
        print_indent(spaces + 2);
        printf("type: ");
        print_type(&expr->as.literal.type, spaces);
        printf("\n");
        print_indent(spaces);
        printf(")");
      } else {
        printf("Literal(\"%s\")", expr->as.literal.as.str);
      }
      break;
    }
    case EXPR_VARIABLE: {
      printf("Variable(%s)", expr->as.var.name);
      break;
    }
    case EXPR_UNARY: {
      printf("Unary(\n");

      print_indent(spaces + 2);
      printf("expr = ");
      print_expr(expr->as.unary.expr, spaces + 4);
      printf(",\n");

      print_indent(spaces);
      printf(")");

      break;
    }
    case EXPR_BINARY: {
      printf("Binary(\n");

      print_indent(spaces + 2);
      printf("lhs = ");
      print_expr(expr->as.binary.lhs, spaces + 4);
      printf(",\n");

      print_indent(spaces + 2);
      printf("rhs = ");
      print_expr(expr->as.binary.rhs, spaces + 4);
      printf(",\n");

      print_indent(spaces + 2);
      printf("kind = ");
      print_binary_op(expr->as.binary.kind);
      printf(",\n");

      print_indent(spaces + 2);
      printf("type = ");
      print_type(&expr->type, spaces);
      printf(",\n");

      print_indent(spaces);
      printf(")");
      break;
    }
    case EXPR_ASSIGN: {
      printf("Assign(\n");

      print_indent(spaces + 2);
      printf("lhs = ");
      print_expr(expr->as.assign.lhs, spaces + 4);
      printf(",\n");

      print_indent(spaces + 2);
      printf("rhs = ");
      print_expr(expr->as.assign.rhs, spaces + 4);
      printf(",\n");

      print_indent(spaces);
      printf(")");
      break;
    }
    case EXPR_COMPOUND_ASSIGN: {
      printf("CompoundAssign(\n");
      print_indent(spaces + 2);
      printf("op = ");
      print_binary_op(expr->as.compound_assign.kind);
      printf(",\n");
      print_indent(spaces + 2);
      printf("lhs = ");
      print_expr(expr->as.compound_assign.lhs, spaces + 4);
      printf(",\n");
      print_indent(spaces + 2);
      printf("rhs = ");
      print_expr(expr->as.compound_assign.rhs, spaces + 4);
      printf(",\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case EXPR_CALL: {
      printf("Call(\n");

      print_indent(spaces + 2);
      printf("target = ");
      print_expr(expr->as.call.target, spaces + 2);
      printf(",\n");

      print_indent(spaces + 2);
      printf("arguments: [\n");

      for (int i = 0; i < expr->as.call.arguments.len; i++) {
        print_indent(spaces + 4);
        print_expr(&expr->as.call.arguments.data[i], spaces + 4);
        printf(",\n");
      }

      print_indent(spaces + 2);
      printf("]\n");

      print_indent(spaces);
      printf(")");
      break;
    }
    case EXPR_SIZEOF: {
      printf("SizeOf(\n");
      print_indent(spaces + 2);
      print_expr(expr->as.sizeof_expr.expr, spaces + 2);
      printf("\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case EXPR_ADDROF: {
      printf("AddrOf(\n");
      print_indent(spaces + 2);
      print_expr(expr->as.addrof.expr, spaces + 2);
      printf("\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case EXPR_DEREF: {
      printf("Deref(\n");
      print_indent(spaces + 2);
      print_expr(expr->as.deref.expr, spaces + 2);
      printf("\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case EXPR_CAST: {
      printf("Cast(\n");
      print_indent(spaces + 2);
      print_expr(expr->as.cast.expr, spaces + 2);
      printf("\n");
      print_indent(spaces + 2);
      printf("target_type: ");
      print_type(&expr->as.cast.target_type, spaces + 2);
      printf("\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case EXPR_STRUCT_INIT: {
      printf("StructInit(\n");
      print_indent(spaces + 2);
      printf("name = %s,\n", expr->as.struct_init.struct_name);
      print_indent(spaces + 2);
      printf("values = [\n");
      for (int i = 0; i < expr->as.struct_init.values.len; i++) {
        print_indent(spaces + 4);
        print_expr(expr->as.struct_init.values.data[i].expr, spaces + 4);
        printf(",\n");
      }
      print_indent(spaces + 2);
      printf("]\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case EXPR_MEMBER: {
      printf("Member(\n");
      print_indent(spaces + 2);
      printf("target = ");
      print_expr(expr->as.member.target, spaces + 4);
      printf(",\n");
      print_indent(spaces + 2);
      printf("field = %s,\n", expr->as.member.field_name);
      print_indent(spaces + 2);
      printf("is_arrow = %s\n", expr->as.member.is_arrow ? "true" : "false");
      print_indent(spaces);
      printf(")");
      break;
    }
    default:
      assert(0);
  }
}

static void print_params(VecParam *params, int spaces)
{
  print_indent(spaces);
  printf("params = [\n");
  for (int i = 0; i < params->len; i++) {
    print_indent(spaces + 2);
    printf("%s: ", params->data[i].name);
    print_type(&params->data[i].type, 0);
    printf(",\n");
  }
  print_indent(spaces);
  printf("],\n");
}

static void print_fields(VecStructField *fields, int spaces)
{
  print_indent(spaces);
  printf("fields = [\n");
  for (int i = 0; i < fields->len; i++) {
    print_indent(spaces + 2);
    printf("Field(name: %s, type: ", fields->data[i].name);
    print_type(&fields->data[i].type, spaces + 4);
    printf(", offset: %d),\n", fields->data[i].offset);
  }
  print_indent(spaces);
  printf("]\n");
}

void print_stmt(struct Stmt *stmt, int spaces)
{
  switch (stmt->kind) {
    case STMT_LET: {
      print_indent(spaces);
      printf("STMT_LET(\n");

      print_indent(spaces + 2);
      printf("name = %s,\n", stmt->as.let.name);

      print_indent(spaces + 2);
      printf("type = ");
      print_type(&stmt->as.let.type, spaces + 4);
      printf(",\n");

      print_indent(spaces + 2);
      printf("init = ");
      print_expr(stmt->as.let.init, spaces + 2);
      printf("\n");

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_RET: {
      print_indent(spaces);
      printf("STMT_RET(\n");

      print_indent(spaces + 2);
      printf("val = ");
      if (stmt->as.ret.val) {
        print_expr(stmt->as.ret.val, spaces + 2);
      } else {
        printf("NULL");
      }
      printf("\n");

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_IF: {
      print_indent(spaces);
      printf("STMT_IF(\n");

      print_indent(spaces + 2);
      printf("cond = ");
      print_expr(&stmt->as.if_stmt.cond, spaces + 2);
      printf(",\n");

      print_indent(spaces + 2);
      printf("then = \n");
      print_stmt(stmt->as.if_stmt.then_block, spaces + 2);

      if (stmt->as.if_stmt.else_block) {
        print_indent(spaces + 2);
        printf("else = \n");
        print_stmt(stmt->as.if_stmt.else_block, spaces + 4);
      }

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_DO_WHILE: {
      print_indent(spaces);
      printf("STMT_DO_WHILE(\n");

      print_indent(spaces + 2);
      printf("label = %s,\n", stmt->as.do_while_stmt.label
                                  ? stmt->as.do_while_stmt.label
                                  : "NULL");

      print_indent(spaces + 2);
      printf("cond = ");
      print_expr(&stmt->as.do_while_stmt.cond, spaces + 2);
      printf(",\n");

      print_indent(spaces + 2);
      printf("body = \n");
      print_stmt(stmt->as.do_while_stmt.body, spaces + 2);

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_WHILE: {
      print_indent(spaces);
      printf("STMT_WHILE(\n");

      print_indent(spaces + 2);
      printf("label = %s,\n",
             stmt->as.while_stmt.label ? stmt->as.while_stmt.label : "NULL");

      print_indent(spaces + 2);
      printf("cond = ");
      print_expr(&stmt->as.while_stmt.cond, spaces + 2);
      printf(",\n");

      print_indent(spaces + 2);
      printf("body = \n");
      print_stmt(stmt->as.while_stmt.body, spaces + 2);

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_LOOP: {
      print_indent(spaces);
      printf("STMT_LOOP(\n");

      print_indent(spaces + 2);
      printf("body = \n");
      print_stmt(stmt->as.loop.body, spaces + 2);

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_BREAK: {
      print_indent(spaces);
      printf("STMT_BREAK(\n");

      print_indent(spaces + 2);
      printf("label = %s\n",
             stmt->as.break_stmt.label ? stmt->as.break_stmt.label : "NULL");

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_CONTINUE: {
      print_indent(spaces);
      printf("STMT_CONTINUE(\n");

      print_indent(spaces + 2);
      printf("label = %s\n", stmt->as.continue_stmt.label
                                 ? stmt->as.continue_stmt.label
                                 : "NULL");

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_BLOCK: {
      print_indent(spaces);
      printf("STMT_BLOCK(\n");

      print_indent(spaces + 2);
      printf("body = [\n");
      for (int i = 0; i < stmt->as.block.stmts.len; i++) {
        print_stmt(&stmt->as.block.stmts.data[i], spaces + 4);
      }

      print_indent(spaces + 2);
      printf("]\n");

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_EXPR: {
      print_indent(spaces);
      printf("STMT_EXPR(\n");

      print_indent(spaces + 2);
      printf("expr = ");
      print_expr(&stmt->as.expr_stmt.expr, spaces + 2);
      printf("\n");

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_LABELED: {
      print_indent(spaces);
      printf("STMT_LABELED(\n");
      print_stmt(stmt->as.labeled.stmt, spaces + 2);
      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_GOTO: {
      print_indent(spaces);
      printf("STMT_GOTO(\n");
      print_indent(spaces + 2);
      printf("label = \"%s\",\n", stmt->as.goto_stmt.label);
      print_indent(spaces);
      printf(")\n");
      break;
    }
    default:
      assert(0 && "Unhandled statement kind in print_stmt");
  }
}

void print_decl(struct Decl *decl, int spaces)
{
  switch (decl->kind) {
    case DECL_FN: {
      print_indent(spaces);
      printf("DECL_FN(\n");

      print_indent(spaces + 2);
      printf("name = %s,\n", decl->as.fn.name);

      print_indent(spaces + 2);
      printf("is_extern = %s,\n", decl->as.fn.is_extern ? "true" : "false");
      print_indent(spaces + 2);
      printf("is_variadic = %s,\n",
             decl->as.fn.is_variadic ? "true" : "false");

      print_params(&decl->as.fn.params, spaces + 2);

      print_indent(spaces + 2);
      printf("body = [\n");
      for (int i = 0; i < decl->as.fn.body.len; i++) {
        print_stmt(&decl->as.fn.body.data[i], spaces + 4);
      }
      print_indent(spaces + 2);
      printf("],\n");

      print_indent(spaces + 2);
      printf("retval = ");
      print_type(&decl->as.fn.retval, spaces + 4);
      printf("\n");

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case DECL_STRUCT: {
      print_indent(spaces);
      printf("DECL_STRUCT(\n");
      print_indent(spaces + 2);
      printf("name = %s,\n", decl->as.struct_decl.name);
      print_fields(&decl->as.struct_decl.fields, spaces + 2);
      print_indent(spaces);
      printf(")\n");
      break;
    }
    case DECL_UNION: {
      print_indent(spaces);
      printf("DECL_UNION(\n");
      print_indent(spaces + 2);
      printf("name = %s,\n", decl->as.union_decl.name);
      print_fields(&decl->as.union_decl.fields, spaces + 2);
      print_indent(spaces);
      printf(")\n");
      break;
    }
    case DECL_VARIABLE: {
      print_indent(spaces);
      printf("DECL_VARIABLE(\n");
      print_indent(spaces + 2);
      printf("name = %s,\n", decl->as.variable.name);
      print_indent(spaces + 2);
      printf("is_extern = %s,\n",
             decl->as.variable.is_extern ? "true" : "false");
      print_indent(spaces + 2);
      printf("type = ");
      print_type(&decl->as.variable.type, spaces + 4);
      printf(",\n");
      if (decl->as.variable.init) {
        print_indent(spaces + 2);
        printf("init = ");
        print_expr(decl->as.variable.init, spaces + 2);
        printf("\n");
      }
      print_indent(spaces);
      printf(")\n");
      break;
    }
    case DECL_ENUM: {
      print_indent(spaces);
      printf("DECL_ENUM(\n");
      print_indent(spaces + 2);
      printf("name = %s,\n", decl->as.enum_decl.name);
      print_indent(spaces + 2);
      printf("variants = [\n");
      for (int i = 0; i < decl->as.enum_decl.variants.len; i++) {
        print_indent(spaces + 4);
        printf("%s = %d,\n", decl->as.enum_decl.variants.data[i].name,
               decl->as.enum_decl.variants.data[i].value);
      }
      print_indent(spaces + 2);
      printf("]\n");
      print_indent(spaces);
      printf(")\n");
      break;
    }
    default:
      assert(0 && "Unhandled declaration kind in print_decl");
  }
}

void print_ast(struct AST *ast)
{
  for (int i = 0; i < ast->decls.len; i++) {
    print_decl(&ast->decls.data[i], 0);
  }
}
#endif 
