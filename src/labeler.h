#ifndef X_LABELER_H
#define X_LABELER_H

#include <stdbool.h>

#include "parser.h"
#include "vector.h"

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

struct LoopLabelResult loop_label(struct AST *ast);
struct CollectLabelsResult collect_labels(struct AST *ast);
struct LabelCheckResult check_labels(struct AST *ast, VecCharPtr *labels);
void free_labels(VecCharPtr *labels);

#endif
