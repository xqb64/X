#ifndef X_EMITTER_H
#define X_EMITTER_H

#include "codegen.h"

void emit(struct AsmProgram *prog, char *path);
struct AssembleLinkResult assemble_and_link(const char *path,
                                            const char *out_path,
                                            bool assemble_only);

#endif
