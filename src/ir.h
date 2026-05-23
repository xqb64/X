#ifndef MINI_COMPILER_IR_H
#define MINI_COMPILER_IR_H

#include "common.h"

/* ir */

struct IRValue *clone_irval(struct IRValue *v);
void print_ir_val(struct IRValue *ir_val, int spaces);
void free_ir_val(struct IRValue *val);
void print_ir_binary_op(enum IRInstrBinaryKind kind);
enum IRInstrBinaryKind expr_bin_to_ir_bin(enum ExprBinKind kind, Type type);
void print_ir_instr(struct IRInstr *instr, int spaces);
void free_ir_instr(struct IRInstr *instr);
void print_ir_fn(struct IRFunction *func);
void free_ir_fn(struct IRFunction *func);
void print_ir(struct IRProgram *prog);
void free_ir_prog(struct IRProgram *prog);
struct IRValue *mkirvar(void);
struct IRValue *irfy_expr_and_convert(VecIRInstr *instrs, struct Expr *expr);
void free_global_constants(void);
int get_type_size(Type t);
bool is_unsigned(enum TypeKind kind);
bool is_integer_type(enum TypeKind kind);
enum IRCastKind get_cast_kind(Type src, Type dst);
int extract_label_number(const char *label);
void irfy_stmt(VecIRInstr *instrs, struct Stmt *stmt);
struct IRFunction *irfy_fn(struct Stmt *stmt);
struct IrfyResult irfy_ast(struct AST *ast);

#endif /* MINI_COMPILER_IR_H */
