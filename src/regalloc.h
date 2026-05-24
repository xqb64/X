#ifndef X_REGALLOC_H
#define X_REGALLOC_H

#include "codegen.h"

struct RegallocResult {
  bool is_ok;
  char *msg;
  struct AsmProgram *prog;
};

struct RegallocResult regalloc(struct AsmProgram *asmcode);

#endif
