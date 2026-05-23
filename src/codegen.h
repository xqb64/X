#ifndef MINI_COMPILER_CODEGEN_H
#define MINI_COMPILER_CODEGEN_H

#include "common.h"

/* codegen */

bool asm_type_can_live_in_int_reg(struct AsmType type);
void print_asm_type(struct AsmType type);
struct AsmType type_to_asm_type(Type type);
int asm_type_stack_size(struct AsmType t);
int asm_type_stack_align(struct AsmType t);
int align_up_int(int n, int align);
const char *reg_to_str_64(enum AsmRegister reg);
void print_asm_operand(struct AsmOperand *op);
void print_condition_code(enum ConditionCode cc);
void print_asm_binary_op(enum AsmInstrBinaryKind kind);
void print_asm_instr(struct AsmInstr *instr, int spaces);
void free_asm_instr(struct AsmInstr *instr);
void print_asm_fn(struct AsmFunction *fn);
void free_asm_fn(struct AsmFunction *fn);
void print_asm(struct AsmProgram *prog);
void free_asm(struct AsmProgram *prog);
struct AsmOperand codegen_irvalue(struct IRValue *val);
bool is_comparison(enum IRInstrBinaryKind kind);
struct ABIClassification classify_type(Type *type);
bool is_sret(Type *retval);
bool is_shift_asm_binary(enum AsmInstrBinaryKind kind);
void codegen_instr(struct IRInstr *ir_instr, VecAsmInstr *instrs,
                   Type *fn_retval);
struct AsmFunction codegen_fn(struct IRFunction *ir_func);
struct AsmResult codegen(struct IRProgram *ir_prog);
struct AsmProgram *replace_pseudo(struct AsmProgram *asmcode);
struct AsmProgram *fixup(struct AsmProgram *prog);

#endif /* MINI_COMPILER_CODEGEN_H */
