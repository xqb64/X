#ifndef X_TYPECHECKER_H
#define X_TYPECHECKER_H

#include "ir.h"
#include "parser.h"

struct TypecheckResult {
  bool is_ok;
  char *msg;
  struct AST *ast;
};

struct Symbol {
  struct Symbol *next;
  char *name;
  Type type;
  bool is_mut;
};

#define IN_RANGE(val, min, max) ((val) >= (min) && (val) <= (max))

struct TypecheckResult typecheck(struct AST *ast);
void get_type_size_and_align(Type *type, int *size, int *align);
void struct_insert(struct StructTable **table, struct StructDef def);
struct StructDef *struct_get(struct StructTable *table, char *name);
Type clone_type(Type t);
void print_type(Type *type, int spaces);
void free_type(Type *t);
void free_enum_types(void);
void free_enum_variants(void);
bool enum_variant_get(char *name, int *out_val);
void enum_variant_insert(char *name, int value);
void enum_type_insert(char *name);
bool is_integer_type(enum TypeKind kind);
bool is_unsigned(enum TypeKind kind);
int get_type_size(Type t);

#endif
