#ifndef X_EMITTER_H
#define X_EMITTER_H

#include "codegen.h"

struct EmitResult {
  bool is_ok;
  char *msg;
};

struct EmitResult emit(struct AsmProgram *prog, char *path);

#endif
