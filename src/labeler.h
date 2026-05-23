#ifndef X_LABELER_H
#define X_LABELER_H

#include "vector.h"
#include "parser.h"

#include <stdbool.h>

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

struct LoopLabelResult loop_label_stmt(struct Stmt *stmt, char *label);
struct LoopLabelResult loop_label(struct AST *ast);
struct CollectLabelsResult collect_labels_stmt(struct Stmt *stmt,
                                               VecCharPtr *labels,
                                               char *funcname);
struct CollectLabelsResult collect_labels(struct AST *ast);
struct LabelCheckResult check_labels_stmt(struct Stmt *stmt, VecCharPtr *labels,
                                          char *funcname);
struct LabelCheckResult check_labels(struct AST *ast, VecCharPtr *labels);
void free_labels(VecCharPtr *labels);

#endif
