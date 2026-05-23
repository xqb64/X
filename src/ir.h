#ifndef X_IR_H
#define X_IR_H

#include <stddef.h>

#include "parser.h"

enum IRValueKind {
  IRValue_CONST,
  IRValue_VAR,
};

struct IRValue {
  enum IRValueKind kind;
  Type type;
  union {
    char *var;
    struct Literal konst;
  } as;
};

enum IRInstrKind {
  IRInstr_BIN,
  IRInstr_UNARY,
  IRInstr_RET,
  IRInstr_CPY,
  IRInstr_CALL,
  IRInstr_JMP,
  IRInstr_JZ,
  IRInstr_LBL,
  IRInstr_GETADDR,
  IRInstr_LOAD,
  IRInstr_STORE,
  IRInstr_CAST,
  IRInstr_CPY_TO_OFFSET,
  IRInstr_CPY_FROM_OFFSET,
  IRInstr_ADD_PTR,
};

enum IRInstrBinaryKind {
  IRInstrBinary_ADD,
  IRInstrBinary_SUB,
  IRInstrBinary_MUL,
  IRInstrBinary_DIV,
  IRInstrBinary_E,
  IRInstrBinary_NE,
  IRInstrBinary_L,
  IRInstrBinary_LE,
  IRInstrBinary_G,
  IRInstrBinary_GE,
  IRInstrBinary_BIT_AND,
  IRInstrBinary_BIT_XOR,
  IRInstrBinary_BIT_OR,
  IRInstrBinary_SHL,
  IRInstrBinary_SHR,
  IRInstrBinary_SAR,
};

struct IRInstr_Binary {
  enum IRInstrBinaryKind kind;
  struct IRValue *lhs;
  struct IRValue *rhs;
  struct IRValue *dst;
};

enum IRInstrUnaryKind {
  IRInstrUnary_NEG,
  IRInstrUnary_NOT,
  IRInstrUnary_BIT_NOT,
};

struct IRInstr_Unary {
  enum IRInstrUnaryKind kind;
  struct IRValue *src;
  struct IRValue *dst;
};

struct IRInstr_Ret {
  struct IRValue *val;
};

struct IRInstr_Copy {
  struct IRValue *src;
  struct IRValue *dst;
};

typedef Vector(struct IRValue *) VecIRValuePtr;

struct IRInstr_Call {
  struct Expr target;
  VecIRValuePtr args;
  struct IRValue *dst;
};

struct IRInstr_Jump {
  char *target;
};

struct IRInstr_JumpIfZero {
  struct IRValue cond;
  char *target;
};

struct IRInstr_Label {
  char *name;
};

struct IRInstr_GetAddress {
  struct IRValue *src;
  struct IRValue *dst;
};

struct IRInstr_Load {
  struct IRValue *src;
  struct IRValue *dst;
};

struct IRInstr_Store {
  struct IRValue *val;
  struct IRValue *dst;
};

enum IRCastKind {
  IRCast_None,

  IRCast_Truncate,
  IRCast_SignExtend,
  IRCast_ZeroExtend,

  IRCast_FloatPromote,
  IRCast_FloatDemote,

  IRCast_IntToFloat,
  IRCast_IntToDouble,
  IRCast_FloatToInt,
  IRCast_DoubleToInt,

  IRCast_UIntToFloat,
  IRCast_UIntToDouble,
  IRCast_FloatToUInt,
  IRCast_DoubleToUInt,

  IRCast_PtrToInt,
  IRCast_IntToPtr,
  IRCast_Bitcast,
};

struct IRInstr_Cast {
  enum IRCastKind kind;
  struct IRValue *src;
  struct IRValue *dst;
};

struct IRInstr_CopyFromOffset {
  struct IRValue *src;
  int offset;
  struct IRValue *dst;
};

struct IRInstr_CopyToOffset {
  struct IRValue *dst;
  int offset;
  struct IRValue *src;
};

struct IRInstr_AddPtr {
  struct IRValue *ptr;
  struct IRValue *index;
  int scale;
  struct IRValue *dst;
};

struct IRInstr {
  enum IRInstrKind kind;
  union {
    struct IRInstr_Binary binary;
    struct IRInstr_Unary unary;
    struct IRInstr_Ret ret;
    struct IRInstr_Copy copy;
    struct IRInstr_Call call;
    struct IRInstr_Jump jmp;
    struct IRInstr_JumpIfZero jz;
    struct IRInstr_Label label;
    struct IRInstr_GetAddress getaddr;
    struct IRInstr_Load load;
    struct IRInstr_Store store;
    struct IRInstr_Cast cast;
    struct IRInstr_CopyFromOffset cpy_from_offset;
    struct IRInstr_CopyToOffset cpy_to_offset;
    struct IRInstr_AddPtr add_ptr;
  } as;
};

typedef Vector(struct IRInstr) VecIRInstr;

struct IRFunction {
  char *name;
  VecParam params;
  Type retval;
  VecIRInstr body;
};

typedef Vector(struct IRFunction *) VecIRFunctionPtr;

struct IRProgram {
  VecIRFunctionPtr funcs;
};

/*
 * ExpResult describes what an expression refers to during IR lowering.
 *
 * Some expressions already produce a normal IR value:
 *   x, 42, foo()
 * These use EXPRESULT_PLAIN.
 *
 * Some expressions describe a memory location instead of immediately loading
 * it: *ptr        -> EXPRESULT_DEREF point.x     -> EXPRESULT_SUBOBJECT
 *
 * Keeping these as locations is important because the caller may want to:
 *   - read from them,
 *   - assign to them,
 *   - or take their address.
 *
 * For example:
 *   point.x        can become a load from (point + field_offset)
 *   point.x = 10   can become a store to (point + field_offset)
 *   &point.x       can become the address (point + field_offset)
 *
 * SUBOBJECT stores:
 *   - the base aggregate object,
 *   - the byte offset of the field within that object,
 *   - and the base object's type, so later lowering still knows its layout.
 */
enum ExpResultKind {
  EXPRESULT_PLAIN,
  EXPRESULT_DEREF,
  EXPRESULT_SUBOBJECT,
};

struct ExpResult {
  enum ExpResultKind kind;
  union {
    struct IRValue *plain;
    struct IRValue *ptr;
    struct {
      char *base;
      size_t offset;
      Type base_type;
    } subobject;
  } as;
};

struct ExpResult irfy_expr(VecIRInstr *instrs, struct Expr *expr);

struct StaticConstant {
  char *name;
  char *value;
};

typedef Vector(struct StaticConstant) VecStaticConstant;

extern VecStaticConstant global_constants;

struct IrfyResult {
  bool is_ok;
  char *msg;
  struct IRProgram prog;
};

struct IrfyResult irfy_ast(struct AST *ast);
bool is_unsigned(enum TypeKind kind);
void free_ir_val(struct IRValue *val);
struct IRValue *clone_irval(struct IRValue *v);
void free_ir_instr(struct IRInstr *instr);
void print_ir(struct IRProgram *prog);
void free_ir_prog(struct IRProgram *prog);
void free_global_constants(void);
int get_type_size(Type t);
bool is_integer_type(enum TypeKind kind);

#endif
