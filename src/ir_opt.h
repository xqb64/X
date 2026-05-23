#ifndef X_IR_OPT_H
#define X_IR_OPT_H

#include "vector.h"

#define MAX_OPTIMIZATION_PASSES 100

typedef Vector(char *) VecIRNameSet;

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

void optimize_ir(struct IRProgram *prog);

#endif
