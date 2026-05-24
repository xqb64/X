#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "codegen.h"
#include "emitter.h"
#include "ir.h"
#include "ir_opt.h"
#include "labeler.h"
#include "parser.h"
#include "regalloc.h"
#include "resolver.h"
#include "tokenizer.h"
#include "typechecker.h"
#include "util.h"

struct ReadFileResult {
  bool is_ok;
  char *msg;
  char *contents;
};

struct ReadFileResult read_file(const char *path)
{
  struct ReadFileResult result;
  FILE *f;
  int seek_result;
  long offset;
  size_t bytes_read;
  char *buf;

  result.is_ok = true;
  result.msg = NULL;
  result.contents = NULL;

  f = fopen(path, "r");
  if (!f) {
    result.is_ok = false;
    result.msg = "fopen";
    goto end;
  }

  seek_result = fseek(f, 0L, SEEK_END);
  if (seek_result != 0) {
    result.is_ok = false;
    result.msg = "fseek";
    goto close_then_end;
  }

  offset = ftell(f);
  if (offset == -1) {
    result.is_ok = false;
    result.msg = "ftell";
    goto close_then_end;
  }

  rewind(f);

  buf = malloc(offset + 1);
  if (!buf) {
    result.is_ok = false;
    result.msg = "malloc";
    goto close_then_end;
  }

  bytes_read = fread(buf, 1, offset, f);
  if (bytes_read < (size_t) offset) {
    result.is_ok = false;
    if (ferror(f) != 0) {
      result.msg = "ferror";
    } else {
      if (feof(f) != 0) {
        result.msg = "feof";
      } else {
        result.msg = "unknown error during fread";
      }
      goto dealloc_then_close_then_end;
    }
  } else {
    buf[offset] = '\0';
    result.contents = buf;
    goto close_then_end;
  }

dealloc_then_close_then_end:
  free(buf);

close_then_end:
  fclose(f);

end:
  return result;
}

struct AssembleLinkResult {
  bool is_ok;
  char *msg;
};

struct AssembleLinkResult assemble_and_link(const char *path,
                                            const char *out_path,
                                            bool assemble_only)
{
  struct AssembleLinkResult result;
  result.is_ok = true;
  result.msg = NULL;

  pid_t pid = fork();

  if (pid < 0) {
    result.is_ok = false;
    result.msg = "Fork failed";
    return result;
  } else if (pid == 0) {
    if (assemble_only) {
      execlp("gcc", "gcc", "-c", path, "-o", out_path, NULL);
    } else {
      execlp("gcc", "gcc", path, "-o", out_path, NULL);
    }

    exit(EXIT_FAILURE);
  } else {
    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
      return result;
    } else {
      result.is_ok = false;
      result.msg = assemble_only
                       ? "gcc failed at the assemble stage\n"
                       : "gcc failed at the assemble-and-link stage\n";
      return result;
    }
  }
}

enum TargetStage {
  STAGE_FULL,
  STAGE_TOKENIZE,
  STAGE_PARSE,
  STAGE_RESOLVE,
  STAGE_TYPECHECK,
  STAGE_IR,
  STAGE_IR_OPT,
  STAGE_CODEGEN_RAW,
  STAGE_CODEGEN_REPLACE_PSEUDO,
  STAGE_CODEGEN_FIXUP,
  STAGE_EMIT,
  STAGE_ASM,
  STAGE_LINK,
};

struct CompilerOptions {
  enum TargetStage target_stage;
  char *path;
};

struct CompilerOptions parse_args(int argc, char **argv)
{
  struct CompilerOptions opts;
  opts.target_stage = STAGE_FULL;

  static struct option long_options[] = {
      {"tokenize", no_argument, 0, 't'},
      {"parse", no_argument, 0, 'p'},
      {"resolve", no_argument, 0, 'r'},
      {"typecheck", no_argument, 0, 'c'},
      {"ir", no_argument, 0, 'i'},
      {"ir-opt", no_argument, 0, 'I'},
      {"codegen", required_argument, 0, 'g'},
      {"raw", no_argument, 0, 'R'},
      {"replace-pseudo", no_argument, 0, 'P'},
      {"fixup", no_argument, 0, 'F'},
      {"emit", no_argument, 0, 'e'},
      {"asm", no_argument, 0, 'a'},
      {"ln", no_argument, 0, 'l'},
      {0, 0, 0, 0}};

  int opt;
  int option_index = 0;

  while ((opt = getopt_long(argc, argv, "tprciIg:", long_options,
                            &option_index)) != -1) {
    switch (opt) {
      case 't':
        opts.target_stage = STAGE_TOKENIZE;
        break;
      case 'p':
        opts.target_stage = STAGE_PARSE;
        break;
      case 'r':
        opts.target_stage = STAGE_RESOLVE;
        break;
      case 'c':
        opts.target_stage = STAGE_TYPECHECK;
        break;
      case 'i':
        opts.target_stage = STAGE_IR;
        break;
      case 'I':
        opts.target_stage = STAGE_IR_OPT;
        break;
      case 'g':
        if (strcmp(optarg, "raw") == 0 || strcmp(optarg, "--raw") == 0) {
          opts.target_stage = STAGE_CODEGEN_RAW;
        } else if (strcmp(optarg, "replace-pseudo") == 0 ||
                   strcmp(optarg, "--replace-pseudo") == 0) {
          opts.target_stage = STAGE_CODEGEN_REPLACE_PSEUDO;
        } else if (strcmp(optarg, "fixup") == 0 ||
                   strcmp(optarg, "--fixup") == 0) {
          opts.target_stage = STAGE_CODEGEN_FIXUP;
        } else {
          fprintf(stderr, "Unknown codegen stage: %s\n", optarg);
          exit(1);
        }
        break;
      case 'R':
        opts.target_stage = STAGE_CODEGEN_RAW;
        break;
      case 'P':
        opts.target_stage = STAGE_CODEGEN_REPLACE_PSEUDO;
        break;
      case 'F':
        opts.target_stage = STAGE_CODEGEN_FIXUP;
        break;
      case 'e':
        opts.target_stage = STAGE_EMIT;
        break;
      case 'a':
        opts.target_stage = STAGE_ASM;
        break;
      case 'l':
        opts.target_stage = STAGE_LINK;
        break;
      case '?':
        exit(1);
      default:
        abort();
    }
  }

  if (optind < argc) {
    opts.path = argv[optind];
  }

  return opts;
}

struct RunResult {
  bool is_ok;
  char *msg;
};

struct RunResult run(struct CompilerOptions *opts)
{
  enum TargetStage target_stage;
  char *path;
  struct ReadFileResult read_file_result;
  char *src;
  struct Tokenizer tokenizer;
  struct Parser parser;
  struct ParseResult parse_result;
  struct AST *ast, *resolved_ast, *typechecked_ast, *labeled_ast;
  struct ResolveResult resolve_result;
  struct TypecheckResult typecheck_result;
  struct LoopLabelResult loop_label_result;
  struct CollectLabelsResult collect_labels_result;
  struct LabelCheckResult label_check_result;
  struct IrfyResult irfy_result;
  struct IRProgram ir_prog;
  struct AsmResult asm_result;
  struct AsmProgram asm_prog;
  struct RunResult r;
  char *asm_path, *o_path, *exe_path;

  asm_path = NULL;
  o_path = NULL;
  exe_path = NULL;

  r.is_ok = true;
  r.msg = NULL;

  target_stage = opts->target_stage;
  path = opts->path;

  read_file_result = read_file(path);
  if (!read_file_result.is_ok) {
    r.msg = "Couldn't read file";
    r.is_ok = false;
    goto free_up2_fread;
  }

  src = read_file_result.contents;

  init_tokenizer(&tokenizer, src);

  init_parser(&tokenizer, &parser);
  parse_result = parse(&parser);

  if (!parse_result.is_ok) {
    r.msg = parse_result.msg;
    r.is_ok = false;
    goto free_up2_parse;
  }

  ast = parse_result.ast;

#ifdef DEBUG_PARSER
  print_ast(ast);
#endif

  if (target_stage == STAGE_PARSE) {
    goto free_up2_parse;
  }

  resolve_result = resolve(ast);
  if (!resolve_result.is_ok) {
    r.msg = resolve_result.msg;
    r.is_ok = false;
    goto free_up2_parse;
  }

  resolved_ast = resolve_result.as.ast;

#ifdef DEBUG_RESOLVER
  printf("resolved ast:\n");
  print_ast(resolved_ast);
#endif

  if (target_stage == STAGE_RESOLVE) {
    goto free_up2_parse;
  }

  typecheck_result = typecheck(resolved_ast);
  if (!typecheck_result.is_ok) {
    r.msg = typecheck_result.msg;
    r.is_ok = false;
    goto free_up2_parse;
  }

  typechecked_ast = typecheck_result.ast;

#ifdef DEBUG_TYPECHECKER
  printf("typechecked ast:\n");
  print_ast(typechecked_ast);
#endif

  if (target_stage == STAGE_TYPECHECK) {
    goto free_up2_parse;
  }

  loop_label_result = loop_label(typechecked_ast);
  if (!loop_label_result.is_ok) {
    r.msg = loop_label_result.msg;
    r.is_ok = false;
    goto free_up2_parse;
  }

  labeled_ast = loop_label_result.ast;

#ifdef DEBUG_LABELER
  printf("labeled ast:\n");
  print_ast(labeled_ast);
#endif

  collect_labels_result = collect_labels(labeled_ast);
  if (!collect_labels_result.is_ok) {
    r.msg = collect_labels_result.msg;
    r.is_ok = false;
    goto free_up2_collect_labels;
  }

  label_check_result = check_labels(labeled_ast, &collect_labels_result.labels);
  if (!label_check_result.is_ok) {
    r.msg = label_check_result.msg;
    r.is_ok = false;
    goto free_up2_collect_labels;
  }

  irfy_result = irfy_ast(labeled_ast);
  if (!irfy_result.is_ok) {
    r.msg = irfy_result.msg;
    r.is_ok = false;
    goto free_up2_irfy;
  }

  ir_prog = irfy_result.prog;

#ifdef DEBUG_IR
  printf("raw ir:\n");
  print_ir(&ir_prog);
#endif

  if (target_stage == STAGE_IR) {
    goto free_up2_irfy;
  }

  optimize_ir(&ir_prog);

#ifdef DEBUG_IR_OPT
  printf("optimized ir:\n");
  print_ir(&ir_prog);
#endif

  if (target_stage == STAGE_IR_OPT) {
    goto free_up2_irfy;
  }

  asm_result = codegen(&ir_prog);
  if (!asm_result.is_ok) {
    r.msg = asm_result.msg;
    r.is_ok = false;
    goto free_up2_asm;
  }

  asm_prog = asm_result.prog;

#ifdef DEBUG_CODEGEN_RAW
  print_asm(&asm_prog);
#endif

  if (target_stage == STAGE_CODEGEN_RAW) {
    goto free_up2_asm;
  }

  asm_prog = *regalloc(&asm_prog);

#ifdef DEBUG_CODEGEN_REGALLOC
  printf("regalloc\n");
  print_asm(&asm_prog);
#endif

  if (target_stage == STAGE_CODEGEN_REPLACE_PSEUDO) {
    goto free_up2_asm;
  }

  asm_prog = *fixup(&asm_prog);

#ifdef DEBUG_CODEGEN_FIXUP
  printf("fixup...\n");
  print_asm(&asm_prog);
#endif

  if (target_stage == STAGE_CODEGEN_FIXUP) {
    goto free_up2_asm;
  }

  asm_path = replace_ext(path, "s");

  emit(&asm_prog, asm_path);
  if (target_stage == STAGE_EMIT) {
    goto free_up2_asm;
  }

  o_path = replace_ext(path, "o");
  exe_path = strip_ext(path);

  if (target_stage == STAGE_ASM) {
    assemble_and_link(asm_path, o_path, true);
  } else if (target_stage == STAGE_LINK || target_stage == STAGE_FULL) {
    assemble_and_link(asm_path, exe_path, false);
  }

free_up2_asm:
  free_asm(&asm_result.prog);
  free(asm_path);
  free(o_path);
  free(exe_path);

free_up2_irfy:
  free_ir_prog(&irfy_result.prog);

free_up2_collect_labels:
  free_labels(&collect_labels_result.labels);

free_up2_parse:
  free_ast(parse_result.ast);

free_up2_fread:
  free(read_file_result.contents);

  free_global_constants();
  free_enum_types();
  free_enum_variants();

  return r;
}

int main(int argc, char **argv)
{
  struct CompilerOptions opts;
  struct RunResult r;

  opts = parse_args(argc, argv);
  r = run(&opts);
  if (!r.is_ok) {
    fprintf(stderr, "err: %s\n", r.msg);
    return 1;
  }

  return 0;
}
