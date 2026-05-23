#ifndef MINI_COMPILER_TYPECHECKER_H
#define MINI_COMPILER_TYPECHECKER_H

#include "common.h"

/* typechecker */

void free_struct_table(struct StructTable *table);
void struct_insert(struct StructTable **table, struct StructDef def);
struct StructDef *struct_get(struct StructTable *table, char *name);
void free_enum_types(void);
void enum_type_insert(char *name);
bool is_enum_type(char *name);
void free_enum_variants(void);
void enum_variant_insert(char *name, int value);
bool enum_variant_get(char *name, int *out_val);
Type clone_type(Type t);
void free_type(Type *t);
bool vectype_equal(VecType a, VecType b);
void free_symbol(struct Symbol *sym);
void sym_insert(struct Symbol **sym, char *name, Type type, bool is_mut);
struct Symbol *sym_get(struct Symbol *sym, char *name);
void print_type(Type *type, int spaces);
#define IN_RANGE(val, min, max) ((val) >= (min) && (val) <= (max))

bool is_bitwise_binop(enum ExprBinKind kind);
bool is_shift_binop(enum ExprBinKind kind);
void get_type_size_and_align(Type *type, int *size, int *align);
Type get_common_type(struct Expr *lhs, struct Expr *rhs);
bool promote_literal(struct Expr *expr, Type target_type);
struct TypecheckResult coerce_expr_to_type(struct Expr *expr, Type target_type,
                                           char *err_msg);
bool is_expr_mutable(struct Expr *expr, struct Symbol *sym_table);
struct TypecheckResult typecheck_expr(struct Expr *expr,
                                      struct Symbol *sym_table);
struct TypecheckResult typecheck_stmt(struct Stmt *stmt,
                                      struct Symbol **sym_table);
struct TypecheckResult typecheck(struct AST *ast);

#endif /* MINI_COMPILER_TYPECHECKER_H */
