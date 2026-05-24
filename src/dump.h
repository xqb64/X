#ifndef X_DUMP_H
#define X_DUMP_H

#ifdef DEBUG_ENABLE_DUMPS
#include "codegen.h"
#include "parser.h"
#include "ir.h"

#include "stdio.h"

void print_ast(struct AST *ast);
void print_ir(struct IRProgram *prog);
void print_asm(struct AsmProgram *prog);
#endif

#endif
