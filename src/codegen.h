#ifndef X_CODEGEN_H
#define X_CODEGEN_H

#include <stdbool.h>

#include "ir.h"
#include "vector.h"

enum AsmOperandKind {
  AsmOperand_IMM,
  AsmOperand_PSEUDO,
  AsmOperand_REG,
  AsmOperand_STACK,
  AsmOperand_DATA,
  AsmOperand_MEMORY,
  AsmOperand_INDEXED,
};

enum AsmRegister {
  AX,
  BX,
  CX,
  DX,
  SI,
  DI,
  R8,
  R9,
  R10,
  R11,
  R12,
  R13,
  R14,
  R15,
  BP,
  SP,
  XMM0,
  XMM1,
  XMM2,
  XMM3,
  XMM4,
  XMM5,
  XMM6,
  XMM7,
  XMM8,
  XMM9,
  XMM10,
  XMM11,
  XMM12,
  XMM13,
  XMM14,
  XMM15,
};

enum AsmTypeKind {
  AsmType_BYTE,
  AsmType_WORD,
  AsmType_LONGWORD,
  AsmType_QUADWORD,
  AsmType_FLOAT,
  AsmType_DOUBLE,
  AsmType_BYTE_ARRAY,
};

struct AsmType {
  enum AsmTypeKind kind;
  union {
    struct {
      int size;
      int alignment;
    } bytearray;
  } as;
};

struct AsmOperand {
  enum AsmOperandKind kind;
  struct AsmType asm_type;
  union {
    long long imm;
    char *pseudo;
    enum AsmRegister reg;
    int stack_offset;
    char *data;
    struct {
      enum AsmRegister base;
      int offset;
    } mem;
    struct {
      enum AsmRegister base;
      enum AsmRegister index;
      int scale;
    } indexed;
  } as;
};

enum AsmInstrKind {
  AsmInstr_PUSH,
  AsmInstr_POP,
  AsmInstr_MOV,
  AsmInstr_BIN,
  AsmInstr_RET,
  AsmInstr_CALL,
  AsmInstr_JMP,
  AsmInstr_LBL,
  AsmInstr_CMP,
  AsmInstr_JmpCC,
  AsmInstr_SetCC,
  AsmInstr_LEA,
  AsmInstr_UNARY,
  AsmInstr_CVT,
  AsmInstr_REP_MOVSB,
  AsmInstr_SIGN_EXTEND_AX,
  AsmInstr_DIV,
};

struct AsmInstrMov {
  struct AsmOperand src;
  struct AsmOperand dst;
};

enum AsmInstrBinaryKind {
  AsmInstrBinary_ADD,
  AsmInstrBinary_SUB,
  AsmInstrBinary_MUL,
  AsmInstrBinary_DIV,
  AsmInstrBinary_LESS,
  AsmInstrBinary_LESS_EQUAL,
  AsmInstrBinary_GREATER,
  AsmInstrBinary_GREATER_EQUAL,
  AsmInstrBinary_EQUAL_EQUAL,
  AsmInstrBinary_BANG_EQUAL,
  AsmInstrBinary_BIT_AND,
  AsmInstrBinary_BIT_XOR,
  AsmInstrBinary_BIT_OR,
  AsmInstrBinary_SHL,
  AsmInstrBinary_SHR,
  AsmInstrBinary_SAR,
};

struct AsmInstrBinary {
  enum AsmInstrBinaryKind kind;
  struct AsmOperand lhs;
  struct AsmOperand rhs;
};

struct AsmInstrRet {
  short __dummy;
};

struct AsmInstrPush {
  struct AsmOperand op;
};

struct AsmInstrPop {
  struct AsmOperand op;
};

struct AsmInstrCall {
  char *target;
};

struct AsmInstrJmp {
  char *target;
};

enum ConditionCode {
  /* signed */
  CC_E,  /* equal */
  CC_NE, /* not equal */
  CC_L,  /* less, */
  CC_LE, /* less or equal */
  CC_G,  /* greater */
  CC_GE, /* greater or equal */

  /* unsigned */
  CC_A,  /* above */
  CC_AE, /* above or equal */
  CC_B,  /* below */
  CC_BE, /* below or equal */
};

struct AsmInstrJmpCC {
  enum ConditionCode cc;
  char *target;
};

struct AsmInstrSetCC {
  enum ConditionCode cc;
  struct AsmOperand op;
};

struct AsmInstrCmp {
  struct AsmType asm_type;
  struct AsmOperand lhs;
  struct AsmOperand rhs;
};

struct AsmInstrLabel {
  char *name;
};

struct AsmInstrLea {
  struct AsmOperand src;
  struct AsmOperand dst;
};

enum AsmInstrUnaryKind {
  AsmInstrUnary_NEG,
  AsmInstrUnary_BIT_NOT,
};

struct AsmInstrUnary {
  enum AsmInstrUnaryKind kind;
  struct AsmOperand op;
};

enum AsmCastKind {
  AsmCast_SignExtend,
  AsmCast_ZeroExtend,
  AsmCast_FloatPromote,
  AsmCast_FloatDemote,
  AsmCast_IntToFloat,
  AsmCast_IntToDouble,
  AsmCast_FloatToInt,
  AsmCast_DoubleToInt,
};

struct AsmInstrCvt {
  enum AsmCastKind kind;
  struct AsmOperand src;
  struct AsmOperand dst;
};

struct AsmInstrSignExtendAX {
  short __dummy;
};

struct AsmInstrDiv {
  bool is_signed;
  struct AsmOperand divisor;
};

struct AsmInstr {
  enum AsmInstrKind kind;
  struct AsmType asm_type;
  union {
    struct AsmInstrMov mov;
    struct AsmInstrBinary binary;
    struct AsmInstrRet ret;
    struct AsmInstrPush push;
    struct AsmInstrPop pop;
    struct AsmInstrCall call;
    struct AsmInstrJmp jmp;
    struct AsmInstrJmpCC jmpcc;
    struct AsmInstrSetCC setcc;
    struct AsmInstrCmp cmp;
    struct AsmInstrLabel lbl;
    struct AsmInstrLea lea;
    struct AsmInstrUnary unary;
    struct AsmInstrCvt cvt;
    struct AsmInstrSignExtendAX sign_extend_ax;
    struct AsmInstrDiv div;
  } as;
};

typedef Vector(struct AsmInstr) VecAsmInstr;

struct AsmFunction {
  char *name;
  VecAsmInstr body;
};

typedef Vector(struct AsmFunction) VecAsmFunction;

struct AsmProgram {
  VecAsmFunction funcs;
};

enum ABIClass { ABI_NO_CLASS, ABI_INTEGER, ABI_SSE, ABI_MEMORY };

struct ABIClassification {
  bool is_memory;
  enum ABIClass eightbytes[2];
};

struct AsmResult {
  bool is_ok;
  char *msg;
  struct AsmProgram prog;
};

struct Map {
  struct Map *next;
  char *name;
  int offset;
  struct AsmType asm_type;
};

struct AsmResult codegen(struct IRProgram *ir_prog);
const char *reg_to_str_64(enum AsmRegister reg);
void print_asm(struct AsmProgram *prog);
void free_asm(struct AsmProgram *prog);
int asm_type_stack_size(struct AsmType t);
int asm_type_stack_align(struct AsmType t);
int align_up_int(int n, int align);
void print_asm_operand(struct AsmOperand *op);

#endif
