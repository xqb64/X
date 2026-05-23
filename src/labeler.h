#ifndef MINI_COMPILER_LABELER_H
#define MINI_COMPILER_LABELER_H

#include "common.h"

/* labeler */

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

#endif /* MINI_COMPILER_LABELER_H */
