#include "typechecker.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "util.h"

struct StructTable *struct_table = NULL;
struct EnumTypeItem *enum_types = NULL;
struct EnumVariantItem *enum_variants = NULL;

void struct_insert(struct StructTable **table, struct StructDef def)
{
  struct StructTable *node;

  node = malloc(sizeof(struct StructTable));
  node->def = def;
  node->def.name = strdup(def.name);
  node->next = *table;

  *table = node;
}

struct StructDef *struct_get(struct StructTable *table, char *name)
{
  while (table) {
    if (strcmp(table->def.name, name) == 0) {
      return &table->def;
    }
    table = table->next;
  }
  return NULL;
}

void free_enum_types(void)
{
  struct EnumTypeItem *curr_t = enum_types;
  while (curr_t) {
    struct EnumTypeItem *tmp = curr_t;
    curr_t = curr_t->next;
    free(tmp->name);
    free(tmp);
  }
  enum_types = NULL;
}

void enum_type_insert(char *name)
{
  struct EnumTypeItem *item;

  item = malloc(sizeof(*item));
  item->name = strdup(name);
  item->next = enum_types;
  enum_types = item;
}

void free_enum_variants(void)
{
  struct EnumVariantItem *curr_v = enum_variants;
  while (curr_v) {
    struct EnumVariantItem *tmp = curr_v;
    curr_v = curr_v->next;
    free(tmp->name);
    free(tmp);
  }
  enum_variants = NULL;
}

void enum_variant_insert(char *name, int value)
{
  struct EnumVariantItem *item;

  item = malloc(sizeof(*item));
  item->name = strdup(name);
  item->value = value;
  item->next = enum_variants;
  enum_variants = item;
}

bool enum_variant_get(char *name, int *out_val)
{
  struct EnumVariantItem *curr;

  curr = enum_variants;
  while (curr) {
    if (strcmp(curr->name, name) == 0) {
      *out_val = curr->value;
      return true;
    }
    curr = curr->next;
  }
  return false;
}

Type clone_type(Type t)
{
  Type copy = t;
  if (t.kind == PTR_T) {
    copy.as.base = malloc(sizeof(Type));
    *copy.as.base = clone_type(*t.as.base);
  } else if (t.kind == STRUCT_T) {
    copy.as.struct_name = strdup(t.as.struct_name);
  }
  return copy;
}

void free_type(Type *t)
{
  switch (t->kind) {
    case U8_T:
    case U16_T:
    case U32_T:
    case U64_T:
    case I8_T:
    case I16_T:
    case I32_T:
    case I64_T:
    case F32_T:
    case F64_T:
    case STR_T:
    case BOOL_T:
    case VOID_T:
    case UNKNOWN_T:
      break;
    case PTR_T: {
      free_type(t->as.base);
      free(t->as.base);
      break;
    }
    case STRUCT_T: {
      free(t->as.struct_name);
      break;
    }
    case FN_T: {
      for (int i = 0; i < t->as.func.params.len; i++) {
        free_type(&t->as.func.params.data[i]);
      }
      vec_free(&t->as.func.params);

      if (t->as.func.retval) {
        free_type(t->as.func.retval);
        free(t->as.func.retval);
      }
      break;
    }
    default:
      assert(0);
  }
}

static bool vectype_equal(VecType a, VecType b)
{
  if (a.len != b.len) {
    return false;
  }

  for (int i = 0; i < a.len; i++) {
    if (!types_equal(a.data[i], b.data[i])) {
      return false;
    }
  }

  return true;
}

bool types_equal(Type a, Type b)
{
  if (a.kind != b.kind) {
    return false;
  }

  switch (a.kind) {
    case STRUCT_T:
      return strcmp(a.as.struct_name, b.as.struct_name) == 0;
    case PTR_T:
      return types_equal(*a.as.base, *b.as.base);
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
    case STR_T:
    case BOOL_T:
    case UNKNOWN_T:
      return true;

    case FN_T: {
      if (a.as.func.retval == NULL || b.as.func.retval == NULL) {
        return a.as.func.retval == b.as.func.retval;
      }

      if (!types_equal(*a.as.func.retval, *b.as.func.retval)) {
        return false;
      }

      return vectype_equal(a.as.func.params, b.as.func.params);
    }

    default:
      return false;
  }
}

static void free_symbol(struct Symbol *sym)
{
  free_type(&sym->type);
  free(sym);
}

static void sym_insert(struct Symbol **sym, char *name, Type type, bool is_mut)
{
  struct Symbol *node;

  node = malloc(sizeof(struct Symbol));

  node->name = name;
  node->type = type;
  node->is_mut = is_mut;
  node->next = *sym;

  *sym = node;
}

static struct Symbol *sym_get(struct Symbol *sym, char *name)
{
  while (sym) {
    if (strcmp(sym->name, name) == 0) {
      return sym;
    }
    sym = sym->next;
  }
  return NULL;
}

#define IN_RANGE(val, min, max) ((val) >= (min) && (val) <= (max))

static bool is_bitwise_binop(enum ExprBinKind kind)
{
  return kind == EXPR_BIN_BITWISE_AND || kind == EXPR_BIN_BITWISE_XOR ||
         kind == EXPR_BIN_BITWISE_OR || kind == EXPR_BIN_SHIFT_LEFT ||
         kind == EXPR_BIN_SHIFT_RIGHT;
}

static bool is_shift_binop(enum ExprBinKind kind)
{
  return kind == EXPR_BIN_SHIFT_LEFT || kind == EXPR_BIN_SHIFT_RIGHT;
}

void get_type_size_and_align(Type *type, int *size, int *align)
{
  switch (type->kind) {
    case I8_T:
    case U8_T:
    case BOOL_T:
      *size = 1;
      *align = 1;
      break;
    case I16_T:
    case U16_T:
      *size = 2;
      *align = 2;
      break;
    case I32_T:
    case U32_T:
    case F32_T:
      *size = 4;
      *align = 4;
      break;
    case I64_T:
    case U64_T:
    case F64_T:
    case PTR_T:
    case STR_T:
      *size = 8;
      *align = 8;
      break;
    case STRUCT_T: {
      struct StructDef *def = struct_get(struct_table, type->as.struct_name);
      assert(def && "Tried to get size of incomplete/unknown struct");
      *size = def->size;
      *align = def->alignment;
      break;
    }
    default:
      *size = -1;
      *align = -1;
      break;
  }
}

static Type get_common_type(struct Expr *lhs, struct Expr *rhs)
{
  Type t1 = lhs->type;
  Type t2 = rhs->type;

  if (t1.kind == t2.kind) {
    return t1;
  }

  if (t1.kind == F64_T || t2.kind == F64_T) {
    return (Type){.kind = F64_T};
  }

  if (t1.kind == F32_T || t2.kind == F32_T) {
    return (Type){.kind = F32_T};
  }

  int size1 = get_type_size(t1);
  int size2 = get_type_size(t2);

  if (size1 == -1 || size2 == -1) {
    return (Type){.kind = UNKNOWN_T};
  }

  if (size1 > size2) {
    return t1;
  }
  if (size2 > size1) {
    return t2;
  }

  if (is_unsigned(t1.kind)) {
    return t1;
  }
  if (is_unsigned(t2.kind)) {
    return t2;
  }

  return (Type){.kind = UNKNOWN_T};
}

static bool promote_literal(struct Expr *expr, Type target_type)
{
  if (expr->kind == EXPR_UNARY && expr->as.unary.expr->kind == EXPR_LITERAL &&
      expr->as.unary.expr->as.literal.kind == LITERAL_NUM) {
    bool res = promote_literal(expr->as.unary.expr, target_type);
    if (res) {
      expr->type = target_type;
    }
    return res;
  }

  if (expr->kind != EXPR_LITERAL || expr->as.literal.kind != LITERAL_NUM) {
    return false;
  }

  if (expr->kind != EXPR_LITERAL || expr->as.literal.kind != LITERAL_NUM) {
    return false;
  }

  bool src_is_float = (expr->as.literal.type.kind == F32_T ||
                       expr->as.literal.type.kind == F64_T);
  bool tgt_is_float = (target_type.kind == F32_T || target_type.kind == F64_T);

  if (src_is_float && tgt_is_float) {
    double val = (expr->as.literal.type.kind == F32_T)
                     ? (double) expr->as.literal.as.f32
                     : expr->as.literal.as.f64;

    expr->type = target_type;
    expr->as.literal.type = target_type;

    if (target_type.kind == F32_T) {
      expr->as.literal.as.f32 = (float) val;
    } else {
      expr->as.literal.as.f64 = val;
    }
    return true;
  }

  if (src_is_float != tgt_is_float) {
    return false;
  }

  bool src_is_unsigned = is_unsigned(expr->as.literal.type.kind);
  bool tgt_is_unsigned = is_unsigned(target_type.kind);

  unsigned long long uval = 0;
  long long sval = 0;

  if (src_is_unsigned) {
    switch (expr->as.literal.type.kind) {
      case U8_T:
        uval = expr->as.literal.as.u8;
        break;
      case U16_T:
        uval = expr->as.literal.as.u16;
        break;
      case U32_T:
        uval = expr->as.literal.as.u32;
        break;
      case U64_T:
        uval = expr->as.literal.as.u64;
        break;
      default:
        break;
    }
  } else {
    switch (expr->as.literal.type.kind) {
      case I8_T:
        sval = expr->as.literal.as.i8;
        break;
      case I16_T:
        sval = expr->as.literal.as.i16;
        break;
      case I32_T:
        sval = expr->as.literal.as.i32;
        break;
      case I64_T:
        sval = expr->as.literal.as.i64;
        break;
      default:
        break;
    }
  }

  bool fits = false;

  if (src_is_unsigned && tgt_is_unsigned) {
    switch (target_type.kind) {
      case U8_T:
        fits = (uval <= UCHAR_MAX);
        break;
      case U16_T:
        fits = (uval <= USHRT_MAX);
        break;
      case U32_T:
        fits = (uval <= UINT_MAX);
        break;
      case U64_T:
        fits = (uval <= ULLONG_MAX);
        break;
      default:
        break;
    }
  } else if (!src_is_unsigned && !tgt_is_unsigned) {
    switch (target_type.kind) {
      case I8_T:
        fits = IN_RANGE(sval, SCHAR_MIN, SCHAR_MAX);
        break;
      case I16_T:
        fits = IN_RANGE(sval, SHRT_MIN, SHRT_MAX);
        break;
      case I32_T:
        fits = IN_RANGE(sval, INT_MIN, INT_MAX);
        break;
      case I64_T:
        fits = IN_RANGE(sval, LLONG_MIN, LLONG_MAX);
        break;
      default:
        break;
    }
  } else if (!src_is_unsigned && tgt_is_unsigned) {
    if (sval < 0) {
      return false;
    }

    unsigned long long casted = (unsigned long long) sval;
    switch (target_type.kind) {
      case U8_T:
        fits = (casted <= UCHAR_MAX);
        break;
      case U16_T:
        fits = (casted <= USHRT_MAX);
        break;
      case U32_T:
        fits = (casted <= UINT_MAX);
        break;
      case U64_T:
        fits = (casted <= ULLONG_MAX);
        break;
      default:
        break;
    }
  } else if (src_is_unsigned && !tgt_is_unsigned) {
    switch (target_type.kind) {
      case I8_T:
        fits = (uval <= SCHAR_MAX);
        break;
      case I16_T:
        fits = (uval <= SHRT_MAX);
        break;
      case I32_T:
        fits = (uval <= INT_MAX);
        break;
      case I64_T:
        fits = (uval <= LLONG_MAX);
        break;
      default:
        break;
    }
  }

  if (fits) {
    expr->type = target_type;
    expr->as.literal.type = target_type;

    unsigned long long final_uval =
        src_is_unsigned ? uval : (unsigned long long) sval;
    long long final_sval = src_is_unsigned ? (long long) uval : sval;

    switch (target_type.kind) {
      case I8_T:
        expr->as.literal.as.i8 = (char) final_sval;
        break;
      case U8_T:
        expr->as.literal.as.u8 = (unsigned char) final_uval;
        break;
      case I16_T:
        expr->as.literal.as.i16 = (short) final_sval;
        break;
      case U16_T:
        expr->as.literal.as.u16 = (unsigned short) final_uval;
        break;
      case I32_T:
        expr->as.literal.as.i32 = (int) final_sval;
        break;
      case U32_T:
        expr->as.literal.as.u32 = (unsigned int) final_uval;
        break;
      case I64_T:
        expr->as.literal.as.i64 = (long long) final_sval;
        break;
      case U64_T:
        expr->as.literal.as.u64 = final_uval;
        break;
      default:
        break;
    }
    return true;
  }

  return false;
}

static bool is_float_type(enum TypeKind k)
{
  return k == F32_T || k == F64_T;
}

int get_type_size(Type t)
{
  switch (t.kind) {
    case I8_T:
    case U8_T:
    case BOOL_T:
      return 1;
    case I16_T:
    case U16_T:
      return 2;
    case I32_T:
    case U32_T:
    case F32_T:
      return 4;
    case I64_T:
    case U64_T:
    case F64_T:
    case STR_T:
    case PTR_T:
      return 8;
    case STRUCT_T: {
      struct StructDef *def = struct_get(struct_table, t.as.struct_name);
      return def->size;
    }
    default:
      return -1;
  }
}

bool is_unsigned(enum TypeKind kind)
{
  switch (kind) {
    case U8_T:
    case U16_T:
    case U32_T:
    case U64_T:
      return true;
    default:
      return false;
  }
}

bool is_integer_type(enum TypeKind kind)
{
  switch (kind) {
    case I8_T:
    case I16_T:
    case I32_T:
    case I64_T:
    case U8_T:
    case U16_T:
    case U32_T:
    case U64_T:
      return true;
    default:
      return false;
  }
}

static bool is_scalar_type(Type t)
{
  return is_integer_type(t.kind) || t.kind == BOOL_T || is_float_type(t.kind) ||
         t.kind == PTR_T;
}

static bool can_explicit_cast(Type src, Type dst)
{
  if (types_equal(src, dst)) {
    return true;
  }

  if (is_scalar_type(src) && is_scalar_type(dst)) {
    return true;
  }

  return false;
}

static struct TypecheckResult coerce_expr_to_type(struct Expr *expr,
                                                  Type target_type,
                                                  char *err_msg)
{
  if (types_equal(expr->type, target_type)) {
    return (struct TypecheckResult){.is_ok = true, .msg = NULL, .ast = NULL};
  }

  bool is_literal =
      (expr->kind == EXPR_LITERAL && expr->as.literal.kind == LITERAL_NUM);
  bool is_unary_literal =
      (expr->kind == EXPR_UNARY && expr->as.unary.expr->kind == EXPR_LITERAL &&
       expr->as.unary.expr->as.literal.kind == LITERAL_NUM);

  if (is_literal || is_unary_literal) {
    if (!promote_literal(expr, target_type)) {
      return (struct TypecheckResult){
          .is_ok = false, .msg = err_msg, .ast = NULL};
    }
  }

  return (struct TypecheckResult){.is_ok = true, .msg = NULL, .ast = NULL};
}

static bool is_expr_mutable(struct Expr *expr, struct Symbol *sym_table)
{
  switch (expr->kind) {
    case EXPR_VARIABLE: {
      struct Symbol *sym = sym_get(sym_table, expr->as.var.name);
      return sym ? sym->is_mut : false;
    }
    case EXPR_MEMBER: {
      if (expr->as.member.is_arrow) {
        return true;
      }
      return is_expr_mutable(expr->as.member.target, sym_table);
    }
    case EXPR_DEREF: {
      return true;
    }
    default:
      return false;
  }
}

static struct TypecheckResult typecheck_expr(struct Expr *expr,
                                             struct Symbol *sym_table)
{
  struct TypecheckResult res = {.is_ok = true, .msg = NULL, .ast = NULL};

  switch (expr->kind) {
    case EXPR_SIZEOF: {
      struct TypecheckResult r;

      r = typecheck_expr(expr->as.sizeof_expr.expr, sym_table);
      if (!r.is_ok) {
        return r;
      }

      expr->type = expr->as.sizeof_expr.expr->type;

      /* FIXME: is complete? */
      break;
    }
    case EXPR_MEMBER: {
      struct TypecheckResult r;

      r = typecheck_expr(expr->as.member.target, sym_table);
      if (!r.is_ok) {
        return r;
      }

      Type target_type = expr->as.member.target->type;
      char *struct_name = NULL;

      /* 1. Validate the left-hand side */
      if (expr->as.member.is_arrow) {
        if (target_type.kind != PTR_T ||
            target_type.as.base->kind != STRUCT_T) {
          return (struct TypecheckResult){
              .is_ok = false,
              .msg = "Left of '->' must be a pointer to struct",
              .ast = NULL};
        }
        struct_name = target_type.as.base->as.struct_name;
      } else {
        if (target_type.kind != STRUCT_T) {
          return (struct TypecheckResult){.is_ok = false,
                                          .msg = "Left of '.' must be a struct",
                                          .ast = NULL};
        }
        struct_name = target_type.as.struct_name;
      }

      /* 2. Look up the struct definition */
      struct StructDef *def = struct_get(struct_table, struct_name);
      if (!def) {
        return (struct TypecheckResult){
            .is_ok = false,
            .msg = "Accessing member of incomplete/unknown struct",
            .ast = NULL};
      }

      /* 3. Find the field and assign the expression's type */
      bool found = false;
      for (int i = 0; i < def->fields.len; i++) {
        if (strcmp(def->fields.data[i].name, expr->as.member.field_name) == 0) {
          expr->type = clone_type(def->fields.data[i].type);
          found = true;
          break;
        }
      }

      if (!found) {
        return (struct TypecheckResult){
            .is_ok = false, .msg = "Struct has no such field", .ast = NULL};
      }

      break;
    }
    case EXPR_STRUCT_INIT: {
      struct StructDef *def =
          struct_get(struct_table, expr->as.struct_init.struct_name);
      if (!def) {
        return (struct TypecheckResult){
            .is_ok = false, .msg = "Initializing unknown struct", .ast = NULL};
      }

      int positional_idx = 0;
      for (int i = 0; i < expr->as.struct_init.values.len; i++) {
        struct StructInitItem *item = &expr->as.struct_init.values.data[i];

        struct TypecheckResult r = typecheck_expr(item->expr, sym_table);
        if (!r.is_ok) {
          return r;
        }

        Type expected_type;
        int resolved_offset = 0;

        if (item->designator) {
          char *path = strdup(item->designator);
          char *part = strtok(path, ".");
          struct StructDef *curr_def = def;
          Type current_type;

          while (part) {
            bool found = false;
            for (int f = 0; f < curr_def->fields.len; f++) {
              if (strcmp(curr_def->fields.data[f].name, part) == 0) {
                found = true;
                resolved_offset += curr_def->fields.data[f].offset;
                current_type = curr_def->fields.data[f].type;
                break;
              }
            }
            if (!found) {
              free(path);
              return (struct TypecheckResult){.is_ok = false,
                                              .msg = "Field not found"};
            }

            part = strtok(NULL, ".");
            if (part) {
              if (current_type.kind != STRUCT_T) {
                free(path);
                return (struct TypecheckResult){.is_ok = false,
                                                .msg = "Not a struct"};
              }
              curr_def = struct_get(struct_table, current_type.as.struct_name);
              if (!curr_def) {
                free(path);
                return (struct TypecheckResult){
                    .is_ok = false,
                    .msg = "Designator references unknown nested struct",
                    .ast = NULL};
              }
            }
          }
          free(path);
          expected_type = current_type;

        } else {
          /* Handle positional fallback */
          if (positional_idx >= def->fields.len) {
            return (struct TypecheckResult){
                .is_ok = false,
                .msg = "Too many positional values in struct initializer",
                .ast = NULL};
          }
          expected_type = def->fields.data[positional_idx].type;
          resolved_offset = def->fields.data[positional_idx].offset;
          positional_idx++;
        }

        Type actual_type = item->expr->type;
        if (!types_equal(actual_type, expected_type)) {
          bool is_literal = item->expr->kind == EXPR_LITERAL &&
                            item->expr->as.literal.kind == LITERAL_NUM;
          bool is_unary_literal =
              item->expr->kind == EXPR_UNARY &&
              item->expr->as.unary.expr->kind == EXPR_LITERAL &&
              item->expr->as.unary.expr->as.literal.kind == LITERAL_NUM;

          if (is_literal || is_unary_literal) {
            if (!promote_literal(item->expr, expected_type)) {
              return (struct TypecheckResult){
                  .is_ok = false,
                  .msg =
                      "Type error: struct initializer value does not fit in "
                      "the expected field type",
                  .ast = NULL};
            }
          } else {
            struct Expr *inner = malloc(sizeof(struct Expr));
            *inner = *item->expr;

            item->expr->kind = EXPR_CAST;
            item->expr->type = clone_type(expected_type);
            item->expr->as.cast.expr = inner;
          }
        }

        /* Save the final flat offset for the IR phase! */
        item->resolved_offset = resolved_offset;
      }

      expr->type.kind = STRUCT_T;
      expr->type.as.struct_name = strdup(def->name);
      break;
    }
    case EXPR_ADDROF: {
      struct TypecheckResult r;

      r = typecheck_expr(expr->as.addrof.expr, sym_table);
      if (!r.is_ok) {
        return r;
      }

      Type *base = malloc(sizeof(Type));
      *base = expr->as.addrof.expr->type;
      expr->type = (Type){.kind = PTR_T, .as.base = base};
      break;
    }
    case EXPR_DEREF: {
      struct TypecheckResult r;

      r = typecheck_expr(expr->as.deref.expr, sym_table);
      if (!r.is_ok) {
        return r;
      }

      if (expr->as.deref.expr->type.kind != PTR_T) {
        return (struct TypecheckResult){
            .is_ok = false, .msg = "Cannot dereference a non-pointer"};
      }
      if (expr->as.deref.expr->type.as.base->kind == VOID_T) {
        return (struct TypecheckResult){
            .is_ok = false, .msg = "Cannot dereference a void pointer"};
      }

      expr->type = *expr->as.deref.expr->type.as.base;
      break;
    }
    case EXPR_CAST: {
      struct TypecheckResult r = typecheck_expr(expr->as.cast.expr, sym_table);
      if (!r.is_ok) {
        return r;
      }

      if (!can_explicit_cast(expr->as.cast.expr->type,
                             expr->as.cast.target_type)) {
        return (struct TypecheckResult){
            .is_ok = false, .msg = "Invalid cast", .ast = NULL};
      }

      expr->type = clone_type(expr->as.cast.target_type);
      break;
    }
    case EXPR_LITERAL: {
      if (expr->as.literal.kind == LITERAL_STR) {
        expr->type = (Type){.kind = STR_T};
      }
      break;
    }
    case EXPR_UNARY: {
      struct TypecheckResult r;

      r = typecheck_expr(expr->as.unary.expr, sym_table);
      if (!r.is_ok) {
        return r;
      }

      expr->type = expr->as.unary.expr->type;

      if (*expr->as.unary.op == '-') {
        switch (expr->type.kind) {
          case U8_T:
            expr->type.kind = I8_T;
            break;
          case U16_T:
            expr->type.kind = I16_T;
            break;
          case U32_T:
            expr->type.kind = I32_T;
            break;
          case U64_T:
            expr->type.kind = I64_T;
            break;
          default:
            break;
        }
      } else if (*expr->as.unary.op == '!') {
        if (expr->type.kind != BOOL_T) {
          return (struct TypecheckResult){
              .is_ok = false,
              .msg =
                  "Type error: logical NOT (!) requires a boolean expression",
              .ast = NULL};
        }
      } else if (*expr->as.unary.op == '~') {
        if (!is_integer_type(expr->type.kind)) {
          return (struct TypecheckResult){
              .is_ok = false,
              .msg =
                  "Type error: bitwise NOT (~) requires an integer expression",
              .ast = NULL};
        }
      }

      break;
    }
    case EXPR_VARIABLE: {
      struct Symbol *sym = sym_get(sym_table, expr->as.var.name);

      if (!sym) {
        return (struct TypecheckResult){
            .is_ok = false,
            .msg = "Referenced an undefined variable or function",
            .ast = NULL};
      }

      if (sym->type.kind == UNKNOWN_T) {
        return (struct TypecheckResult){
            .is_ok = false, .msg = "Referenced an unknown type", .ast = NULL};
      }

      expr->type = clone_type(sym->type);

      break;
    }
    case EXPR_BINARY: {
      struct TypecheckResult lhs_res =
          typecheck_expr(expr->as.binary.lhs, sym_table);
      if (!lhs_res.is_ok) {
        return lhs_res;
      }

      struct TypecheckResult rhs_res =
          typecheck_expr(expr->as.binary.rhs, sym_table);
      if (!rhs_res.is_ok) {
        return rhs_res;
      }

      if (expr->as.binary.kind == EXPR_BIN_LOGICAL_AND ||
          expr->as.binary.kind == EXPR_BIN_LOGICAL_OR) {
        if (expr->as.binary.lhs->type.kind != BOOL_T ||
            expr->as.binary.rhs->type.kind != BOOL_T) {
          return (struct TypecheckResult){
              .is_ok = false,
              .msg = "Type error: logical operators require bool operands",
              .ast = NULL};
        }

        struct TypecheckResult lhs_coerce =
            coerce_expr_to_type(expr->as.binary.lhs, (Type){.kind = BOOL_T},
                                "Type error: logical lhs must be bool");
        if (!lhs_coerce.is_ok) {
          return lhs_coerce;
        }

        struct TypecheckResult rhs_coerce =
            coerce_expr_to_type(expr->as.binary.rhs, (Type){.kind = BOOL_T},
                                "Type error: logical rhs must be bool");
        if (!rhs_coerce.is_ok) {
          return rhs_coerce;
        }

        expr->type = (Type){.kind = BOOL_T};
        break;
      }

      if (expr->as.binary.kind == EXPR_BIN_LESS ||
          expr->as.binary.kind == EXPR_BIN_LESS_EQUAL ||
          expr->as.binary.kind == EXPR_BIN_GREATER ||
          expr->as.binary.kind == EXPR_BIN_GREATER_EQUAL ||
          expr->as.binary.kind == EXPR_BIN_EQUAL_EQUAL ||
          expr->as.binary.kind == EXPR_BIN_BANG_EQUAL) {
        Type common_type =
            get_common_type(expr->as.binary.lhs, expr->as.binary.rhs);
        if (common_type.kind == UNKNOWN_T) {
          return (struct TypecheckResult){
              .is_ok = false,
              .msg = "Unable to compute common type",
              .ast = NULL};
        }

        struct TypecheckResult lhs_coerce = coerce_expr_to_type(
            expr->as.binary.lhs, common_type,
            "Type error: comparison lhs does not fit in the common type");
        if (!lhs_coerce.is_ok) {
          return lhs_coerce;
        }

        struct TypecheckResult rhs_coerce = coerce_expr_to_type(
            expr->as.binary.rhs, common_type,
            "Type error: comparison rhs does not fit in the common type");
        if (!rhs_coerce.is_ok) {
          return rhs_coerce;
        }

        expr->type = (Type){.kind = BOOL_T};
        break;
      }
      if (is_bitwise_binop(expr->as.binary.kind)) {
        if (!is_integer_type(expr->as.binary.lhs->type.kind) ||
            !is_integer_type(expr->as.binary.rhs->type.kind)) {
          return (struct TypecheckResult){
              .is_ok = false,
              .msg = "Type error: bitwise operators require integer operands",
              .ast = NULL};
        }

        if (is_shift_binop(expr->as.binary.kind)) {
          expr->type = expr->as.binary.lhs->type;
        } else {
          Type common_type =
              get_common_type(expr->as.binary.lhs, expr->as.binary.rhs);
          if (common_type.kind == UNKNOWN_T) {
            return (struct TypecheckResult){
                .is_ok = false,
                .msg = "Unable to compute common type",
                .ast = NULL};
          }
          struct TypecheckResult lhs_coerce = coerce_expr_to_type(
              expr->as.binary.lhs, common_type,
              "Type error: bitwise lhs does not fit in the common type");
          if (!lhs_coerce.is_ok) {
            return lhs_coerce;
          }

          struct TypecheckResult rhs_coerce = coerce_expr_to_type(
              expr->as.binary.rhs, common_type,
              "Type error: bitwise rhs does not fit in the common type");
          if (!rhs_coerce.is_ok) {
            return rhs_coerce;
          }

          expr->type = common_type;
        }
        break;
      }

      Type common_type;

      common_type = get_common_type(expr->as.binary.lhs, expr->as.binary.rhs);
      if (common_type.kind == UNKNOWN_T) {
        return (struct TypecheckResult){.is_ok = false,
                                        .msg = "Unable to compute common type",
                                        .ast = NULL};
      }

      expr->type = common_type;

      break;
    }
    case EXPR_ASSIGN: {
      struct TypecheckResult lhs_res =
          typecheck_expr(expr->as.assign.lhs, sym_table);
      if (!lhs_res.is_ok) {
        return lhs_res;
      }

      struct TypecheckResult rhs_res =
          typecheck_expr(expr->as.assign.rhs, sym_table);
      if (!rhs_res.is_ok) {
        return rhs_res;
      }

      if (expr->as.assign.lhs->kind != EXPR_VARIABLE &&
          expr->as.assign.lhs->kind != EXPR_DEREF &&
          expr->as.assign.lhs->kind != EXPR_MEMBER) {
        return (struct TypecheckResult){
            .is_ok = false,
            .msg = "Invalid assignment target: left side must be a variable",
            .ast = NULL};
      }

      if (!is_expr_mutable(expr->as.assign.lhs, sym_table)) {
        return (struct TypecheckResult){
            .is_ok = false,
            .msg = "Type error: cannot assign to an immutable variable",
            .ast = NULL};
      }

      Type actual_type = expr->as.assign.rhs->type;
      Type expected_type = expr->as.assign.lhs->type;

      if (!types_equal(actual_type, expected_type)) {
        bool is_literal = (expr->as.assign.rhs->kind == EXPR_LITERAL &&
                           expr->as.assign.rhs->as.literal.kind == LITERAL_NUM);
        bool is_unary_literal =
            (expr->as.assign.rhs->kind == EXPR_UNARY &&
             expr->as.assign.rhs->as.unary.expr->kind == EXPR_LITERAL &&
             expr->as.assign.rhs->as.unary.expr->as.literal.kind ==
                 LITERAL_NUM);

        if (is_literal || is_unary_literal) {
          if (!promote_literal(expr->as.assign.rhs, expected_type)) {
            return (struct TypecheckResult){
                .is_ok = false,
                .msg =
                    "Type error: assignment does not fit in the expected type",
                .ast = NULL,
            };
          }
        } else {
          struct Expr *inner = malloc(sizeof(struct Expr));
          *inner = *expr->as.assign.rhs;

          expr->as.assign.rhs->kind = EXPR_CAST;
          expr->as.assign.rhs->type = expected_type;
          expr->as.assign.rhs->as.cast.expr = inner;
        }
      }

      expr->type = expr->as.assign.lhs->type;
      break;
    }
    case EXPR_COMPOUND_ASSIGN: {
      struct TypecheckResult lhs_res =
          typecheck_expr(expr->as.compound_assign.lhs, sym_table);
      if (!lhs_res.is_ok) {
        return lhs_res;
      }

      struct TypecheckResult rhs_res =
          typecheck_expr(expr->as.compound_assign.rhs, sym_table);
      if (!rhs_res.is_ok) {
        return rhs_res;
      }

      if (expr->as.compound_assign.lhs->kind != EXPR_VARIABLE &&
          expr->as.compound_assign.lhs->kind != EXPR_DEREF &&
          expr->as.compound_assign.lhs->kind != EXPR_MEMBER) {
        return (struct TypecheckResult){
            .is_ok = false,
            .msg =
                "Invalid compound assignment target: left side must be a "
                "variable",
            .ast = NULL};
      }

      if (!is_expr_mutable(expr->as.compound_assign.lhs, sym_table)) {
        return (struct TypecheckResult){
            .is_ok = false,
            .msg = "Type error: cannot assign to an immutable variable",
            .ast = NULL};
      }

      if (is_bitwise_binop(expr->as.compound_assign.kind) &&
          (!is_integer_type(expr->as.compound_assign.lhs->type.kind) ||
           !is_integer_type(expr->as.compound_assign.rhs->type.kind))) {
        return (struct TypecheckResult){
            .is_ok = false,
            .msg =
                "Type error: compound bitwise assignment requires integer "
                "operands",
            .ast = NULL};
      }

      Type actual_type = expr->as.compound_assign.rhs->type;
      Type expected_type = expr->as.compound_assign.lhs->type;

      if (!types_equal(actual_type, expected_type)) {
        bool is_literal =
            (expr->as.compound_assign.rhs->kind == EXPR_LITERAL &&
             expr->as.compound_assign.rhs->as.literal.kind == LITERAL_NUM);
        bool is_unary_literal =
            (expr->as.compound_assign.rhs->kind == EXPR_UNARY &&
             expr->as.compound_assign.rhs->as.unary.expr->kind ==
                 EXPR_LITERAL &&
             expr->as.compound_assign.rhs->as.unary.expr->as.literal.kind ==
                 LITERAL_NUM);

        if (is_literal || is_unary_literal) {
          if (!promote_literal(expr->as.compound_assign.rhs, expected_type)) {
            return (struct TypecheckResult){
                .is_ok = false,
                .msg =
                    "Type error: compound assignment does not fit in the "
                    "expected type",
                .ast = NULL,
            };
          }
        } else {
          struct Expr *inner = malloc(sizeof(struct Expr));
          *inner = *expr->as.compound_assign.rhs;

          expr->as.compound_assign.rhs->kind = EXPR_CAST;
          expr->as.compound_assign.rhs->type = expected_type;
          expr->as.compound_assign.rhs->as.cast.expr = inner;
        }
      }

      expr->type = expr->as.compound_assign.lhs->type;
      break;
    }
    case EXPR_CALL: {
      struct Symbol *callee_sym =
          sym_get(sym_table, expr->as.call.target->as.var.name);

      if (!callee_sym) {
        return (struct TypecheckResult){
            .is_ok = false, .msg = "Called an undefined function", .ast = NULL};
      }

      bool is_variadic;
      int expected_args, provided_args;

      is_variadic = callee_sym->type.as.func.is_variadic;
      expected_args = callee_sym->type.as.func.params.len;
      provided_args = expr->as.call.arguments.len;

      if (is_variadic ? (provided_args < expected_args)
                      : (provided_args != expected_args)) {
        return (struct TypecheckResult){
            .is_ok = false,
            .msg = "Called with a wrong number of args",
            .ast = NULL};
      }

      for (int i = 0; i < expr->as.call.arguments.len; i++) {
        struct TypecheckResult arg_res =
            typecheck_expr(&expr->as.call.arguments.data[i], sym_table);

        if (!arg_res.is_ok) {
          return arg_res;
        }

        Type actual_type = expr->as.call.arguments.data[i].type;

        if (i < expected_args) {
          Type expected_type = callee_sym->type.as.func.params.data[i];

          if (!types_equal(actual_type, expected_type)) {
            bool is_literal =
                expr->as.call.arguments.data[i].kind == EXPR_LITERAL &&
                expr->as.call.arguments.data[i].as.literal.kind == LITERAL_NUM;

            bool is_unary_literal =
                expr->as.call.arguments.data[i].kind == EXPR_UNARY &&
                expr->as.call.arguments.data[i].as.unary.expr->kind ==
                    EXPR_LITERAL &&
                expr->as.call.arguments.data[i]
                        .as.unary.expr->as.literal.kind == LITERAL_NUM;

            if (is_literal || is_unary_literal) {
              if (!promote_literal(&expr->as.call.arguments.data[i],
                                   expected_type)) {
                return (struct TypecheckResult){
                    .is_ok = false,
                    .msg =
                        "Type error: argument literal value does not fit in "
                        "the expected type",
                    .ast = NULL,
                };
              }
            } else {
              struct Expr *inner = malloc(sizeof(struct Expr));
              *inner = expr->as.call.arguments.data[i];

              struct Expr cast_expr;
              cast_expr.kind = EXPR_CAST;
              cast_expr.type = clone_type(expected_type);
              cast_expr.as.cast.expr = inner;

              expr->as.call.arguments.data[i] = cast_expr;
            }
          }
        } else {
          if (actual_type.kind == I8_T || actual_type.kind == I16_T ||
              actual_type.kind == BOOL_T) {
            struct Expr *inner = malloc(sizeof(struct Expr));
            *inner = expr->as.call.arguments.data[i];

            struct Expr cast_expr;
            cast_expr.kind = EXPR_CAST;
            cast_expr.type = (Type){.kind = I32_T};
            cast_expr.as.cast.expr = inner;

            expr->as.call.arguments.data[i] = cast_expr;
          } else if (actual_type.kind == U8_T || actual_type.kind == U16_T) {
            struct Expr *inner = malloc(sizeof(struct Expr));
            *inner = expr->as.call.arguments.data[i];

            struct Expr cast_expr;
            cast_expr.kind = EXPR_CAST;
            cast_expr.type = (Type){.kind = U32_T};
            cast_expr.as.cast.expr = inner;

            expr->as.call.arguments.data[i] = cast_expr;
          } else if (actual_type.kind == F32_T) {
            struct Expr *inner = malloc(sizeof(struct Expr));
            *inner = expr->as.call.arguments.data[i];

            struct Expr cast_expr;
            cast_expr.kind = EXPR_CAST;
            cast_expr.type = (Type){.kind = F64_T};
            cast_expr.as.cast.expr = inner;

            expr->as.call.arguments.data[i] = cast_expr;
          }
        }
      }

      expr->type = clone_type(*callee_sym->type.as.func.retval);

      break;
    }
    default:
      assert(0);
  }
  return res;
}

static struct TypecheckResult typecheck_initializer(struct Expr *init,
                                                     Type expected_type,
                                                     struct Symbol **sym_table,
                                                     char *error_msg)
{
  struct TypecheckResult res;

  res = typecheck_expr(init, *sym_table);
  if (!res.is_ok) {
    return res;
  }

  Type actual_type = init->type;
  if (!types_equal(actual_type, expected_type)) {
    bool is_literal =
        (init->kind == EXPR_LITERAL && init->as.literal.kind == LITERAL_NUM);
    bool is_unary_literal =
        (init->kind == EXPR_UNARY &&
         init->as.unary.expr->kind == EXPR_LITERAL &&
         init->as.unary.expr->as.literal.kind == LITERAL_NUM);

    if (is_literal || is_unary_literal) {
      if (!promote_literal(init, expected_type)) {
        return (struct TypecheckResult){
            .is_ok = false,
            .msg = error_msg,
            .ast = NULL,
        };
      }
    } else {
      struct Expr *inner = malloc(sizeof(struct Expr));
      *inner = *init;

      init->kind = EXPR_CAST;
      init->type = expected_type;
      init->as.cast.expr = inner;
    }
  }

  return (struct TypecheckResult){.is_ok = true, .msg = NULL, .ast = NULL};
}

static struct TypecheckResult typecheck_stmt(struct Stmt *stmt,
                                             struct Symbol **sym_table)
{
  struct TypecheckResult res = {.is_ok = true, .msg = NULL, .ast = NULL};

  switch (stmt->kind) {
    case STMT_LOOP: {
      struct TypecheckResult r;

      r = typecheck_stmt(stmt->as.loop.body, sym_table);
      if (!r.is_ok) {
        return r;
      }

      break;
    }
    case STMT_BLOCK: {
      for (int i = 0; i < stmt->as.block.stmts.len; i++) {
        res = typecheck_stmt(&stmt->as.block.stmts.data[i], sym_table);
        if (!res.is_ok) {
          return res;
        }
      }
      break;
    }
    case STMT_LET: {
      res = typecheck_initializer(
          stmt->as.let.init, stmt->as.let.type, sym_table,
          "Type error: let init does not fit in the expected type");
      if (!res.is_ok) {
        return res;
      }

      sym_insert(sym_table, stmt->as.let.name, clone_type(stmt->as.let.type),
                 stmt->as.let.is_mut);
      break;
    }

    case STMT_RET: {
      if (stmt->as.ret.val) {
        res = typecheck_expr(stmt->as.ret.val, *sym_table);
        if (!res.is_ok) {
          return res;
        }

        Type actual_type = stmt->as.ret.val->type;
        Type expected_type = stmt->as.ret.expected_retval;

        if (!types_equal(actual_type, expected_type)) {
          bool is_literal = (stmt->as.ret.val->kind == EXPR_LITERAL &&
                             stmt->as.ret.val->as.literal.kind == LITERAL_NUM);
          bool is_unary_literal =
              (stmt->as.ret.val->kind == EXPR_UNARY &&
               stmt->as.ret.val->as.unary.expr->kind == EXPR_LITERAL &&
               stmt->as.ret.val->as.unary.expr->as.literal.kind == LITERAL_NUM);

          if (is_literal || is_unary_literal) {
            if (!promote_literal(stmt->as.ret.val, expected_type)) {
              return (struct TypecheckResult){
                  .is_ok = false,
                  .msg =
                      "Type error: returned literal value does not fit in the "
                      "expected return type",
                  .ast = NULL,
              };
            }
          }
        }
      }
      break;
    }
    case STMT_IF: {
      struct TypecheckResult cond_res, then_res, else_res;

      cond_res = typecheck_expr(&stmt->as.if_stmt.cond, *sym_table);
      if (!cond_res.is_ok) {
        return cond_res;
      }

      then_res = typecheck_stmt(stmt->as.if_stmt.then_block, sym_table);
      if (!then_res.is_ok) {
        return then_res;
      }

      if (stmt->as.if_stmt.else_block) {
        else_res = typecheck_stmt(stmt->as.if_stmt.else_block, sym_table);
        if (!else_res.is_ok) {
          return else_res;
        }
      }

      break;
    }
    case STMT_EXPR: {
      struct TypecheckResult r;

      r = typecheck_expr(&stmt->as.expr_stmt.expr, *sym_table);
      if (!r.is_ok) {
        return r;
      }

      break;
    }
    case STMT_DO_WHILE: {
      struct TypecheckResult cond_res, body_res;

      cond_res = typecheck_expr(&stmt->as.do_while_stmt.cond, *sym_table);
      if (!cond_res.is_ok) {
        return cond_res;
      }

      body_res = typecheck_stmt(stmt->as.do_while_stmt.body, sym_table);
      if (!body_res.is_ok) {
        return body_res;
      }

      break;
    }
    case STMT_WHILE: {
      struct TypecheckResult cond_res, body_res;

      cond_res = typecheck_expr(&stmt->as.while_stmt.cond, *sym_table);
      if (!cond_res.is_ok) {
        return cond_res;
      }

      body_res = typecheck_stmt(stmt->as.while_stmt.body, sym_table);
      if (!body_res.is_ok) {
        return body_res;
      }

      break;
    }
    case STMT_LABELED: {
      struct TypecheckResult r;

      r = typecheck_stmt(stmt->as.labeled.stmt, sym_table);
      if (!r.is_ok) {
        return r;
      }

      break;
    }
    case STMT_BREAK:
    case STMT_CONTINUE:
    case STMT_GOTO:
      break;
    default:
      assert(0);
  }

  return res;
}

static Type make_fn_type(struct DeclFn *fn)
{
  Type t = {0};
  VecType param_types = {0};

  for (int i = 0; i < fn->params.len; i++) {
    vec_insert(&param_types, clone_type(fn->params.data[i].type));
  }

  t.kind = FN_T;
  t.as.func.retval = malloc(sizeof(Type));
  *t.as.func.retval = clone_type(fn->retval);
  t.as.func.params = param_types;
  t.as.func.is_variadic = fn->is_variadic;

  return t;
}

static struct TypecheckResult typecheck_record_decl(char *name,
                                                    VecStructField *fields,
                                                    bool is_union)
{
  struct StructDef def;
  def.name = name;
  def.fields = *fields; /* sharing the pointer */
  def.size = 0;
  def.alignment = 1;
  def.is_union = is_union;

  /* Calculate offsets with x86_64 ABI padding */
  for (int i = 0; i < def.fields.len; i++) {
    int field_size, field_align;
    get_type_size_and_align(&def.fields.data[i].type, &field_size,
                            &field_align);

    if (field_size == -1) {
      return (struct TypecheckResult){
          .is_ok = false, .msg = "Struct/Union field has invalid type"};
    }

    if (def.is_union) {
      /* Union: All fields start at offset 0 */
      fields->data[i].offset = 0;
      def.fields.data[i].offset = 0;

      /* Size and alignment are simply the maximum of all fields */
      if (field_size > def.size) {
        def.size = field_size;
      }
      if (field_align > def.alignment) {
        def.alignment = field_align;
      }
    } else {
      /* Pad the current size to match the field's required alignment */
      if (def.size % field_align != 0) {
        def.size += field_align - (def.size % field_align);
      }

      /* Write back the calculated offset to the AST */
      fields->data[i].offset = def.size;
      def.fields.data[i].offset = def.size; /* Update our table copy */

      def.size += field_size;

      /* Struct alignment is the largest alignment of its fields */
      if (field_align > def.alignment) {
        def.alignment = field_align;
      }
    }
  }

  /* Final padding so arrays of this struct align properly */
  if (def.size % def.alignment != 0) {
    def.size += def.alignment - (def.size % def.alignment);
  }

  struct_insert(&struct_table, def);
  return (struct TypecheckResult){.is_ok = true, .msg = NULL, .ast = NULL};
}

static struct TypecheckResult typecheck_fn_decl(struct DeclFn *fn,
                                                struct Symbol **sym_table)
{
  Type t;

  t = make_fn_type(fn);
  sym_insert(sym_table, fn->name, t, false);

  if (fn->is_extern) {
    return (struct TypecheckResult){.is_ok = true, .msg = NULL, .ast = NULL};
  }

  struct Symbol *fn_sym_table = sym_table ? *sym_table : NULL;
  struct Symbol *outer_sym = fn_sym_table;

  for (int i = 0; i < fn->params.len; i++) {
    sym_insert(&fn_sym_table, fn->params.data[i].name,
               clone_type(fn->params.data[i].type), fn->params.data[i].is_mut);
  }

  for (int i = 0; i < fn->body.len; i++) {
    struct TypecheckResult res = typecheck_stmt(&fn->body.data[i],
                                                &fn_sym_table);
    if (!res.is_ok) {
      return res;
    }
  }

  while (fn_sym_table && fn_sym_table != outer_sym) {
    struct Symbol *tmp = fn_sym_table;
    fn_sym_table = fn_sym_table->next;
    free_symbol(tmp);
  }

  return (struct TypecheckResult){.is_ok = true, .msg = NULL, .ast = NULL};
}

static struct TypecheckResult typecheck_variable_decl(
    struct DeclVariable *variable, struct Symbol **sym_table)
{
  if (variable->init) {
    struct TypecheckResult res = typecheck_initializer(
        variable->init, variable->type, sym_table,
        "Type error: variable init does not fit in the expected type");
    if (!res.is_ok) {
      return res;
    }
  }

  sym_insert(sym_table, variable->name, clone_type(variable->type),
             variable->is_mut);

  return (struct TypecheckResult){.is_ok = true, .msg = NULL, .ast = NULL};
}

static struct TypecheckResult typecheck_decl(struct Decl *decl,
                                             struct Symbol **sym_table)
{
  switch (decl->kind) {
    case DECL_STRUCT:
      return typecheck_record_decl(decl->as.struct_decl.name,
                                   &decl->as.struct_decl.fields, false);
    case DECL_UNION:
      return typecheck_record_decl(decl->as.union_decl.name,
                                   &decl->as.union_decl.fields, true);
    case DECL_FN:
      return typecheck_fn_decl(&decl->as.fn, sym_table);
    case DECL_VARIABLE:
      return typecheck_variable_decl(&decl->as.variable, sym_table);
    case DECL_ENUM:
      break;
    default:
      assert(0);
  }

  return (struct TypecheckResult){.is_ok = true, .msg = NULL, .ast = NULL};
}

struct TypecheckResult typecheck(struct AST *ast)
{
  struct Symbol *global_sym = NULL;

  for (int i = 0; i < ast->decls.len; i++) {
    struct TypecheckResult r;
    r = typecheck_decl(&ast->decls.data[i], &global_sym);
    if (!r.is_ok) {
      while (global_sym) {
        struct Symbol *tmp = global_sym;
        global_sym = global_sym->next;
        free_symbol(tmp);
      }
      return r;
    }
  }

  while (global_sym) {
    struct Symbol *tmp = global_sym;
    global_sym = global_sym->next;
    free_symbol(tmp);
  }

  return (struct TypecheckResult){.is_ok = true, .msg = NULL, .ast = ast};
}
