#ifndef X_REGALLOC_H
#define X_REGALLOC_H

#include "codegen.h"
#include "vector.h"

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

#define NUM_CALLEE_SAVED_INT_REGS \
  ((int) (sizeof(callee_saved_int_regs) / sizeof(callee_saved_int_regs[0])))

#define MAX_REGALLOC_ATTEMPTS 20

struct AsmProgram *regalloc(struct AsmProgram *asmcode);

#endif
