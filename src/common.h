#ifndef MINI_COMPILER_COMMON_H
#define MINI_COMPILER_COMMON_H

/* Shared macros, standard includes, and cross-component data types. */

#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define ALLOC(obj) (memcpy(malloc(sizeof((obj))), &(obj), sizeof(obj)))

#define Vector(T) \
  struct {        \
    int capacity; \
    int len;      \
    T *data;      \
  }

#define vec_insert(vec, item)                                             \
  /* Capacity grows exponentially (doubling) when the vector is full.     \
   * This geometric growth strategy ensures an amortized O(1) time        \
   * complexity for insertions, avoiding the O(n) bottleneck that         \
   * would occur with linear reallocation. */                             \
  do {                                                                    \
    if ((vec)->len >= (vec)->capacity) {                                  \
      (vec)->capacity = (vec)->capacity == 0 ? 2 : (vec)->capacity * 2;   \
      (vec)->data =                                                       \
          realloc((vec)->data, sizeof((vec)->data[0]) * (vec)->capacity); \
    }                                                                     \
    (vec)->data[(vec)->len++] = (item);                                   \
  } while (0)

#define vec_free(vec) free((vec)->data);

/* Shared declarations. */



struct ReadFileResult {
  bool is_ok;
  char *msg;
  char *contents;
};


enum TokenKind {
  /* keywords */
  TOKEN_FN,
  TOKEN_LET,
  TOKEN_MUT,
  TOKEN_AS,
  TOKEN_IF,
  TOKEN_ELSE,
  TOKEN_WHILE,
  TOKEN_LOOP,
  TOKEN_BREAK,
  TOKEN_CONTINUE,
  TOKEN_GOTO,
  TOKEN_RET,
  TOKEN_EXTERN,
  TOKEN_VOID,
  TOKEN_SIZEOF,
  TOKEN_STRUCT,
  TOKEN_UNION,
  TOKEN_ENUM,
  TOKEN_BOOL,
  TOKEN_TRUE,
  TOKEN_FALSE,
  TOKEN_I8,
  TOKEN_I16,
  TOKEN_I32,
  TOKEN_I64,
  TOKEN_U8,
  TOKEN_U16,
  TOKEN_U32,
  TOKEN_U64,
  TOKEN_F32,
  TOKEN_F64,
  TOKEN_STR,

  /* one char */
  TOKEN_LPAREN,
  TOKEN_RPAREN,
  TOKEN_LBRACE,
  TOKEN_RBRACE,
  TOKEN_PLUS,
  TOKEN_MINUS,
  TOKEN_STAR,
  TOKEN_SLASH,
  TOKEN_COMMA,
  TOKEN_COLON,
  TOKEN_SEMICOLON,
  TOKEN_LESS,
  TOKEN_GREATER,
  TOKEN_EQUAL,
  TOKEN_BANG,
  TOKEN_PIPE,
  TOKEN_AMPERSAND,
  TOKEN_CARET,
  TOKEN_TILDE,
  TOKEN_DOT,

  /* two chars */
  TOKEN_LESS_EQUAL,
  TOKEN_GREATER_EQUAL,
  TOKEN_EQUAL_EQUAL,
  TOKEN_BANG_EQUAL,
  TOKEN_PIPE_PIPE,
  TOKEN_AMPERSAND_AMPERSAND,
  TOKEN_ARROW,
  TOKEN_PLUS_EQUAL,
  TOKEN_MINUS_EQUAL,
  TOKEN_STAR_EQUAL,
  TOKEN_SLASH_EQUAL,
  TOKEN_AMPERSAND_EQUAL,
  TOKEN_PIPE_EQUAL,
  TOKEN_CARET_EQUAL,
  TOKEN_LESS_LESS,
  TOKEN_GREATER_GREATER,

  /* three chars */
  TOKEN_LESS_LESS_EQUAL,
  TOKEN_GREATER_GREATER_EQUAL,
  TOKEN_ELLIPSIS,

  /* variable length */
  TOKEN_IDENTIFIER,
  TOKEN_NUMBER,
  TOKEN_FP_NUMBER,
  TOKEN_STRING,

  /* control */
  TOKEN_ERROR,
};


struct Token {
  enum TokenKind kind;
  char *start;
  int len;
};


typedef Vector(struct Token) VecToken;


struct Tokenizer {
  char *src;
};


struct TokenizeResult {
  bool is_ok;
  char *msg;
  VecToken tokens;
};


enum LiteralKind {
  LITERAL_NUM,
  LITERAL_BOOL,
  LITERAL_STR,
};


enum TypeKind {
  I8_T,
  I16_T,
  I32_T,
  I64_T,
  U8_T,
  U16_T,
  U32_T,
  U64_T,
  F32_T,
  F64_T,
  BOOL_T,
  STR_T,
  FN_T,
  VOID_T,
  PTR_T,
  STRUCT_T,
  UNKNOWN_T,
};


typedef struct Type Type;


typedef Vector(Type) VecType;


struct Type {
  enum TypeKind kind;
  union {
    struct {
      VecType params;
      Type *retval;
      bool is_variadic;
    } func;
    struct Type *base;
    char *struct_name;
  } as;
};


struct StructField {
  char *name;
  Type type;
  int offset;
};


typedef Vector(struct StructField) VecStructField;


struct StructDef {
  char *name;
  VecStructField fields;
  int size;
  int alignment;
  bool is_union;
};


struct StructTable {
  struct StructTable *next;
  struct StructDef def;
};
extern struct StructTable *struct_table;



struct EnumTypeItem {
  char *name;
  struct EnumTypeItem *next;
};
extern struct EnumTypeItem *enum_types;

struct EnumVariantItem {
  char *name;
  int value;
  struct EnumVariantItem *next;
};
extern struct EnumVariantItem *enum_variants;

struct Literal {
  enum LiteralKind kind;
  union {
    char *str;
    unsigned char u8;
    char i8;
    unsigned short u16;
    short i16;
    unsigned int u32;
    int i32;
    unsigned long long u64;
    long long i64;
    float f32;
    double f64;
    bool boolean;
  } as;
  Type type;
};


enum ExprKind {
  EXPR_LITERAL,
  EXPR_VARIABLE,
  EXPR_UNARY,
  EXPR_BINARY,
  EXPR_ASSIGN,
  EXPR_COMPOUND_ASSIGN,
  EXPR_CALL,
  EXPR_SIZEOF,
  EXPR_ADDROF,
  EXPR_DEREF,
  EXPR_CAST,
  EXPR_STRUCT_INIT,
  EXPR_MEMBER,
};


enum ExprBinKind {
  EXPR_BIN_ADD,
  EXPR_BIN_SUB,
  EXPR_BIN_MUL,
  EXPR_BIN_DIV,
  EXPR_BIN_LESS,
  EXPR_BIN_GREATER,
  EXPR_BIN_LESS_EQUAL,
  EXPR_BIN_GREATER_EQUAL,
  EXPR_BIN_EQUAL_EQUAL,
  EXPR_BIN_BANG_EQUAL,
  EXPR_BIN_BITWISE_AND,
  EXPR_BIN_BITWISE_XOR,
  EXPR_BIN_BITWISE_OR,
  EXPR_BIN_SHIFT_LEFT,
  EXPR_BIN_SHIFT_RIGHT,
  EXPR_BIN_LOGICAL_AND,
  EXPR_BIN_LOGICAL_OR,
};


struct ExprVar {
  char *name;
  Type type;
};


struct ExprUnary {
  char *op;
  struct Expr *expr;
};


struct ExprBin {
  enum ExprBinKind kind;
  struct Expr *lhs;
  struct Expr *rhs;
};


struct ExprAssign {
  struct Expr *lhs;
  struct Expr *rhs;
};


struct ExprCompoundAssign {
  enum ExprBinKind kind;
  struct Expr *lhs;
  struct Expr *rhs;
};


typedef Vector(struct Expr) VecExpr;


struct ExprCall {
  struct Expr *target;
  VecExpr arguments;
};


struct ExprDeref {
  struct Expr *expr;
};


struct ExprAddrOf {
  struct Expr *expr;
};


struct ExprSizeof {
  struct Expr *expr;
};


struct ExprCast {
  struct Expr *expr;
  Type target_type;
};


struct StructInitItem {
  char *designator;
  struct Expr *expr;
  int resolved_offset;
};

typedef Vector(struct StructInitItem) VecStructInitItem;


struct ExprStructInit {
  char *struct_name;
  VecStructInitItem values;
};


struct ExprMember {
  struct Expr *target;
  char *field_name;
  bool is_arrow;
};


struct ExprAs {
  struct Expr *target;
  Type target_type;
};


struct Expr {
  enum ExprKind kind;
  union {
    struct Literal literal;
    struct ExprBin binary;
    struct ExprVar var;
    struct ExprCall call;
    struct ExprAssign assign;
    struct ExprCompoundAssign compound_assign;
    struct ExprUnary unary;
    struct ExprAddrOf addrof;
    struct ExprSizeof sizeof_expr;
    struct ExprDeref deref;
    struct ExprCast cast;
    struct ExprStructInit struct_init;
    struct ExprMember member;
    struct ExprAs as;
  } as;
  Type type;
};


bool types_equal(Type a, Type b);


struct Parameter {
  char *name;
  Type type;
  bool is_mut;
};


typedef Vector(struct Stmt) VecStmt;

typedef Vector(struct Parameter) VecParam;


enum StmtKind {
  STMT_FN,
  STMT_LET,
  STMT_RET,
  STMT_IF,
  STMT_WHILE,
  STMT_LOOP,
  STMT_BREAK,
  STMT_CONTINUE,
  STMT_GOTO,
  STMT_LABELED,
  STMT_BLOCK,
  STMT_EXTERN,
  STMT_EXPR,
  STMT_STRUCT,
  STMT_ENUM,
};


struct StmtFn {
  char *name;
  VecParam params;
  Type retval;
  VecStmt body;
};


struct StmtLet {
  char *name;
  Type type;
  struct Expr *init;
  bool is_mut;
};


struct StmtRet {
  struct Expr *val;
  Type expected_retval;
};


struct StmtIf {
  struct Expr cond;
  struct Stmt *then_block;
  struct Stmt *else_block;
};


struct StmtWhile {
  struct Expr cond;
  struct Stmt *body;
  char *label;
};


struct StmtLoop {
  struct Stmt *body;
  char *label;
};


struct StmtBreak {
  char *label;
};


struct StmtContinue {
  char *label;
};


struct StmtGoto {
  char *label;
};


struct StmtLabeled {
  char *label;
  struct Stmt *stmt;
};


struct StmtBlock {
  VecStmt stmts;
};


struct StmtExtern {
  char *name;
  VecParam params;
  Type retval;
  bool is_variadic;
};


struct StmtExpr {
  struct Expr expr;
};


struct StmtStruct {
  char *name;
  VecStructField fields;
  bool is_union;
};


struct EnumVariant {
  char *name;
  int value;
};


typedef Vector(struct EnumVariant) VecEnumVariant;


struct StmtEnum {
  char *name;
  VecEnumVariant variants;
};


struct Stmt {
  enum StmtKind kind;
  union {
    struct StmtFn fn;
    struct StmtLet let;
    struct StmtRet ret;
    struct StmtIf if_stmt;
    struct StmtWhile while_stmt;
    struct StmtLoop loop;
    struct StmtBreak break_stmt;
    struct StmtContinue continue_stmt;
    struct StmtGoto goto_stmt;
    struct StmtLabeled labeled;
    struct StmtBlock block;
    struct StmtExtern extern_stmt;
    struct StmtExpr expr_stmt;
    struct StmtStruct struct_stmt;
    struct StmtEnum enum_stmt;
  } as;
};


struct AST {
  VecStmt stmts;
};


struct Parser {
  struct StmtFn *current_fn;
  struct Token *curr;
  struct Token *prev;
  VecToken *tokens;
  int idx;
  VecStmt *global_stmts;
};


struct Token *advance_parser(struct Parser *parser);


struct ParseFnResult {
  bool is_ok;
  char *msg;
  union {
    struct Expr expr;
    struct Stmt stmt;
  } as;
};


struct ParseResult {
  bool is_ok;
  char *msg;
  struct AST *ast;
};


struct ParseFnResult parse_stmt(struct Parser *parser);

struct ParseFnResult parse_expr(struct Parser *parser);


struct LoopLabelResult {
  bool is_ok;
  char *msg;
  struct AST *ast;
};


typedef Vector(char *) VecCharPtr;


struct CollectLabelsResult {
  bool is_ok;
  char *msg;
  VecCharPtr labels;
  ;
};


struct LabelCheckResult {
  bool is_ok;
  char *msg;
};


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


struct ConstBinding {
  char *name;
  struct IRValue *konst;
};


typedef Vector(struct ConstBinding) VecConstBinding;


struct CopyBinding {
  char *dst;
  char *src;
};


typedef Vector(struct CopyBinding) VecCopyBinding;


typedef Vector(char *) VecIRNameSet;

typedef Vector(int) VecInt;


struct IRInstrLiveness {
  VecIRNameSet use;
  VecIRNameSet def;
  VecIRNameSet live_before;
  VecIRNameSet live_after;
};


struct IRBasicBlock {
  int start;
  int end;

  VecInt succs;

  VecIRNameSet use;
  VecIRNameSet def;

  VecIRNameSet live_in;
  VecIRNameSet live_out;
};


struct IRCFG {
  struct IRBasicBlock *blocks;
  int block_count;
  int *instr_to_block;
};


#define MAX_OPTIMIZATION_PASSES 100

struct ResolveResult {
  bool is_ok;
  char *msg;
  union {
    struct AST *ast;
    struct Stmt *stmt;
    struct Expr *expr;
    struct Parameter *param;
  } as;
};


struct Variable {
  char *uniq_name;
};


struct VariableMap {
  struct VariableMap *next;
  char *name;
  struct Variable value;
};


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


struct AllocatedReg {
  struct AllocatedReg *next;
  char *pseudo;
  enum AsmRegister reg;
};


static enum AsmRegister allocatable_int_regs[] = {
    AX, BX, DX, CX, SI, DI, R8, R9, R12, R13, R14, R15,
};


#define NUM_ALLOCATABLE_INT_REGS \
  ((int) (sizeof(allocatable_int_regs) / sizeof(allocatable_int_regs[0])))

struct RegAllocState {
  struct AllocatedReg *allocated_regs;
  bool reg_used[NUM_ALLOCATABLE_INT_REGS];
};


static enum AsmRegister allocatable_sse_regs[] = {
    XMM0,
    XMM1,
    XMM2,
    XMM3,
    XMM4,
    XMM5,
    XMM6,
    XMM7,
    /* XMM8 reserved for fixup scratch */
    XMM9,
    XMM10,
    XMM11,
    XMM12,
    XMM13,
    XMM14,
    XMM15,
};


#define NUM_ALLOCATABLE_SSE_REGS \
  ((int) (sizeof(allocatable_sse_regs) / sizeof(allocatable_sse_regs[0])))

struct LiveInterval {
  char *pseudo;
  int start;
  int end;
  struct AsmType asm_type;
};


struct PseudoType {
  char *pseudo;
  struct AsmType asm_type;
};


typedef Vector(struct PseudoType) VecPseudoType;


typedef Vector(struct LiveInterval) VecLiveInterval;

typedef Vector(char *) VecPseudo;


struct InstrLiveness {
  VecPseudo use;
  VecPseudo def;
  VecPseudo live_before;
  VecPseudo live_after;
};


struct BasicBlock {
  int start; /* inclusive instruction index */
  int end;   /* inclusive instruction index */

  VecInt succs;

  VecPseudo use;
  VecPseudo def;

  VecPseudo live_in;
  VecPseudo live_out;
};


struct CFG {
  struct BasicBlock *blocks;
  int block_count;

  /*
   * instr_to_block[i] tells us which block owns instruction i.
   */
  int *instr_to_block;
};


enum PseudoHomeKind {
  PSEUDO_HOME_REG,
  PSEUDO_HOME_STACK,
};


struct PseudoHome {
  char *pseudo;
  enum PseudoHomeKind kind;
  union {
    enum AsmRegister reg;
    int stack_offset;
  } as;
};


typedef Vector(struct PseudoHome) VecPseudoHome;


struct ActiveInterval {
  char *pseudo;
  int end;
  enum AsmRegister reg;
};


typedef Vector(struct ActiveInterval) VecActiveInterval;


struct InterferenceNode {
  /*
   * For pseudo nodes:      name = "var.x.0"
   * For precolored nodes: name = "%eax", "%edx", etc.
   */
  char *pseudo;

  bool is_precolored;
  enum AsmRegister precolored_reg;

  VecInt neighbors;
};


typedef Vector(struct InterferenceNode) VecInterferenceNode;


struct InterferenceGraph {
  VecInterferenceNode nodes;
};


static enum AsmRegister caller_saved_int_regs[] = {
    AX, DX, CX, SI, DI, R8, R9,
};


#define NUM_CALLER_SAVED_INT_REGS \
  ((int) (sizeof(caller_saved_int_regs) / sizeof(caller_saved_int_regs[0])))

static enum AsmRegister caller_saved_sse_regs[] = {
    XMM0, XMM1,  XMM2,  XMM3,  XMM4,  XMM5,  XMM6,  XMM7,
    XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15,
};


#define NUM_CALLER_SAVED_SSE_REGS \
  ((int) (sizeof(caller_saved_sse_regs) / sizeof(caller_saved_sse_regs[0])))

static enum AsmRegister callee_saved_int_regs[] = {
    BX, R12, R13, R14, R15,
};


struct AbiRegParamMove {
  enum AsmRegister src_reg;
  char *dst_pseudo;
  struct AsmType asm_type;
};


typedef Vector(struct AbiRegParamMove) VecAbiRegParamMove;


struct SelectStackEntry {
  int node_idx;
  bool was_spill_candidate;
};


typedef Vector(struct SelectStackEntry) VecSelectStack;


struct SpillCost {
  char *pseudo;
  double cost;
};


typedef Vector(struct SpillCost) VecSpillCost;


enum RegClass {
  REGCLASS_NONE,
  REGCLASS_INT,
  REGCLASS_SSE,
};


struct Move {
  int src_idx;
  int dst_idx;
};


typedef Vector(struct Move) VecMove;


struct AssembleLinkResult {
  bool is_ok;
  char *msg;
};


enum TargetStage {
  STAGE_FULL,
  STAGE_TOKENIZE,
  STAGE_PARSE,
  STAGE_RESOLVE,
  STAGE_TYPECHECK,
  STAGE_IR,
  STAGE_IR_OPT,
  STAGE_CODEGEN_RAW,
  STAGE_CODEGEN_REPLACE_PSEUDO,
  STAGE_CODEGEN_FIXUP,
  STAGE_EMIT,
  STAGE_ASM,
  STAGE_LINK,
};


struct CompilerOptions {
  enum TargetStage target_stage;
  char *path;
};


struct RunResult {
  bool is_ok;
  char *msg;
};

#endif /* MINI_COMPILER_COMMON_H */
