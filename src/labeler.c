#include "labeler.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "util.h"

static struct LoopLabelResult loop_label_stmt(struct Stmt *stmt, char *label)
{
  switch (stmt->kind) {
    case STMT_LOOP: {
      char *new_label;
      int tmp;

      tmp = mktmp();

      new_label = mklbl("Loop", tmp);

      stmt->as.loop.label = new_label;

      loop_label_stmt(stmt->as.loop.body, new_label);
      break;
    }
    case STMT_DO_WHILE: {
      char *new_label;
      int tmp;

      tmp = mktmp();

      new_label = mklbl("DoWhile", tmp);

      stmt->as.do_while_stmt.label = new_label;

      loop_label_stmt(stmt->as.do_while_stmt.body, new_label);
      break;
    }
    case STMT_WHILE: {
      char *new_label;
      int tmp;

      tmp = mktmp();

      new_label = mklbl("While", tmp);

      stmt->as.while_stmt.label = new_label;

      loop_label_stmt(stmt->as.while_stmt.body, new_label);
      break;
    }
    case STMT_BREAK: {
      stmt->as.break_stmt.label = label;
      break;
    }
    case STMT_CONTINUE: {
      stmt->as.continue_stmt.label = label;
      break;
    }
    case STMT_LABELED: {
      struct LoopLabelResult r;

      r = loop_label_stmt(stmt->as.labeled.stmt, label);
      if (!r.is_ok) {
        return r;
      }

      break;
    }
    case STMT_FN: {
      for (int i = 0; i < stmt->as.fn.body.len; i++) {
        struct LoopLabelResult r;
        r = loop_label_stmt(&stmt->as.fn.body.data[i], label);
        if (!r.is_ok) {
          return r;
        }
      }
      break;
    }
    case STMT_IF: {
      struct LoopLabelResult then_res, else_res;

      then_res = loop_label_stmt(stmt->as.if_stmt.then_block, label);
      if (!then_res.is_ok) {
        return then_res;
      }

      if (stmt->as.if_stmt.else_block) {
        else_res = loop_label_stmt(stmt->as.if_stmt.else_block, label);
        if (!else_res.is_ok) {
          return else_res;
        }
      }
      break;
    }
    case STMT_BLOCK: {
      for (int i = 0; i < stmt->as.block.stmts.len; i++) {
        struct LoopLabelResult r;
        r = loop_label_stmt(&stmt->as.block.stmts.data[i], label);
        if (!r.is_ok) {
          return r;
        }
      }
      break;
    }
    case STMT_RET:
    case STMT_EXPR:
    case STMT_LET:
    case STMT_EXTERN:
    case STMT_STRUCT:
    case STMT_ENUM:
    case STMT_GOTO:
      break;
    default:
      assert(0);
  }
  return (struct LoopLabelResult){.is_ok = true, .msg = NULL};
}

struct LoopLabelResult loop_label(struct AST *ast)
{
  for (int i = 0; i < ast->stmts.len; i++) {
    struct LoopLabelResult r;
    r = loop_label_stmt(&ast->stmts.data[i], NULL);
    if (!r.is_ok) {
      return r;
    }
  }
  return (struct LoopLabelResult){.is_ok = true, .msg = NULL, .ast = ast};
}

static struct CollectLabelsResult collect_labels_stmt(struct Stmt *stmt,
                                                      VecCharPtr *labels,
                                                      char *funcname)
{
  switch (stmt->kind) {
    case STMT_LABELED: {
      struct CollectLabelsResult r;
      char *label;

      label = mkstr("%s.%s", funcname, stmt->as.labeled.label);

      for (int i = 0; i < labels->len; i++) {
        if (strcmp(labels->data[i], label) == 0) {
          free(labels->data[i]);
          vec_free(labels);
          free(label);
          return (struct CollectLabelsResult){
              .is_ok = false, .msg = "Duplicate label", .labels = {0}};
        }
      }

      vec_insert(labels, label);

      r = collect_labels_stmt(stmt->as.labeled.stmt, labels, funcname);
      if (!r.is_ok) {
        return r;
      }
      break;
    }
    case STMT_FN: {
      for (int i = 0; i < stmt->as.fn.body.len; i++) {
        struct CollectLabelsResult r;

        r = collect_labels_stmt(&stmt->as.fn.body.data[i], labels,
                                stmt->as.fn.name);
        if (!r.is_ok) {
          return r;
        }
      }
      break;
    }
    case STMT_BLOCK: {
      for (int i = 0; i < stmt->as.block.stmts.len; i++) {
        struct CollectLabelsResult r;

        r = collect_labels_stmt(&stmt->as.block.stmts.data[i], labels,
                                funcname);
        if (!r.is_ok) {
          return r;
        }
      }
      break;
    }
    case STMT_IF: {
      struct CollectLabelsResult r1, r2;

      r1 = collect_labels_stmt(stmt->as.if_stmt.then_block, labels, funcname);
      if (!r1.is_ok) {
        return r1;
      }

      if (stmt->as.if_stmt.else_block) {
        r2 = collect_labels_stmt(stmt->as.if_stmt.else_block, labels, funcname);
        if (!r2.is_ok) {
          return r2;
        }
      }
      break;
    }
    case STMT_WHILE: {
      struct CollectLabelsResult r;

      r = collect_labels_stmt(stmt->as.while_stmt.body, labels, funcname);
      if (!r.is_ok) {
        return r;
      }

      break;
    }
    default:
      break;
  }
  return (struct CollectLabelsResult){.is_ok = true, .msg = NULL};
}

struct CollectLabelsResult collect_labels(struct AST *ast)
{
  VecCharPtr labels = {0};

  for (int i = 0; i < ast->stmts.len; i++) {
    struct CollectLabelsResult r;

    r = collect_labels_stmt(&ast->stmts.data[i], &labels, NULL);
    if (!r.is_ok) {
      for (int j = 0; j < labels.len; j++) {
        free(labels.data[j]);
      }
      vec_free(&labels);
      return r;
    }
  }

  for (int i = 0; i < labels.len; i++) {
    printf("found label: %s\n", labels.data[i]);
  }

  return (struct CollectLabelsResult){
      .is_ok = true, .msg = NULL, .labels = labels};
}

static struct LabelCheckResult check_labels_stmt(struct Stmt *stmt,
                                                 VecCharPtr *labels,
                                                 char *funcname)
{
  switch (stmt->kind) {
    case STMT_FN: {
      for (int i = 0; i < stmt->as.fn.body.len; i++) {
        struct LabelCheckResult r;

        r = check_labels_stmt(&stmt->as.fn.body.data[i], labels,
                              stmt->as.fn.name);
        if (!r.is_ok) {
          return r;
        }
      }
      break;
    }
    case STMT_GOTO: {
      bool is_found;
      char *label;

      is_found = false;
      label = mkstr("%s.%s", funcname, stmt->as.goto_stmt.label);

      printf("labels->len is: %d\n", labels->len);
      for (int i = 0; i < labels->len; i++) {
        printf("label inside is: %s\n", labels->data[i]);
        if (strcmp(labels->data[i], label) == 0) {
          is_found = true;
          break;
        }
      }

      if (!is_found) {
        return (struct LabelCheckResult){
            .is_ok = false, .msg = mkstr("No label %s found", label)};
      }

      free(label);
      return (struct LabelCheckResult){.is_ok = true, .msg = NULL};
    }
    case STMT_LOOP: {
      struct LabelCheckResult body_res;

      body_res = check_labels_stmt(stmt->as.loop.body, labels, funcname);
      if (!body_res.is_ok) {
        return body_res;
      }

      break;
    }
    case STMT_IF: {
      struct LabelCheckResult then_res, else_res;

      then_res =
          check_labels_stmt(stmt->as.if_stmt.then_block, labels, funcname);
      if (!then_res.is_ok) {
        return then_res;
      }

      if (stmt->as.if_stmt.else_block) {
        else_res =
            check_labels_stmt(stmt->as.if_stmt.else_block, labels, funcname);
        if (!else_res.is_ok) {
          return else_res;
        }
      }

      break;
    }
    case STMT_WHILE: {
      struct LabelCheckResult body_res;

      body_res = check_labels_stmt(stmt->as.while_stmt.body, labels, funcname);
      if (!body_res.is_ok) {
        return body_res;
      }

      break;
    }
    case STMT_LABELED: {
      struct LabelCheckResult labeled_res;

      labeled_res = check_labels_stmt(stmt->as.labeled.stmt, labels, funcname);
      if (!labeled_res.is_ok) {
        return labeled_res;
      }

      break;
    }
    default:
      break;
  }
  return (struct LabelCheckResult){.is_ok = true, .msg = NULL};
}

struct LabelCheckResult check_labels(struct AST *ast, VecCharPtr *labels)
{
  for (int i = 0; i < ast->stmts.len; i++) {
    struct LabelCheckResult r;

    r = check_labels_stmt(&ast->stmts.data[i], labels, NULL);
    if (!r.is_ok) {
      return r;
    }
  }
  return (struct LabelCheckResult){.is_ok = true, .msg = NULL};
}

void free_labels(VecCharPtr *labels)
{
  for (int i = 0; i < labels->len; i++) {
    free(labels->data[i]);
  }
  vec_free(labels);
}
