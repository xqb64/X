#ifndef X_PARSER_H
#define X_PARSER_H

#include "tokenizer.h"
#include "vector.h"

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

struct Parameter {
  char *name;
  Type type;
  bool is_mut;
};

typedef Vector(struct Stmt) VecStmt;
typedef Vector(struct Decl) VecDecl;
typedef Vector(struct Parameter) VecParam;

enum StmtKind {
  STMT_LET,
  STMT_RET,
  STMT_IF,
  STMT_DO_WHILE,
  STMT_WHILE,
  STMT_LOOP,
  STMT_BREAK,
  STMT_CONTINUE,
  STMT_GOTO,
  STMT_LABELED,
  STMT_BLOCK,
  STMT_EXPR,
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

struct StmtDoWhile {
  struct Expr cond;
  struct Stmt *body;
  char *label;
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

struct StmtExpr {
  struct Expr expr;
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

struct EnumVariant {
  char *name;
  int value;
};

typedef Vector(struct EnumVariant) VecEnumVariant;

enum DeclKind {
  DECL_FN,
  DECL_STRUCT,
  DECL_VARIABLE,
  DECL_ENUM,
  DECL_UNION,
};

struct DeclFn {
  char *name;
  VecParam params;
  Type retval;
  VecStmt body;
  bool is_extern;
  bool is_variadic;
};

struct DeclStruct {
  char *name;
  VecStructField fields;
};

struct DeclUnion {
  char *name;
  VecStructField fields;
};

struct DeclVariable {
  char *name;
  Type type;
  struct Expr *init;
  bool is_mut;
  bool is_extern;
};

struct DeclEnum {
  char *name;
  VecEnumVariant variants;
};

struct Decl {
  enum DeclKind kind;
  union {
    struct DeclFn fn;
    struct DeclStruct struct_decl;
    struct DeclUnion union_decl;
    struct DeclVariable variable;
    struct DeclEnum enum_decl;
  } as;
};

struct Stmt {
  enum StmtKind kind;
  union {
    struct StmtLet let;
    struct StmtRet ret;
    struct StmtIf if_stmt;
    struct StmtDoWhile do_while_stmt;
    struct StmtWhile while_stmt;
    struct StmtLoop loop;
    struct StmtBreak break_stmt;
    struct StmtContinue continue_stmt;
    struct StmtGoto goto_stmt;
    struct StmtLabeled labeled;
    struct StmtBlock block;
    struct StmtExpr expr_stmt;
  } as;
};

struct AST {
  VecDecl decls;
};

struct Parser {
  struct Tokenizer *tokenizer;

  // Value buffers
  struct Token prev_tok;
  struct Token curr_tok;
  struct Token peek_tok;
  bool has_peek;

  // Pointers for backwards compatibility with existing parser code
  struct Token *prev;
  struct Token *curr;

  struct DeclFn *current_fn;
  VecDecl *global_decls;
};

struct ParseFnResult {
  bool is_ok;
  char *msg;
  union {
    struct Expr expr;
    struct Stmt stmt;
    struct Decl decl;
  } as;
};

struct ParseResult {
  bool is_ok;
  char *msg;
  struct AST *ast;
};

bool types_equal(Type a, Type b);
void init_parser(struct Tokenizer *tokenizer, struct Parser *parser);
struct ParseResult parse(struct Parser *parser);
void print_expr(struct Expr *expr, int spaces);
void free_expr(struct Expr *expr);
void print_stmt(struct Stmt *stmt, int spaces);
void print_decl(struct Decl *decl, int spaces);
void free_stmt(struct Stmt *stmt);
void free_decl(struct Decl *decl);
void print_ast(struct AST *ast);
void free_ast(struct AST *ast);

#if defined(DEBUG_PARSER) || defined(DEBUG_RESOLVER) || defined(DEBUG_TYPECHECKER) || defined(DEBUG_LABELER)
void print_ast(struct AST *ast);
#endif

#endif
