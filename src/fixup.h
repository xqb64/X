#ifndef X_FIXUP_H
#define X_FIXUP_H

#include "codegen.h"

struct FixupResult {
  bool is_ok;
  char *msg;
  struct AsmProgram *prog;
};

struct FixupResult fixup(struct AsmProgram *prog);

#endif
