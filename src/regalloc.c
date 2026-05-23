#include "regalloc.h"
#include "util.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

bool map_get_offset(struct Map *map, char *name, int *out_offset)
{
  for (struct Map *curr = map; curr; curr = curr->next) {
    if (curr->name && strcmp(curr->name, name) == 0) {
      *out_offset = curr->offset;
      return true;
    }
  }

  return false;
}

int get_offset(struct Map *map, char *name, struct AsmType asm_type,
               int *used_stack_bytes)
{
  struct Map *curr = map;

  while (curr) {
    if (curr->name && strcmp(curr->name, name) == 0) {
      return curr->offset;
    }

    if (!curr->next) {
      break;
    }

    curr = curr->next;
  }

  int size = asm_type_stack_size(asm_type);
  int align = asm_type_stack_align(asm_type);

  int aligned_used = align_up_int(*used_stack_bytes, align);
  int new_used = aligned_used + size;
  int new_offset = -new_used;

  struct Map *new_entry = malloc(sizeof(struct Map));
  assert(new_entry);

  new_entry->next = NULL;
  new_entry->name = name;
  new_entry->offset = new_offset;
  new_entry->asm_type = asm_type;

  curr->next = new_entry;
  *used_stack_bytes = new_used;

  return new_offset;
}

void replace_operand_pseudo(struct AsmOperand *op,
                                          struct Map *map,
                                          int *used_stack_bytes)
{
  if (op->kind == AsmOperand_PSEUDO) {
    for (int k = 0; k < global_constants.len; k++) {
      if (strcmp(op->as.pseudo, global_constants.data[k].name) == 0) {
        op->kind = AsmOperand_DATA;
        op->as.data = strdup(op->as.pseudo);
        return;
      }
    }

    op->kind = AsmOperand_STACK;
    op->as.stack_offset =
        get_offset(map, op->as.pseudo, op->asm_type, used_stack_bytes);
  }
}

bool is_tmp(char *name)
{
  return strncmp(name, "tmp.", 4) == 0;
}

struct AsmType *pseudo_type_get(VecPseudoType *types, char *pseudo)
{
  for (int i = 0; i < types->len; i++) {
    if (strcmp(types->data[i].pseudo, pseudo) == 0) {
      return &types->data[i].asm_type;
    }
  }

  return NULL;
}

void pseudo_type_add(VecPseudoType *types, char *pseudo,
                            struct AsmType asm_type)
{
  if (pseudo_type_get(types, pseudo)) {
    return;
  }

  struct PseudoType item;

  item.pseudo = strdup(pseudo);
  if (!item.pseudo) {
    perror("strdup");
    exit(1);
  }

  item.asm_type = asm_type;

  vec_insert(types, item);
}

void remember_operand_type(VecPseudoType *types, struct AsmOperand *op)
{
  if (op->kind == AsmOperand_PSEUDO) {
    pseudo_type_add(types, op->as.pseudo, op->asm_type);
  }
}

VecPseudoType collect_pseudo_types(struct AsmFunction *fn)
{
  VecPseudoType types = {0};

  for (int i = 0; i < fn->body.len; i++) {
    struct AsmInstr *instr = &fn->body.data[i];

    switch (instr->kind) {
      case AsmInstr_MOV:
        remember_operand_type(&types, &instr->as.mov.src);
        remember_operand_type(&types, &instr->as.mov.dst);
        break;

      case AsmInstr_BIN:
        remember_operand_type(&types, &instr->as.binary.lhs);
        remember_operand_type(&types, &instr->as.binary.rhs);
        break;

      case AsmInstr_CMP:
        remember_operand_type(&types, &instr->as.cmp.lhs);
        remember_operand_type(&types, &instr->as.cmp.rhs);
        break;

      case AsmInstr_CVT:
        remember_operand_type(&types, &instr->as.cvt.src);
        remember_operand_type(&types, &instr->as.cvt.dst);
        break;

      case AsmInstr_SetCC:
        remember_operand_type(&types, &instr->as.setcc.op);
        break;

      case AsmInstr_UNARY:
        remember_operand_type(&types, &instr->as.unary.op);
        break;

      case AsmInstr_PUSH:
        remember_operand_type(&types, &instr->as.push.op);
        break;

      case AsmInstr_POP:
        remember_operand_type(&types, &instr->as.pop.op);
        break;

      case AsmInstr_LEA:
        remember_operand_type(&types, &instr->as.lea.dst);
        break;

      case AsmInstr_CALL:
      case AsmInstr_RET:
      case AsmInstr_JMP:
      case AsmInstr_JmpCC:
      case AsmInstr_LBL:
      case AsmInstr_REP_MOVSB:
        break;

      default:
        assert(0 && "Unhandled instruction in collect_pseudo_types");
    }
  }

  return types;
}

void free_pseudo_types(VecPseudoType *types)
{
  for (int i = 0; i < types->len; i++) {
    free(types->data[i].pseudo);
  }

  vec_free(types);

  types->capacity = 0;
  types->len = 0;
  types->data = NULL;
}

int find_live_interval(VecLiveInterval *intervals, char *pseudo)
{
  for (int i = 0; i < intervals->len; i++) {
    if (strcmp(intervals->data[i].pseudo, pseudo) == 0) {
      return i;
    }
  }

  return -1;
}

void touch_live_interval(VecLiveInterval *intervals, char *pseudo,
                                struct AsmType asm_type, int instr_idx)
{
  int idx = find_live_interval(intervals, pseudo);

  if (idx >= 0) {
    if (instr_idx < intervals->data[idx].start) {
      intervals->data[idx].start = instr_idx;
    }

    if (instr_idx > intervals->data[idx].end) {
      intervals->data[idx].end = instr_idx;
    }

    return;
  }

  struct LiveInterval interval;

  interval.pseudo = strdup(pseudo);
  if (!interval.pseudo) {
    perror("strdup");
    exit(1);
  }

  interval.start = instr_idx;
  interval.end = instr_idx;
  interval.asm_type = asm_type;

  vec_insert(intervals, interval);
}

void touch_pseudo_set(VecLiveInterval *intervals, VecPseudoType *types,
                             VecPseudo *set, int instr_idx)
{
  for (int i = 0; i < set->len; i++) {
    struct AsmType *asm_type = pseudo_type_get(types, set->data[i]);

    assert(asm_type && "live pseudo has no known AsmType");

    touch_live_interval(intervals, set->data[i], *asm_type, instr_idx);
  }
}

VecLiveInterval build_live_intervals(struct InstrLiveness *lv, int n,
                                            VecPseudoType *types)
{
  VecLiveInterval intervals = {0};

  for (int i = 0; i < n; i++) {
    touch_pseudo_set(&intervals, types, &lv[i].use, i);
    touch_pseudo_set(&intervals, types, &lv[i].def, i);
    touch_pseudo_set(&intervals, types, &lv[i].live_before, i);
    touch_pseudo_set(&intervals, types, &lv[i].live_after, i);
  }

  return intervals;
}

int compare_live_intervals(const void *a, const void *b)
{
  const struct LiveInterval *ia = a;
  const struct LiveInterval *ib = b;

  if (ia->start != ib->start) {
    return ia->start - ib->start;
  }

  return ia->end - ib->end;
}

void sort_live_intervals(VecLiveInterval *intervals)
{
  qsort(intervals->data, intervals->len, sizeof(intervals->data[0]),
        compare_live_intervals);
}

void print_live_intervals(VecLiveInterval *intervals)
{
  printf("Live intervals:\n");

  for (int i = 0; i < intervals->len; i++) {
    printf("  %-20s [%d, %d]\n", intervals->data[i].pseudo,
           intervals->data[i].start, intervals->data[i].end);
  }
}

void free_live_intervals(VecLiveInterval *intervals)
{
  for (int i = 0; i < intervals->len; i++) {
    free(intervals->data[i].pseudo);
  }

  vec_free(intervals);

  intervals->capacity = 0;
  intervals->len = 0;
  intervals->data = NULL;
}

void init_regalloc_state(struct RegAllocState *state)
{
  memset(state, 0, sizeof(*state));
}

void free_regalloc_state(struct RegAllocState *state)
{
  struct AllocatedReg *curr = state->allocated_regs;

  while (curr) {
    struct AllocatedReg *next = curr->next;
    free(curr->pseudo);
    free(curr);
    curr = next;
  }

  state->allocated_regs = NULL;
}

int allocatable_reg_index(enum AsmRegister reg)
{
  for (int i = 0; i < NUM_ALLOCATABLE_INT_REGS; i++) {
    if (allocatable_int_regs[i] == reg) {
      return i;
    }
  }

  return -1;
}

void free_reg(struct RegAllocState *state, enum AsmRegister reg)
{
  int idx = allocatable_reg_index(reg);

  assert(idx >= 0 && "tried to free a non-allocatable register");

  state->reg_used[idx] = false;
}

void int_set_add(VecInt *set, int value)
{
  for (int i = 0; i < set->len; i++) {
    if (set->data[i] == value) {
      return;
    }
  }

  vec_insert(set, value);
}

int find_label_instr(struct AsmFunction *fn, char *label)
{
  for (int i = 0; i < fn->body.len; i++) {
    struct AsmInstr *instr = &fn->body.data[i];

    if (instr->kind == AsmInstr_LBL && strcmp(instr->as.lbl.name, label) == 0) {
      return i;
    }
  }

  return -1;
}

int block_after(struct CFG *cfg, int block_idx)
{
  int next = block_idx + 1;

  if (next >= cfg->block_count) {
    return -1;
  }

  return next;
}

bool pseudo_set_contains(VecPseudo *set, char *name)
{
  for (int i = 0; i < set->len; i++) {
    if (strcmp(set->data[i], name) == 0) {
      return true;
    }
  }

  return false;
}

bool pseudo_set_equal(VecPseudo *a, VecPseudo *b)
{
  if (a->len != b->len) {
    return false;
  }

  for (int i = 0; i < a->len; i++) {
    if (!pseudo_set_contains(b, a->data[i])) {
      return false;
    }
  }

  return true;
}

void pseudo_set_add(VecPseudo *set, char *name)
{
  if (!name) {
    return;
  }

  if (pseudo_set_contains(set, name)) {
    return;
  }

  vec_insert(set, name);
}

void pseudo_set_copy(VecPseudo *dst, VecPseudo *src)
{
  dst->len = 0;

  for (int i = 0; i < src->len; i++) {
    pseudo_set_add(dst, src->data[i]);
  }
}

void pseudo_set_union_into(VecPseudo *dst, VecPseudo *src)
{
  for (int i = 0; i < src->len; i++) {
    pseudo_set_add(dst, src->data[i]);
  }
}

void pseudo_set_remove(VecPseudo *set, char *name)
{
  for (int i = 0; i < set->len; i++) {
    if (strcmp(set->data[i], name) == 0) {
      set->data[i] = set->data[set->len - 1];
      set->len--;
      return;
    }
  }
}

void pseudo_set_subtract(VecPseudo *dst, VecPseudo *to_remove)
{
  for (int i = 0; i < to_remove->len; i++) {
    pseudo_set_remove(dst, to_remove->data[i]);
  }
}

void pseudo_set_free(VecPseudo *set)
{
  vec_free(set);
  set->capacity = 0;
  set->len = 0;
  set->data = NULL;
}

void pseudo_set_print(VecPseudo *set)
{
  printf("{");

  for (int i = 0; i < set->len; i++) {
    if (i > 0) {
      printf(", ");
    }

    printf("%s", set->data[i]);
  }

  printf("}");
}

int block_for_label(struct AsmFunction *fn, struct CFG *cfg, char *label)
{
  int instr_idx = find_label_instr(fn, label);

  assert(instr_idx >= 0 && "jump target label not found");

  return cfg->instr_to_block[instr_idx];
}

void dot_escape(FILE *out, const char *s)
{
  for (; *s; s++) {
    switch (*s) {
      case '\\':
        fputs("\\\\", out);
        break;
      case '"':
        fputs("\\\"", out);
        break;
      case '\n':
        fputs("\\n", out);
        break;
      default:
        fputc(*s, out);
        break;
    }
  }
}

const char *asm_instr_kind_name(enum AsmInstrKind kind)
{
  switch (kind) {
    case AsmInstr_PUSH:
      return "PUSH";
    case AsmInstr_POP:
      return "POP";
    case AsmInstr_MOV:
      return "MOV";
    case AsmInstr_BIN:
      return "BIN";
    case AsmInstr_RET:
      return "RET";
    case AsmInstr_CALL:
      return "CALL";
    case AsmInstr_JMP:
      return "JMP";
    case AsmInstr_LBL:
      return "LBL";
    case AsmInstr_CMP:
      return "CMP";
    case AsmInstr_JmpCC:
      return "JmpCC";
    case AsmInstr_SetCC:
      return "SetCC";
    case AsmInstr_LEA:
      return "LEA";
    case AsmInstr_UNARY:
      return "UNARY";
    case AsmInstr_CVT:
      return "CVT";
    case AsmInstr_REP_MOVSB:
      return "REP_MOVSB";
    default:
      assert(0);
  }

  assert(0 && "unhandled AsmInstrKind");
}

void dot_print_pseudo_set(FILE *out, VecPseudo *set)
{
  fputc('{', out);

  for (int i = 0; i < set->len; i++) {
    if (i > 0) {
      fputs(", ", out);
    }

    dot_escape(out, set->data[i]);
  }

  fputc('}', out);
}

struct CFG build_cfg(struct AsmFunction *fn)
{
  int n = fn->body.len;
  bool *leader = calloc(n, sizeof(*leader));

  if (!leader) {
    perror("calloc");
    exit(1);
  }

  if (n > 0) {
    leader[0] = true;
  }

  /*
   * First pass: mark obvious leaders.
   */
  for (int i = 0; i < n; i++) {
    struct AsmInstr *instr = &fn->body.data[i];

    if (instr->kind == AsmInstr_LBL) {
      leader[i] = true;
    }

    if (instr->kind == AsmInstr_JMP || instr->kind == AsmInstr_JmpCC ||
        instr->kind == AsmInstr_RET) {
      if (i + 1 < n) {
        leader[i + 1] = true;
      }
    }
  }

  /*
   * Second pass: jump targets are leaders too.
   */
  for (int i = 0; i < n; i++) {
    struct AsmInstr *instr = &fn->body.data[i];

    if (instr->kind == AsmInstr_JMP) {
      int target_idx = find_label_instr(fn, instr->as.jmp.target);
      assert(target_idx >= 0 && "JMP target not found");
      leader[target_idx] = true;
    } else if (instr->kind == AsmInstr_JmpCC) {
      int target_idx = find_label_instr(fn, instr->as.jmpcc.target);
      assert(target_idx >= 0 && "JmpCC target not found");
      leader[target_idx] = true;
    }
  }

  int block_count = 0;
  for (int i = 0; i < n; i++) {
    if (leader[i]) {
      block_count++;
    }
  }

  struct CFG cfg;
  cfg.blocks = calloc(block_count, sizeof(*cfg.blocks));
  cfg.instr_to_block = calloc(n, sizeof(*cfg.instr_to_block));
  cfg.block_count = block_count;

  if (!cfg.blocks || !cfg.instr_to_block) {
    perror("calloc");
    exit(1);
  }

  /*
   * Create blocks.
   */
  int b = 0;
  for (int i = 0; i < n; i++) {
    if (!leader[i]) {
      continue;
    }

    int start = i;
    int end = i;

    while (end + 1 < n && !leader[end + 1]) {
      end++;
    }

    cfg.blocks[b].start = start;
    cfg.blocks[b].end = end;

    for (int j = start; j <= end; j++) {
      cfg.instr_to_block[j] = b;
    }

    b++;
  }

  /*
   * Add successor edges.
   */
  for (int bi = 0; bi < cfg.block_count; bi++) {
    struct BasicBlock *block = &cfg.blocks[bi];
    struct AsmInstr *last = &fn->body.data[block->end];

    if (last->kind == AsmInstr_JMP) {
      int_set_add(&block->succs,
                  block_for_label(fn, &cfg, last->as.jmp.target));
    } else if (last->kind == AsmInstr_JmpCC) {
      int target = block_for_label(fn, &cfg, last->as.jmpcc.target);
      int fallthrough = block_after(&cfg, bi);

      int_set_add(&block->succs, target);

      if (fallthrough >= 0) {
        int_set_add(&block->succs, fallthrough);
      }
    } else if (last->kind == AsmInstr_RET) {
      /*
       * No successors.
       */
    } else {
      int fallthrough = block_after(&cfg, bi);

      if (fallthrough >= 0) {
        int_set_add(&block->succs, fallthrough);
      }
    }
  }

  free(leader);

  return cfg;
}

void compute_block_use_def(struct AsmFunction *fn,
                                  struct BasicBlock *block,
                                  struct InstrLiveness *lv)
{
  (void) fn;

  for (int i = block->start; i <= block->end; i++) {
    /*
     * use[B] contains values used before being defined inside B.
     */
    for (int j = 0; j < lv[i].use.len; j++) {
      char *name = lv[i].use.data[j];

      if (!pseudo_set_contains(&block->def, name)) {
        pseudo_set_add(&block->use, name);
      }
    }

    /*
     * def[B] contains values defined inside B.
     */
    for (int j = 0; j < lv[i].def.len; j++) {
      pseudo_set_add(&block->def, lv[i].def.data[j]);
    }
  }
}

void solve_block_liveness(struct CFG *cfg)
{
  bool changed = true;

  while (changed) {
    changed = false;

    /*
     * Backward order usually converges faster.
     */
    for (int bi = cfg->block_count - 1; bi >= 0; bi--) {
      struct BasicBlock *block = &cfg->blocks[bi];

      VecPseudo new_live_out = {0};
      VecPseudo new_live_in = {0};

      /*
       * live_out[B] = union(live_in[S]) for each successor S.
       */
      for (int i = 0; i < block->succs.len; i++) {
        int succ_idx = block->succs.data[i];
        pseudo_set_union_into(&new_live_out, &cfg->blocks[succ_idx].live_in);
      }

      /*
       * live_in[B] = use[B] union (live_out[B] - def[B])
       */
      pseudo_set_copy(&new_live_in, &new_live_out);
      pseudo_set_subtract(&new_live_in, &block->def);
      pseudo_set_union_into(&new_live_in, &block->use);

      if (!pseudo_set_equal(&block->live_out, &new_live_out) ||
          !pseudo_set_equal(&block->live_in, &new_live_in)) {
        changed = true;
      }

      pseudo_set_free(&block->live_out);
      pseudo_set_free(&block->live_in);

      block->live_out = new_live_out;
      block->live_in = new_live_in;
    }
  }
}

void dot_print_instr_summary(FILE *out, int idx, struct AsmInstr *instr)
{
  fprintf(out, "%04d: %s", idx, asm_instr_kind_name(instr->kind));

  switch (instr->kind) {
    case AsmInstr_LBL:
      fputs(" ", out);
      dot_escape(out, instr->as.lbl.name);
      break;

    case AsmInstr_JMP:
      fputs(" -> ", out);
      dot_escape(out, instr->as.jmp.target);
      break;

    case AsmInstr_JmpCC:
      fputs(" -> ", out);
      dot_escape(out, instr->as.jmpcc.target);
      break;

    case AsmInstr_CALL:
      fputs(" ", out);
      dot_escape(out, instr->as.call.target);
      break;

    default:
      break;
  }
}

void dump_cfg_dot(FILE *out, struct AsmFunction *fn, struct CFG *cfg,
                         struct InstrLiveness *lv)
{
  fprintf(out, "digraph \"cfg_%s\" {\n", fn->name);
  fprintf(out, "  graph [fontname=\"monospace\"];\n");
  fprintf(out, "  node [shape=box, fontname=\"monospace\"];\n");
  fprintf(out, "  edge [fontname=\"monospace\"];\n\n");

  for (int bi = 0; bi < cfg->block_count; bi++) {
    struct BasicBlock *block = &cfg->blocks[bi];

    fprintf(out, "  B%d [label=\"", bi);

    fprintf(out, "B%d [%d..%d]\\l", bi, block->start, block->end);

    fputs("use = ", out);
    dot_print_pseudo_set(out, &block->use);
    fputs("\\l", out);

    fputs("def = ", out);
    dot_print_pseudo_set(out, &block->def);
    fputs("\\l", out);

    fputs("live_in = ", out);
    dot_print_pseudo_set(out, &block->live_in);
    fputs("\\l", out);

    fputs("live_out = ", out);
    dot_print_pseudo_set(out, &block->live_out);
    fputs("\\l\\l", out);

    for (int i = block->start; i <= block->end; i++) {
      dot_print_instr_summary(out, i, &fn->body.data[i]);

      if (lv) {
        fputs("\\l    before=", out);
        dot_print_pseudo_set(out, &lv[i].live_before);
        fputs("\\l    after =", out);
        dot_print_pseudo_set(out, &lv[i].live_after);
      }

      fputs("\\l", out);
    }

    fprintf(out, "\"];\n");
  }

  fprintf(out, "\n");

  for (int bi = 0; bi < cfg->block_count; bi++) {
    struct BasicBlock *block = &cfg->blocks[bi];

    for (int i = 0; i < block->succs.len; i++) {
      int succ = block->succs.data[i];

      fprintf(out, "  B%d -> B%d", bi, succ);

      struct AsmInstr *last = &fn->body.data[block->end];

      if (last->kind == AsmInstr_JMP) {
        fprintf(out, " [label=\"jmp\"]");
      } else if (last->kind == AsmInstr_JmpCC) {
        if (succ == block_for_label(fn, cfg, last->as.jmpcc.target)) {
          fprintf(out, " [label=\"true\"]");
        } else {
          fprintf(out, " [label=\"false\"]");
        }
      } else {
        fprintf(out, " [label=\"fallthrough\"]");
      }

      fprintf(out, ";\n");
    }
  }

  fprintf(out, "}\n");
}

void free_cfg(struct CFG *cfg)
{
  for (int i = 0; i < cfg->block_count; i++) {
    vec_free(&cfg->blocks[i].succs);

    pseudo_set_free(&cfg->blocks[i].use);
    pseudo_set_free(&cfg->blocks[i].def);
    pseudo_set_free(&cfg->blocks[i].live_in);
    pseudo_set_free(&cfg->blocks[i].live_out);
  }

  free(cfg->blocks);
  free(cfg->instr_to_block);

  cfg->blocks = NULL;
  cfg->instr_to_block = NULL;
  cfg->block_count = 0;
}

void write_cfg_dot(struct AsmFunction *fn, struct InstrLiveness *lv,
                          const char *path)
{
  FILE *out;
  struct CFG cfg;

  cfg = build_cfg(fn);

  for (int bi = 0; bi < cfg.block_count; bi++) {
    compute_block_use_def(fn, &cfg.blocks[bi], lv);
  }

  solve_block_liveness(&cfg);

  out = fopen(path, "w");
  if (!out) {
    perror("fopen");
    free_cfg(&cfg);
    return;
  }

  dump_cfg_dot(out, fn, &cfg, lv);

  fclose(out);
  free_cfg(&cfg);
}

void release_dead_regs(struct RegAllocState *state,
                              VecPseudo *live_after)
{
  struct AllocatedReg **link = &state->allocated_regs;

  while (*link) {
    struct AllocatedReg *node = *link;

    if (is_tmp(node->pseudo) &&
        !pseudo_set_contains(live_after, node->pseudo)) {
      free_reg(state, node->reg);

      *link = node->next;

      free(node->pseudo);
      free(node);

      continue;
    }

    link = &node->next;
  }
}

bool alloc_reg(struct RegAllocState *state, enum AsmRegister *out_reg)
{
  for (int i = 0; i < NUM_ALLOCATABLE_INT_REGS; i++) {
    if (!state->reg_used[i]) {
      state->reg_used[i] = true;
      *out_reg = allocatable_int_regs[i];
      return true;
    }
  }

  return false;
}

void allocated_reg_append(struct RegAllocState *state, char *pseudo,
                          enum AsmRegister reg)
{
  struct AllocatedReg *node = malloc(sizeof(*node));
  if (!node) {
    perror("malloc");
    exit(1);
  }

  node->next = NULL;
  node->pseudo = strdup(pseudo);
  if (!node->pseudo) {
    perror("strdup");
    exit(1);
  }

  node->reg = reg;

  if (state->allocated_regs == NULL) {
    state->allocated_regs = node;
    return;
  }

  struct AllocatedReg *curr = state->allocated_regs;
  while (curr->next != NULL) {
    curr = curr->next;
  }

  curr->next = node;
}

bool allocated_reg_get(struct RegAllocState *state, char *pseudo,
                       enum AsmRegister *out_reg)
{
  for (struct AllocatedReg *curr = state->allocated_regs; curr;
       curr = curr->next) {
    if (strcmp(curr->pseudo, pseudo) == 0) {
      *out_reg = curr->reg;
      return true;
    }
  }

  return false;
}

void add_use_operand(VecPseudo *use, struct AsmOperand *op)
{
  if (op->kind == AsmOperand_PSEUDO) {
    pseudo_set_add(use, op->as.pseudo);
  }
}

void add_def_operand(VecPseudo *def, struct AsmOperand *op)
{
  if (op->kind == AsmOperand_PSEUDO) {
    pseudo_set_add(def, op->as.pseudo);
  }
}

void compute_instr_use_def(struct AsmInstr *instr, VecPseudo *use,
                                  VecPseudo *def)
{
  switch (instr->kind) {
    case AsmInstr_MOV: {
      add_use_operand(use, &instr->as.mov.src);
      add_def_operand(def, &instr->as.mov.dst);
      break;
    }

    case AsmInstr_BIN: {
      /*
       * x86-style two-address instruction:
       *
       *   rhs = rhs OP lhs
       *
       * So rhs is both read and written.
       */
      add_use_operand(use, &instr->as.binary.lhs);
      add_use_operand(use, &instr->as.binary.rhs);
      add_def_operand(def, &instr->as.binary.rhs);
      break;
    }

    case AsmInstr_CMP: {
      add_use_operand(use, &instr->as.cmp.lhs);
      add_use_operand(use, &instr->as.cmp.rhs);
      break;
    }

    case AsmInstr_CVT: {
      add_use_operand(use, &instr->as.cvt.src);
      add_def_operand(def, &instr->as.cvt.dst);
      break;
    }

    case AsmInstr_SetCC: {
      add_def_operand(def, &instr->as.setcc.op);
      break;
    }

    case AsmInstr_UNARY: {
      /*
       * neg x / not x:
       *
       *   x = OP x
       */
      add_use_operand(use, &instr->as.unary.op);
      add_def_operand(def, &instr->as.unary.op);
      break;
    }

    case AsmInstr_PUSH: {
      add_use_operand(use, &instr->as.push.op);
      break;
    }

    case AsmInstr_POP: {
      add_def_operand(def, &instr->as.pop.op);
      break;
    }

    case AsmInstr_LEA: {
      /*
       * Important:
       *
       * LEA computes an address. The source is an address expression,
       * not a value read.
       *
       * So for now, only the destination is a def.
       */
      add_def_operand(def, &instr->as.lea.dst);
      break;
    }

    case AsmInstr_CALL:
    case AsmInstr_RET:
    case AsmInstr_JMP:
    case AsmInstr_JmpCC:
    case AsmInstr_LBL:
    case AsmInstr_REP_MOVSB: {
      break;
    }

    default:
      assert(0 && "Unhandled instruction in compute_instr_use_def");
  }
}

void compute_instr_liveness_from_blocks(struct AsmFunction *fn,
                                               struct CFG *cfg,
                                               struct InstrLiveness *lv)
{
  (void) fn;

  for (int bi = 0; bi < cfg->block_count; bi++) {
    struct BasicBlock *block = &cfg->blocks[bi];

    VecPseudo live = {0};
    pseudo_set_copy(&live, &block->live_out);

    for (int i = block->end; i >= block->start; i--) {
      pseudo_set_copy(&lv[i].live_after, &live);

      VecPseudo before = {0};

      pseudo_set_copy(&before, &live);
      pseudo_set_subtract(&before, &lv[i].def);
      pseudo_set_union_into(&before, &lv[i].use);

      pseudo_set_copy(&lv[i].live_before, &before);

      pseudo_set_free(&live);
      live = before;
    }

    pseudo_set_free(&live);
  }
}

struct InstrLiveness *compute_liveness_cfg(struct AsmFunction *fn)
{
  int n = fn->body.len;

  struct InstrLiveness *lv = calloc(n, sizeof(*lv));
  if (!lv) {
    perror("calloc");
    exit(1);
  }

  /*
   * Per-instruction use/def.
   */
  for (int i = 0; i < n; i++) {
    compute_instr_use_def(&fn->body.data[i], &lv[i].use, &lv[i].def);
  }

  struct CFG cfg = build_cfg(fn);

  /*
   * Per-block use/def.
   */
  for (int bi = 0; bi < cfg.block_count; bi++) {
    compute_block_use_def(fn, &cfg.blocks[bi], lv);
  }

  /*
   * Fixed-point block liveness.
   */
  solve_block_liveness(&cfg);

  /*
   * Convert back to per-instruction liveness.
   */
  compute_instr_liveness_from_blocks(fn, &cfg, lv);

  free_cfg(&cfg);

  return lv;
}

void free_liveness(struct InstrLiveness *lv, int n)
{
  for (int i = 0; i < n; i++) {
    pseudo_set_free(&lv[i].use);
    pseudo_set_free(&lv[i].def);
    pseudo_set_free(&lv[i].live_before);
    pseudo_set_free(&lv[i].live_after);
  }

  free(lv);
}

void print_liveness(struct AsmFunction *fn, struct InstrLiveness *lv)
{
  printf("Liveness for function %s:\n", fn->name);

  for (int i = 0; i < fn->body.len; i++) {
    printf("%04d: use=", i);
    pseudo_set_print(&lv[i].use);

    printf(" def=");
    pseudo_set_print(&lv[i].def);

    printf(" live_before=");
    pseudo_set_print(&lv[i].live_before);

    printf(" live_after=");
    pseudo_set_print(&lv[i].live_after);

    printf("\n");
  }
}

struct PseudoHome *pseudo_home_get(VecPseudoHome *homes, char *pseudo)
{
  for (int i = 0; i < homes->len; i++) {
    if (strcmp(homes->data[i].pseudo, pseudo) == 0) {
      return &homes->data[i];
    }
  }

  return NULL;
}

void pseudo_home_add_reg(VecPseudoHome *homes, char *pseudo,
                                enum AsmRegister reg)
{
  struct PseudoHome home;

  assert(pseudo_home_get(homes, pseudo) == NULL);

  home.pseudo = strdup(pseudo);
  if (!home.pseudo) {
    perror("strdup");
    exit(1);
  }

  home.kind = PSEUDO_HOME_REG;
  home.as.reg = reg;

  vec_insert(homes, home);
}

void pseudo_home_add_stack(VecPseudoHome *homes, char *pseudo,
                                  int stack_offset)
{
  struct PseudoHome home;

  assert(pseudo_home_get(homes, pseudo) == NULL);

  home.pseudo = strdup(pseudo);
  if (!home.pseudo) {
    perror("strdup");
    exit(1);
  }

  home.kind = PSEUDO_HOME_STACK;
  home.as.stack_offset = stack_offset;

  vec_insert(homes, home);
}

void free_pseudo_homes(VecPseudoHome *homes)
{
  for (int i = 0; i < homes->len; i++) {
    free(homes->data[i].pseudo);
  }

  vec_free(homes);

  homes->capacity = 0;
  homes->len = 0;
  homes->data = NULL;
}

int compare_active_intervals_by_end(const void *a, const void *b)
{
  const struct ActiveInterval *ia = a;
  const struct ActiveInterval *ib = b;

  return ia->end - ib->end;
}

void sort_active_intervals(VecActiveInterval *active)
{
  qsort(active->data, active->len, sizeof(active->data[0]),
        compare_active_intervals_by_end);
}

void active_remove_at(VecActiveInterval *active, int idx)
{
  assert(idx >= 0 && idx < active->len);

  for (int i = idx; i + 1 < active->len; i++) {
    active->data[i] = active->data[i + 1];
  }

  active->len--;
}

bool fixed_reg_is_used(VecActiveInterval *active, enum AsmRegister reg)
{
  for (int i = 0; i < active->len; i++) {
    if (active->data[i].reg == reg) {
      return true;
    }
  }

  return false;
}

bool fixed_find_free_reg(VecActiveInterval *active,
                                enum AsmRegister *out_reg)
{
  for (int i = 0; i < NUM_ALLOCATABLE_INT_REGS; i++) {
    enum AsmRegister reg = allocatable_int_regs[i];

    if (!fixed_reg_is_used(active, reg)) {
      *out_reg = reg;
      return true;
    }
  }

  return false;
}

void expire_old_intervals(VecActiveInterval *active, int start)
{
  int i = 0;

  while (i < active->len) {
    if (active->data[i].end < start) {
      active_remove_at(active, i);
      continue;
    }

    i++;
  }
}

VecPseudoHome allocate_pseudo_homes(VecLiveInterval *intervals,
                                           struct Map *map,
                                           int *used_stack_bytes,
                                           struct AsmFunction *fn)
{
  (void) fn;

  VecPseudoHome homes = {0};
  VecActiveInterval active = {0};

  /*
   * Intervals must already be sorted by start.
   */
  for (int i = 0; i < intervals->len; i++) {
    struct LiveInterval *interval = &intervals->data[i];
    enum AsmRegister reg;

    expire_old_intervals(&active, interval->start);
    sort_active_intervals(&active);

    /*
     * Later we can look up the pseudo's real AsmType here.
     * For now, we assume your interval list only contains pseudos
     * that came from AsmOperands and are legal allocation candidates.
     */

    if (fixed_find_free_reg(&active, &reg)) {
      struct ActiveInterval active_interval;

      pseudo_home_add_reg(&homes, interval->pseudo, reg);

      active_interval.pseudo = interval->pseudo;
      active_interval.end = interval->end;
      active_interval.reg = reg;

      vec_insert(&active, active_interval);
      continue;
    }

    /*
     * No register available.
     *
     * For the first fixed-home version, spill the new interval.
     * This is simple and safe, though not optimal.
     */
    {
      int stack_offset;

      stack_offset = get_offset(map, interval->pseudo, interval->asm_type,
                                used_stack_bytes);

      pseudo_home_add_stack(&homes, interval->pseudo, stack_offset);
    }
  }

  vec_free(&active);

  return homes;
}

int interference_node_index(struct InterferenceGraph *graph,
                                   char *pseudo)
{
  for (int i = 0; i < graph->nodes.len; i++) {
    if (strcmp(graph->nodes.data[i].pseudo, pseudo) == 0) {
      return i;
    }
  }

  return -1;
}

int interference_add_node(struct InterferenceGraph *graph, char *pseudo)
{
  int idx = interference_node_index(graph, pseudo);

  if (idx >= 0) {
    return idx;
  }

  struct InterferenceNode node;

  node.pseudo = strdup(pseudo);
  if (!node.pseudo) {
    perror("strdup");
    exit(1);
  }

  node.is_precolored = false;
  node.precolored_reg = AX; /* unused for non-precolored nodes */
  node.neighbors = (VecInt){0};

  vec_insert(&graph->nodes, node);

  return graph->nodes.len - 1;
}

bool int_vec_contains(VecInt *vec, int value)
{
  for (int i = 0; i < vec->len; i++) {
    if (vec->data[i] == value) {
      return true;
    }
  }

  return false;
}

char *precolored_reg_name(enum AsmRegister reg)
{
  switch (reg) {
    case AX:
      return "ax";
    case BX:
      return "bx";
    case DX:
      return "dx";
    case CX:
      return "cx";
    case SI:
      return "si";
    case DI:
      return "di";
    case R8:
      return "r8";
    case R9:
      return "r9";
    case R12:
      return "r12";
    case R13:
      return "r13";
    case R14:
      return "r14";
    case R15:
      return "r15";
    case BP:
      return "bp";
    case SP:
      return "sp";
    case XMM0:
      return "xmm0";
    case XMM1:
      return "xmm1";
    case XMM2:
      return "xmm2";
    case XMM3:
      return "xmm3";
    case XMM4:
      return "xmm4";
    case XMM5:
      return "xmm5";
    case XMM6:
      return "xmm6";
    case XMM7:
      return "xmm7";
    case XMM8:
      return "xmm8";
    case XMM9:
      return "xmm9";
    case XMM10:
      return "xmm10";
    case XMM11:
      return "xmm11";
    case XMM12:
      return "xmm12";
    case XMM13:
      return "xmm13";
    case XMM14:
      return "xmm14";
    case XMM15:
      return "xmm15";
    default:
      assert(0 && "unhandled precolored register");
  }
}

int interference_add_precolored_reg(struct InterferenceGraph *graph,
                                           enum AsmRegister reg)
{
  char *name = precolored_reg_name(reg);
  int idx = interference_node_index(graph, name);

  if (idx >= 0) {
    graph->nodes.data[idx].is_precolored = true;
    graph->nodes.data[idx].precolored_reg = reg;
    return idx;
  }

  struct InterferenceNode node;

  node.pseudo = strdup(name);
  if (!node.pseudo) {
    perror("strdup");
    exit(1);
  }

  node.is_precolored = true;
  node.precolored_reg = reg;
  node.neighbors = (VecInt){0};

  vec_insert(&graph->nodes, node);

  return graph->nodes.len - 1;
}

void add_precolored_register_nodes(struct InterferenceGraph *graph)
{
  for (int i = 0; i < NUM_ALLOCATABLE_INT_REGS; i++) {
    interference_add_precolored_reg(graph, allocatable_int_regs[i]);
  }

  for (int i = 0; i < NUM_ALLOCATABLE_SSE_REGS; i++) {
    interference_add_precolored_reg(graph, allocatable_sse_regs[i]);
  }
}

void interference_add_edge_by_index(struct InterferenceGraph *graph,
                                           int ai, int bi)
{
  if (ai == bi) {
    return;
  }

  if (!int_vec_contains(&graph->nodes.data[ai].neighbors, bi)) {
    vec_insert(&graph->nodes.data[ai].neighbors, bi);
  }

  if (!int_vec_contains(&graph->nodes.data[bi].neighbors, ai)) {
    vec_insert(&graph->nodes.data[bi].neighbors, ai);
  }
}

void interference_add_edge(struct InterferenceGraph *graph, char *a,
                                  char *b)
{
  int ai;
  int bi;

  if (strcmp(a, b) == 0) {
    return;
  }

  ai = interference_add_node(graph, a);
  bi = interference_add_node(graph, b);

  interference_add_edge_by_index(graph, ai, bi);
}

#define NUM_CALLEE_SAVED_INT_REGS \
  ((int) (sizeof(callee_saved_int_regs) / sizeof(callee_saved_int_regs[0])))

void interference_add_nodes_from_set(struct InterferenceGraph *graph,
                                            VecPseudo *set)
{
  for (int i = 0; i < set->len; i++) {
    interference_add_node(graph, set->data[i]);
  }
}

char *mov_pseudo_src(struct AsmInstr *instr)
{
  if (instr->kind != AsmInstr_MOV) {
    return NULL;
  }

  if (instr->as.mov.src.kind != AsmOperand_PSEUDO) {
    return NULL;
  }

  if (instr->as.mov.dst.kind != AsmOperand_PSEUDO) {
    return NULL;
  }

  return instr->as.mov.src.as.pseudo;
}

void add_interferences_for_instr(struct InterferenceGraph *graph,
                                        struct AsmInstr *instr,
                                        struct InstrLiveness *lv)
{
  VecPseudo live = {0};
  char *move_src;

  /*
   * Make sure all mentioned pseudos become graph nodes, even if they
   * end up having no interference edges.
   */
  interference_add_nodes_from_set(graph, &lv->use);
  interference_add_nodes_from_set(graph, &lv->def);
  interference_add_nodes_from_set(graph, &lv->live_before);
  interference_add_nodes_from_set(graph, &lv->live_after);

  pseudo_set_copy(&live, &lv->live_after);

  /*
   * Move special case:
   *
   *   mov src, dst
   *
   * Do not add dst -- src here, because later coalescing may want to
   * merge src and dst and delete the move.
   */
  move_src = mov_pseudo_src(instr);
  if (move_src) {
    pseudo_set_remove(&live, move_src);
  }

  /*
   * Every def interferes with everything live after the instruction.
   */
  for (int i = 0; i < lv->def.len; i++) {
    char *defined = lv->def.data[i];

    for (int j = 0; j < live.len; j++) {
      char *live_pseudo = live.data[j];

      interference_add_edge(graph, defined, live_pseudo);
    }
  }

  pseudo_set_free(&live);
}

bool is_abi_int_arg_reg(enum AsmRegister reg)
{
  return reg == DI || reg == SI || reg == DX || reg == CX || reg == R8 ||
         reg == R9;
}

bool is_abi_sse_arg_reg(enum AsmRegister reg)
{
  return reg >= XMM0 && reg <= XMM7;
}

bool is_abi_arg_reg(enum AsmRegister reg)
{
  return is_abi_int_arg_reg(reg) || is_abi_sse_arg_reg(reg);
}

bool same_reg_class(enum AsmRegister a, enum AsmRegister b)
{
  return (is_abi_sse_arg_reg(a) && is_abi_sse_arg_reg(b)) ||
         (is_abi_int_arg_reg(a) && is_abi_int_arg_reg(b));
}

void add_abi_param_copy_interference(struct InterferenceGraph *graph,
                                            struct AsmFunction *fn)
{
  VecAbiRegParamMove moves = {0};

  /*
   * Collect initial ABI register-param imports:
   *
   *   mov %xmm0, pseudo.a0
   *   mov %xmm1, pseudo.a1
   *   ...
   *
   * Stop at the first CALL so we do not mistake call-return moves for
   * function-entry parameter moves.
   */
  for (int i = 0; i < fn->body.len; i++) {
    struct AsmInstr *instr = &fn->body.data[i];

    if (instr->kind == AsmInstr_CALL) {
      break;
    }

    if (instr->kind != AsmInstr_MOV) {
      continue;
    }

    if (instr->as.mov.src.kind != AsmOperand_REG) {
      continue;
    }

    if (instr->as.mov.dst.kind != AsmOperand_PSEUDO) {
      continue;
    }

    if (!is_abi_arg_reg(instr->as.mov.src.as.reg)) {
      continue;
    }

    vec_insert(&moves, ((struct AbiRegParamMove){
                           .src_reg = instr->as.mov.src.as.reg,
                           .dst_pseudo = instr->as.mov.dst.as.pseudo,
                           .asm_type = instr->asm_type,
                       }));
  }

  /*
   * These entry copies are sequential, not parallel.
   *
   * For:
   *
   *   mov %xmm0, a0
   *   mov %xmm1, a1
   *
   * `a0` must not be colored as `%xmm1`, because that would clobber
   * the still-unread incoming value for `a1`.
   */
  for (int i = 0; i < moves.len; i++) {
    int pseudo_idx = interference_add_node(graph, moves.data[i].dst_pseudo);

    for (int j = i + 1; j < moves.len; j++) {
      int reg_idx;

      if (!same_reg_class(moves.data[i].src_reg, moves.data[j].src_reg)) {
        continue;
      }

      reg_idx = interference_add_precolored_reg(graph, moves.data[j].src_reg);

      interference_add_edge_by_index(graph, pseudo_idx, reg_idx);
    }
  }

  vec_free(&moves);
}

void print_interference_graph(struct InterferenceGraph *graph)
{
  printf("Interference graph:\n");

  for (int i = 0; i < graph->nodes.len; i++) {
    struct InterferenceNode *node = &graph->nodes.data[i];

    if (node->is_precolored) {
      printf("  %s [precolored]:", node->pseudo);
    } else {
      printf("  %s:", node->pseudo);
    }

    for (int j = 0; j < node->neighbors.len; j++) {
      int neighbor_idx = node->neighbors.data[j];
      printf(" %s", graph->nodes.data[neighbor_idx].pseudo);
    }

    printf("\n");
  }
}

void dump_interference_graph_dot(FILE *out,
                                        struct InterferenceGraph *graph)
{
  fprintf(out, "graph interference {\n");
  fprintf(out, "  graph [fontname=\"monospace\"];\n");
  fprintf(out, "  node [shape=ellipse, fontname=\"monospace\"];\n");
  fprintf(out, "  edge [fontname=\"monospace\"];\n\n");

  /*
   * Print nodes first, including isolated nodes.
   */
  for (int i = 0; i < graph->nodes.len; i++) {
    fprintf(out, "  \"");
    dot_escape(out, graph->nodes.data[i].pseudo);
    fprintf(out, "\";\n");
  }

  fprintf(out, "\n");

  /*
   * Print each undirected edge once.
   */
  for (int i = 0; i < graph->nodes.len; i++) {
    struct InterferenceNode *node = &graph->nodes.data[i];

    for (int j = 0; j < node->neighbors.len; j++) {
      int neighbor_idx = node->neighbors.data[j];

      if (i > neighbor_idx) {
        continue;
      }

      fprintf(out, "  \"");
      dot_escape(out, node->pseudo);
      fprintf(out, "\" -- \"");
      dot_escape(out, graph->nodes.data[neighbor_idx].pseudo);
      fprintf(out, "\";\n");
    }
  }

  fprintf(out, "}\n");
}

void write_interference_graph_dot(struct InterferenceGraph *graph,
                                         const char *path)
{
  FILE *out = fopen(path, "w");

  if (!out) {
    perror("fopen");
    return;
  }

  dump_interference_graph_dot(out, graph);

  fclose(out);
}

void free_interference_graph(struct InterferenceGraph *graph)
{
  for (int i = 0; i < graph->nodes.len; i++) {
    free(graph->nodes.data[i].pseudo);
    vec_free(&graph->nodes.data[i].neighbors);
  }

  vec_free(&graph->nodes);

  graph->nodes.capacity = 0;
  graph->nodes.len = 0;
  graph->nodes.data = NULL;
}

bool pseudo_is_global_constant(char *pseudo)
{
  for (int k = 0; k < global_constants.len; k++) {
    if (strcmp(pseudo, global_constants.data[k].name) == 0) {
      return true;
    }
  }

  return false;
}

bool convert_global_constant_operand(struct AsmOperand *op)
{
  if (op->kind != AsmOperand_PSEUDO) {
    return false;
  }

  if (!pseudo_is_global_constant(op->as.pseudo)) {
    return false;
  }

  op->kind = AsmOperand_DATA;
  op->as.data = strdup(op->as.pseudo);
  if (!op->as.data) {
    perror("strdup");
    exit(1);
  }

  return true;
}

void regalloc_addr_op_from_homes(struct AsmOperand *op,
                                        VecPseudoHome *homes, struct Map *map,
                                        int *used_stack_bytes)
{
  struct PseudoHome *home;

  if (op->kind != AsmOperand_PSEUDO) {
    return;
  }

  if (convert_global_constant_operand(op)) {
    return;
  }

  home = pseudo_home_get(homes, op->as.pseudo);

  if (!home) {
    op->kind = AsmOperand_STACK;
    op->as.stack_offset =
        get_offset(map, op->as.pseudo, op->asm_type, used_stack_bytes);
    return;
  }

  /*
   * LEA needs an addressable object. Stack homes are addressable;
   * register homes are not.
   */
  if (home->kind == PSEUDO_HOME_STACK) {
    op->kind = AsmOperand_STACK;
    op->as.stack_offset = home->as.stack_offset;
    return;
  }

  assert(0 && "LEA source pseudo was allocated to a register");
}

void regalloc_op_from_homes(struct AsmOperand *op, VecPseudoHome *homes,
                                   struct Map *map, int *used_stack_bytes)
{
  struct PseudoHome *home;

  if (op->kind != AsmOperand_PSEUDO) {
    return;
  }

  if (convert_global_constant_operand(op)) {
    return;
  }

  home = pseudo_home_get(homes, op->as.pseudo);

  /*
   * Conservative fallback: pseudo did not appear in allocation.
   * Put it on the stack.
   */
  if (!home) {
    op->kind = AsmOperand_STACK;
    op->as.stack_offset =
        get_offset(map, op->as.pseudo, op->asm_type, used_stack_bytes);
    return;
  }

  if (home->kind == PSEUDO_HOME_REG) {
    op->kind = AsmOperand_REG;
    op->as.reg = home->as.reg;
    return;
  }

  op->kind = AsmOperand_STACK;
  op->as.stack_offset = home->as.stack_offset;
}

struct AsmInstr *regalloc_instr_from_homes(struct AsmInstr *instr,
                                                  VecPseudoHome *homes,
                                                  struct Map *map,
                                                  int *used_stack_bytes)
{
  switch (instr->kind) {
    case AsmInstr_MOV: {
      regalloc_op_from_homes(&instr->as.mov.src, homes, map, used_stack_bytes);
      regalloc_op_from_homes(&instr->as.mov.dst, homes, map, used_stack_bytes);
      break;
    }

    case AsmInstr_BIN: {
      regalloc_op_from_homes(&instr->as.binary.lhs, homes, map,
                             used_stack_bytes);
      regalloc_op_from_homes(&instr->as.binary.rhs, homes, map,
                             used_stack_bytes);
      break;
    }

    case AsmInstr_CMP: {
      regalloc_op_from_homes(&instr->as.cmp.lhs, homes, map, used_stack_bytes);
      regalloc_op_from_homes(&instr->as.cmp.rhs, homes, map, used_stack_bytes);
      break;
    }

    case AsmInstr_CVT: {
      regalloc_op_from_homes(&instr->as.cvt.src, homes, map, used_stack_bytes);
      regalloc_op_from_homes(&instr->as.cvt.dst, homes, map, used_stack_bytes);
      break;
    }

    case AsmInstr_SetCC: {
      regalloc_op_from_homes(&instr->as.setcc.op, homes, map, used_stack_bytes);
      break;
    }

    case AsmInstr_UNARY: {
      regalloc_op_from_homes(&instr->as.unary.op, homes, map, used_stack_bytes);
      break;
    }

    case AsmInstr_PUSH: {
      regalloc_op_from_homes(&instr->as.push.op, homes, map, used_stack_bytes);
      break;
    }

    case AsmInstr_POP: {
      regalloc_op_from_homes(&instr->as.pop.op, homes, map, used_stack_bytes);
      break;
    }

    case AsmInstr_LEA: {
      regalloc_addr_op_from_homes(&instr->as.lea.src, homes, map,
                                  used_stack_bytes);
      regalloc_op_from_homes(&instr->as.lea.dst, homes, map, used_stack_bytes);
      break;
    }

    case AsmInstr_CALL:
    case AsmInstr_RET:
    case AsmInstr_JMP:
    case AsmInstr_JmpCC:
    case AsmInstr_LBL:
    case AsmInstr_REP_MOVSB: {
      break;
    }

    default:
      assert(0 && "Unhandled instruction in regalloc_instr_from_homes");
  }

  return instr;
}

bool reg_is_sse(enum AsmRegister reg)
{
  return reg >= XMM0 && reg <= XMM15;
}

enum RegClass reg_class_of_reg(enum AsmRegister reg)
{
  return reg_is_sse(reg) ? REGCLASS_SSE : REGCLASS_INT;
}

int allocatable_reg_count(enum RegClass cls)
{
  switch (cls) {
    case REGCLASS_INT:
      return NUM_ALLOCATABLE_INT_REGS;
    case REGCLASS_SSE:
      return NUM_ALLOCATABLE_SSE_REGS;
    default:
      return 0;
  }
}

enum AsmRegister allocatable_reg_at(enum RegClass cls, int i)
{
  switch (cls) {
    case REGCLASS_INT:
      return allocatable_int_regs[i];
    case REGCLASS_SSE:
      return allocatable_sse_regs[i];
    default:
      assert(0 && "no registers for this class");
  }
}

int allocatable_reg_index_in_class(enum RegClass cls,
                                          enum AsmRegister reg)
{
  int n = allocatable_reg_count(cls);

  for (int i = 0; i < n; i++) {
    if (allocatable_reg_at(cls, i) == reg) {
      return i;
    }
  }

  return -1;
}

struct SpillCost *spill_cost_get(VecSpillCost *costs, char *pseudo)
{
  for (int i = 0; i < costs->len; i++) {
    if (strcmp(costs->data[i].pseudo, pseudo) == 0) {
      return &costs->data[i];
    }
  }

  return NULL;
}

void spill_cost_add(VecSpillCost *costs, char *pseudo, double amount)
{
  struct SpillCost *cost = spill_cost_get(costs, pseudo);

  if (cost) {
    cost->cost += amount;
    return;
  }

  struct SpillCost new_cost;

  new_cost.pseudo = strdup(pseudo);
  if (!new_cost.pseudo) {
    perror("strdup");
    exit(1);
  }

  new_cost.cost = amount;

  vec_insert(costs, new_cost);
}

void free_spill_costs(VecSpillCost *costs)
{
  for (int i = 0; i < costs->len; i++) {
    free(costs->data[i].pseudo);
  }

  vec_free(costs);

  costs->capacity = 0;
  costs->len = 0;
  costs->data = NULL;
}

VecSpillCost compute_spill_costs(struct InstrLiveness *lv,
                                        int instr_count)
{
  VecSpillCost costs = {0};

  for (int i = 0; i < instr_count; i++) {
    /*
     * Uses are expensive: spilling causes reloads.
     */
    for (int j = 0; j < lv[i].use.len; j++) {
      spill_cost_add(&costs, lv[i].use.data[j], 10.0);
    }

    /*
     * Defs are also expensive: spilling causes stores.
     */
    for (int j = 0; j < lv[i].def.len; j++) {
      spill_cost_add(&costs, lv[i].def.data[j], 5.0);
    }
  }

  return costs;
}

double spill_cost_of(VecSpillCost *costs, char *pseudo)
{
  struct SpillCost *cost = spill_cost_get(costs, pseudo);

  if (!cost) {
    return 1.0;
  }

  return cost->cost;
}

char *fresh_spill_tmp(void)
{
  return mkstr("spilltmp.%d", mktmp());
}

bool is_spill_tmp_name(char *pseudo)
{
  return strncmp(pseudo, "spilltmp.", strlen("spilltmp.")) == 0;
}

enum RegClass asm_type_reg_class(struct AsmType type)
{
  switch (type.kind) {
    case AsmType_BYTE:
    case AsmType_WORD:
    case AsmType_LONGWORD:
    case AsmType_QUADWORD:
      return REGCLASS_INT;

    case AsmType_FLOAT:
    case AsmType_DOUBLE:
      return REGCLASS_SSE;

    case AsmType_BYTE_ARRAY:
      return REGCLASS_NONE;
    default:
      assert(0 && "unhandled asm type");
  }
}

enum RegClass graph_node_reg_class(struct InterferenceGraph *graph,
                                          VecPseudoType *types, int idx)
{
  struct InterferenceNode *node = &graph->nodes.data[idx];

  if (node->is_precolored) {
    return reg_class_of_reg(node->precolored_reg);
  }

  struct AsmType *asm_type = pseudo_type_get(types, node->pseudo);
  assert(asm_type && "node has no known AsmType");

  return asm_type_reg_class(*asm_type);
}

int graph_degree_after_removal(struct InterferenceGraph *graph,
                                      VecPseudoType *types, bool *removed,
                                      int node_idx)
{
  int degree = 0;
  enum RegClass cls = graph_node_reg_class(graph, types, node_idx);
  struct InterferenceNode *node = &graph->nodes.data[node_idx];

  for (int i = 0; i < node->neighbors.len; i++) {
    int neighbor_idx = node->neighbors.data[i];

    if (removed[neighbor_idx]) {
      continue;
    }

    if (graph_node_reg_class(graph, types, neighbor_idx) != cls) {
      continue;
    }

    degree++;
  }

  return degree;
}

int pick_low_degree_node(struct InterferenceGraph *graph,
                                VecPseudoType *types, bool *removed,
                                VecSpillCost *spill_costs)
{
  int best_idx = -1;
  double best_score = 0.0;

  for (int i = 0; i < graph->nodes.len; i++) {
    int degree;
    int k;
    char *pseudo;
    double cost;
    double score;
    enum RegClass cls;

    if (removed[i]) {
      continue;
    }

    if (graph->nodes.data[i].is_precolored) {
      continue;
    }

    cls = graph_node_reg_class(graph, types, i);
    k = allocatable_reg_count(cls);

    if (k == 0) {
      continue;
    }

    degree = graph_degree_after_removal(graph, types, removed, i);

    if (degree >= k) {
      continue;
    }

    pseudo = graph->nodes.data[i].pseudo;

    cost = spill_cost_of(spill_costs, pseudo);
    score = cost / (double) (degree > 0 ? degree : 1);

    if (best_idx < 0 || score < best_score) {
      best_idx = i;
      best_score = score;
    }
  }

  return best_idx;
}

double spill_score_for_node(struct InterferenceGraph *graph,
                                   VecPseudoType *types, bool *removed,
                                   int node_idx, VecSpillCost *spill_costs)
{
  char *pseudo = graph->nodes.data[node_idx].pseudo;
  int degree = graph_degree_after_removal(graph, types, removed, node_idx);
  double cost = spill_cost_of(spill_costs, pseudo);

  return cost / (double) (degree > 0 ? degree : 1);
}

int pick_spill_candidate(struct InterferenceGraph *graph,
                                VecPseudoType *types, bool *removed,
                                VecSpillCost *spill_costs)
{
  int best_idx = -1;
  double best_score = 0.0;

  for (int i = 0; i < graph->nodes.len; i++) {
    char *pseudo;
    int degree;
    double cost;
    double score;

    if (removed[i]) {
      continue;
    }

    if (graph->nodes.data[i].is_precolored) {
      continue;
    }

    pseudo = graph->nodes.data[i].pseudo;

    if (is_spill_tmp_name(pseudo)) {
      continue;
    }

    degree = graph_degree_after_removal(graph, types, removed, i);
    cost = spill_cost_of(spill_costs, pseudo);
    score = cost / (double) (degree > 0 ? degree : 1);

    if (best_idx < 0 || score < best_score) {
      best_idx = i;
      best_score = score;
    }
  }

  if (best_idx < 0) {
    for (int i = 0; i < graph->nodes.len; i++) {
      if (!removed[i] && !graph->nodes.data[i].is_precolored) {
        return i;
      }
    }

    return -1;
  }

  return best_idx;
}

bool choose_color_for_node(struct InterferenceGraph *graph,
                                  VecPseudoType *types, VecPseudoHome *homes,
                                  int node_idx, enum AsmRegister *out_reg)
{
  enum RegClass cls = graph_node_reg_class(graph, types, node_idx);
  int reg_count = allocatable_reg_count(cls);
  bool used[32] = {0};

  struct InterferenceNode *node = &graph->nodes.data[node_idx];

  for (int i = 0; i < node->neighbors.len; i++) {
    int neighbor_idx = node->neighbors.data[i];

    if (graph_node_reg_class(graph, types, neighbor_idx) != cls) {
      continue;
    }

    struct InterferenceNode *neighbor = &graph->nodes.data[neighbor_idx];

    if (neighbor->is_precolored) {
      int reg_idx =
          allocatable_reg_index_in_class(cls, neighbor->precolored_reg);

      if (reg_idx >= 0) {
        used[reg_idx] = true;
      }

      continue;
    }

    struct PseudoHome *neighbor_home = pseudo_home_get(homes, neighbor->pseudo);

    if (!neighbor_home || neighbor_home->kind != PSEUDO_HOME_REG) {
      continue;
    }

    int reg_idx = allocatable_reg_index_in_class(cls, neighbor_home->as.reg);
    assert(reg_idx >= 0 && "colored with register from wrong class");

    used[reg_idx] = true;
  }

  for (int i = 0; i < reg_count; i++) {
    if (!used[i]) {
      *out_reg = allocatable_reg_at(cls, i);
      return true;
    }
  }

  return false;
}

void assign_stack_home(VecPseudoHome *homes, VecPseudoType *types,
                              struct Map *map, int *used_stack_bytes,
                              char *pseudo)
{
  struct AsmType *asm_type;
  int stack_offset;

  if (pseudo_home_get(homes, pseudo)) {
    return;
  }

  asm_type = pseudo_type_get(types, pseudo);
  assert(asm_type && "spilled pseudo has no known AsmType");

  stack_offset = get_offset(map, pseudo, *asm_type, used_stack_bytes);

  pseudo_home_add_stack(homes, pseudo, stack_offset);
}

VecPseudoHome color_interference_graph(
    struct InterferenceGraph *graph, VecPseudoType *types,
    VecPseudo *force_stack, VecPseudo *out_spilled, VecSpillCost *spill_costs,
    struct Map *map, int *used_stack_bytes)
{
  VecPseudoHome homes = {0};
  VecSelectStack select_stack = {0};

  int n = graph->nodes.len;
  int remaining = n;

  bool *removed = calloc(n, sizeof(*removed));
  if (!removed) {
    perror("calloc");
    exit(1);
  }

  /*
   * First, remove nodes that should not be simplified/colored.
   */
  for (int i = 0; i < n; i++) {
    char *pseudo = graph->nodes.data[i].pseudo;

    if (graph->nodes.data[i].is_precolored) {
      removed[i] = true;
      remaining--;
      continue;
    }

    struct AsmType *asm_type = pseudo_type_get(types, pseudo);
    assert(asm_type && "interference node has no known AsmType");

    if (force_stack && pseudo_set_contains(force_stack, pseudo)) {
      if (out_spilled) {
        pseudo_set_add(out_spilled, pseudo);
      }

      assign_stack_home(&homes, types, map, used_stack_bytes, pseudo);

      removed[i] = true;
      remaining--;
      continue;
    }

    if (asm_type_reg_class(*asm_type) == REGCLASS_NONE) {
      /*
       * Non-integer-register values are not classic register-pressure spills.
       * Keep assigning them stack homes directly.
       */
      assign_stack_home(&homes, types, map, used_stack_bytes, pseudo);

      removed[i] = true;
      remaining--;
      continue;
    }
  }

  while (remaining > 0) {
    int node_idx;
    bool was_spill_candidate = false;

    node_idx = pick_low_degree_node(graph, types, removed, spill_costs);

    if (node_idx < 0) {
      node_idx = pick_spill_candidate(graph, types, removed, spill_costs);
      was_spill_candidate = true;
    }

    assert(node_idx >= 0 && "no node available during coloring");

    removed[node_idx] = true;
    remaining--;

    vec_insert(&select_stack, ((struct SelectStackEntry){
                                  .node_idx = node_idx,
                                  .was_spill_candidate = was_spill_candidate,
                              }));
  }

  /*
   * Select phase.
   *
   * Pop nodes and assign real registers. If a spill candidate cannot be
   * colored, report it through out_spilled so regalloc_fn can rewrite and
   * retry instead of accepting a direct stack home.
   */
  while (select_stack.len > 0) {
    struct SelectStackEntry entry;
    struct InterferenceNode *node;
    enum AsmRegister reg;

    entry = select_stack.data[--select_stack.len];
    node = &graph->nodes.data[entry.node_idx];

    if (pseudo_home_get(&homes, node->pseudo)) {
      continue;
    }

    if (choose_color_for_node(graph, types, &homes, entry.node_idx, &reg)) {
      pseudo_home_add_reg(&homes, node->pseudo, reg);
    } else {
      if (out_spilled) {
        pseudo_set_add(out_spilled, node->pseudo);
      }

      /*
       * Do not assign a stack home here for normal register-pressure spills.
       * regalloc_fn will rewrite these spilled pseudos and rerun allocation.
       */
    }
  }

  free(removed);
  vec_free(&select_stack);

  return homes;
}

void print_pseudo_homes(VecPseudoHome *homes)
{
  printf("Pseudo homes:\n");

  for (int i = 0; i < homes->len; i++) {
    struct PseudoHome *home = &homes->data[i];

    printf("  %-20s -> ", home->pseudo);

    if (home->kind == PSEUDO_HOME_REG) {
      struct AsmOperand op = {
          .kind = AsmOperand_REG,
          .asm_type = (struct AsmType){.kind = AsmType_LONGWORD},
          .as.reg = home->as.reg,
      };

      print_asm_operand(&op);
      printf("\n");
    } else {
      printf("stack(%d)\n", home->as.stack_offset);
    }
  }
}

void verify_coloring(struct InterferenceGraph *graph,
                            VecPseudoHome *homes)
{
  for (int i = 0; i < graph->nodes.len; i++) {
    struct InterferenceNode *a_node = &graph->nodes.data[i];

    if (a_node->is_precolored) {
      continue;
    }

    struct PseudoHome *ha = pseudo_home_get(homes, a_node->pseudo);

    if (!ha || ha->kind != PSEUDO_HOME_REG) {
      continue;
    }

    for (int j = 0; j < a_node->neighbors.len; j++) {
      int neighbor_idx = a_node->neighbors.data[j];

      if (i > neighbor_idx) {
        continue;
      }

      struct InterferenceNode *b_node = &graph->nodes.data[neighbor_idx];

      if (b_node->is_precolored) {
        if (ha->as.reg == b_node->precolored_reg) {
          fprintf(stderr,
                  "register coloring bug: %s assigned clobbered/precolored "
                  "register %s\n",
                  a_node->pseudo, b_node->pseudo);
          abort();
        }

        continue;
      }

      struct PseudoHome *hb = pseudo_home_get(homes, b_node->pseudo);

      if (!hb || hb->kind != PSEUDO_HOME_REG) {
        continue;
      }

      if (ha->as.reg == hb->as.reg) {
        fprintf(
            stderr,
            "register coloring bug: %s and %s both assigned same register\n",
            a_node->pseudo, b_node->pseudo);
        abort();
      }
    }
  }
}

char *move_node_name(struct InterferenceGraph *graph, int idx)
{
  assert(idx >= 0 && idx < graph->nodes.len);
  return graph->nodes.data[idx].pseudo;
}

bool interference_has_edge_idx(struct InterferenceGraph *graph,
                                      int a_idx, int b_idx)
{
  if (a_idx < 0 || b_idx < 0) {
    return false;
  }

  return int_vec_contains(&graph->nodes.data[a_idx].neighbors, b_idx);
}

bool move_operand_node(struct InterferenceGraph *graph,
                              struct AsmOperand *op, int *out_idx)
{
  if (op->kind == AsmOperand_PSEUDO) {
    *out_idx = interference_node_index(graph, op->as.pseudo);
    return *out_idx >= 0;
  }

  if (op->kind == AsmOperand_REG) {
    char *name = precolored_reg_name(op->as.reg);
    *out_idx = interference_node_index(graph, name);
    return *out_idx >= 0;
  }

  return false;
}

bool is_pseudo_to_pseudo_move(struct AsmInstr *instr, char **out_src,
                                     char **out_dst)
{
  if (instr->kind != AsmInstr_MOV) {
    return false;
  }

  if (instr->as.mov.src.kind != AsmOperand_PSEUDO) {
    return false;
  }

  if (instr->as.mov.dst.kind != AsmOperand_PSEUDO) {
    return false;
  }

  *out_src = instr->as.mov.src.as.pseudo;
  *out_dst = instr->as.mov.dst.as.pseudo;

  if (strcmp(*out_src, *out_dst) == 0) {
    return false;
  }

  return true;
}

VecMove collect_moves_from_graph(struct AsmFunction *fn,
                                        struct InterferenceGraph *graph)
{
  VecMove moves = {0};

  for (int i = 0; i < fn->body.len; i++) {
    struct AsmInstr *instr = &fn->body.data[i];
    int src_idx;
    int dst_idx;

    if (instr->kind != AsmInstr_MOV) {
      continue;
    }

    if (!move_operand_node(graph, &instr->as.mov.src, &src_idx)) {
      continue;
    }

    if (!move_operand_node(graph, &instr->as.mov.dst, &dst_idx)) {
      continue;
    }

    if (src_idx == dst_idx) {
      continue;
    }

    vec_insert(&moves, ((struct Move){
                           .src_idx = src_idx,
                           .dst_idx = dst_idx,
                       }));
  }

  return moves;
}

void print_moves(struct InterferenceGraph *graph, VecMove *moves)
{
  printf("Moves:\n");

  for (int i = 0; i < moves->len; i++) {
    int src = moves->data[i].src_idx;
    int dst = moves->data[i].dst_idx;

    printf("  %s -> %s\n", move_node_name(graph, src),
           move_node_name(graph, dst));
  }
}

void free_moves(VecMove *moves)
{
  vec_free(moves);

  moves->capacity = 0;
  moves->len = 0;
  moves->data = NULL;
}

void dump_moves_dot(FILE *out, struct InterferenceGraph *graph,
                           VecMove *moves)
{
  fprintf(out, "digraph moves {\n");
  fprintf(out, "  graph [fontname=\"monospace\"];\n");
  fprintf(out, "  node [shape=ellipse, fontname=\"monospace\"];\n");
  fprintf(out, "  edge [fontname=\"monospace\"];\n\n");

  for (int i = 0; i < moves->len; i++) {
    int src = moves->data[i].src_idx;
    int dst = moves->data[i].dst_idx;

    fprintf(out, "  \"");
    dot_escape(out, move_node_name(graph, src));
    fprintf(out, "\" -> \"");
    dot_escape(out, move_node_name(graph, dst));
    fprintf(out, "\" [label=\"mov\"];\n");
  }

  fprintf(out, "}\n");
}

void write_moves_dot(struct InterferenceGraph *graph, VecMove *moves,
                            const char *path)
{
  FILE *out = fopen(path, "w");

  if (!out) {
    perror("fopen");
    return;
  }

  dump_moves_dot(out, graph, moves);

  fclose(out);
}

bool is_same_reg_operand(struct AsmOperand *a, struct AsmOperand *b)
{
  return a->kind == AsmOperand_REG && b->kind == AsmOperand_REG &&
         a->as.reg == b->as.reg;
}

bool is_redundant_mov(struct AsmInstr *instr)
{
  return instr->kind == AsmInstr_MOV &&
         is_same_reg_operand(&instr->as.mov.src, &instr->as.mov.dst);
}

bool interference_has_edge(struct InterferenceGraph *graph, char *a,
                                  char *b)
{
  int ai = interference_node_index(graph, a);
  int bi = interference_node_index(graph, b);

  if (ai < 0 || bi < 0) {
    return false;
  }

  return int_vec_contains(&graph->nodes.data[ai].neighbors, bi);
}

int interference_degree(struct InterferenceGraph *graph, char *pseudo)
{
  int idx = interference_node_index(graph, pseudo);

  if (idx < 0) {
    return 0;
  }

  return graph->nodes.data[idx].neighbors.len;
}

bool briggs_can_coalesce(struct InterferenceGraph *graph, char *a,
                                char *b, int k)
{
  int ai = interference_node_index(graph, a);
  int bi = interference_node_index(graph, b);
  VecInt neighbors = {0};
  int high_degree_count = 0;

  assert(ai >= 0);
  assert(bi >= 0);

  for (int i = 0; i < graph->nodes.data[ai].neighbors.len; i++) {
    int n = graph->nodes.data[ai].neighbors.data[i];

    if (n != bi) {
      int_set_add(&neighbors, n);
    }
  }

  for (int i = 0; i < graph->nodes.data[bi].neighbors.len; i++) {
    int n = graph->nodes.data[bi].neighbors.data[i];

    if (n != ai) {
      int_set_add(&neighbors, n);
    }
  }

  for (int i = 0; i < neighbors.len; i++) {
    int n = neighbors.data[i];

    if (graph->nodes.data[n].neighbors.len >= k) {
      high_degree_count++;
    }
  }

  vec_free(&neighbors);

  return high_degree_count < k;
}

int aliased_index_for_orig_rep(struct InterferenceGraph *aliased,
                                      struct InterferenceGraph *orig,
                                      int orig_rep_idx)
{
  char *name = orig->nodes.data[orig_rep_idx].pseudo;

  return interference_node_index(aliased, name);
}

int uf_find(int *parent, int x)
{
  if (parent[x] != x) {
    parent[x] = uf_find(parent, parent[x]);
  }

  return parent[x];
}

void uf_union(int *parent, int a, int b)
{
  int ra = uf_find(parent, a);
  int rb = uf_find(parent, b);

  if (ra != rb) {
    parent[rb] = ra;
  }
}

bool george_ok(struct InterferenceGraph *graph, int t_idx, int r_idx,
                      int k)
{
  struct InterferenceNode *t = &graph->nodes.data[t_idx];

  if (t->is_precolored) {
    return true;
  }

  if (t->neighbors.len < k) {
    return true;
  }

  if (interference_has_edge_idx(graph, t_idx, r_idx)) {
    return true;
  }

  return false;
}

bool george_can_coalesce(struct InterferenceGraph *graph, int pseudo_idx,
                                int precolored_idx, int k)
{
  assert(!graph->nodes.data[pseudo_idx].is_precolored);
  assert(graph->nodes.data[precolored_idx].is_precolored);

  for (int i = 0; i < graph->nodes.data[pseudo_idx].neighbors.len; i++) {
    int t_idx = graph->nodes.data[pseudo_idx].neighbors.data[i];

    if (!george_ok(graph, t_idx, precolored_idx, k)) {
      return false;
    }
  }

  return true;
}

void uf_union_into(int *parent, int keep, int discard)
{
  int r_keep = uf_find(parent, keep);
  int r_discard = uf_find(parent, discard);

  if (r_keep != r_discard) {
    parent[r_discard] = r_keep;
  }
}

void debug_briggs_moves(struct InterferenceGraph *graph, VecMove *moves)
{
  int k = NUM_ALLOCATABLE_INT_REGS;

  printf("Move coalescing decisions:\n");

  for (int i = 0; i < moves->len; i++) {
    int src_idx = moves->data[i].src_idx;
    int dst_idx = moves->data[i].dst_idx;

    char *src = move_node_name(graph, src_idx);
    char *dst = move_node_name(graph, dst_idx);

    bool src_pre = graph->nodes.data[src_idx].is_precolored;
    bool dst_pre = graph->nodes.data[dst_idx].is_precolored;

    if (interference_has_edge_idx(graph, src_idx, dst_idx)) {
      printf("  reject %-20s -> %-20s : constrained\n", src, dst);
      continue;
    }

    if (src_pre && dst_pre) {
      printf("  reject %-20s -> %-20s : both precolored\n", src, dst);
      continue;
    }

    if (src_pre || dst_pre) {
      int pre_idx = src_pre ? src_idx : dst_idx;
      int pseudo_idx = src_pre ? dst_idx : src_idx;

      if (george_can_coalesce(graph, pseudo_idx, pre_idx, k)) {
        printf("  accept %-20s -> %-20s : George OK\n", src, dst);
      } else {
        printf("  reject %-20s -> %-20s : George risky\n", src, dst);
      }

      continue;
    }

    if (briggs_can_coalesce(graph, src, dst, k)) {
      printf("  accept %-20s -> %-20s : Briggs OK\n", src, dst);
    } else {
      printf("  reject %-20s -> %-20s : Briggs risky\n", src, dst);
    }
  }
}

bool asm_types_coalesce_compatible(struct AsmType a, struct AsmType b)
{
  return a.kind == b.kind && asm_type_can_live_in_int_reg(a) &&
         asm_type_can_live_in_int_reg(b);
}

struct InterferenceGraph build_aliased_interference_graph(
    struct InterferenceGraph *orig, int *parent)
{
  struct InterferenceGraph out = {0};

  /*
   * Add representative nodes.
   */
  for (int i = 0; i < orig->nodes.len; i++) {
    int ri = orig->nodes.data[i].is_precolored ? i : uf_find(parent, i);

    if (orig->nodes.data[ri].is_precolored) {
      interference_add_precolored_reg(&out,
                                      orig->nodes.data[ri].precolored_reg);
    } else {
      interference_add_node(&out, orig->nodes.data[ri].pseudo);
    }
  }

  /*
   * Add edges between representatives.
   */
  for (int i = 0; i < orig->nodes.len; i++) {
    int ri = orig->nodes.data[i].is_precolored ? i : uf_find(parent, i);

    for (int j = 0; j < orig->nodes.data[i].neighbors.len; j++) {
      int n = orig->nodes.data[i].neighbors.data[j];
      int rn = orig->nodes.data[n].is_precolored ? n : uf_find(parent, n);

      if (ri == rn) {
        continue;
      }

      if (orig->nodes.data[ri].is_precolored &&
          orig->nodes.data[rn].is_precolored) {
        int ai = interference_add_precolored_reg(
            &out, orig->nodes.data[ri].precolored_reg);
        int bi = interference_add_precolored_reg(
            &out, orig->nodes.data[rn].precolored_reg);

        interference_add_edge_by_index(&out, ai, bi);
      } else if (orig->nodes.data[ri].is_precolored) {
        int ai = interference_add_precolored_reg(
            &out, orig->nodes.data[ri].precolored_reg);
        int bi = interference_add_node(&out, orig->nodes.data[rn].pseudo);

        interference_add_edge_by_index(&out, ai, bi);
      } else if (orig->nodes.data[rn].is_precolored) {
        int ai = interference_add_node(&out, orig->nodes.data[ri].pseudo);
        int bi = interference_add_precolored_reg(
            &out, orig->nodes.data[rn].precolored_reg);

        interference_add_edge_by_index(&out, ai, bi);
      } else {
        interference_add_edge(&out, orig->nodes.data[ri].pseudo,
                              orig->nodes.data[rn].pseudo);
      }
    }
  }

  return out;
}

int *coalesce_briggs_george(struct InterferenceGraph *orig,
                                   VecPseudoType *types, VecMove *moves)
{
  int n = orig->nodes.len;
  int *parent = malloc(sizeof(*parent) * n);

  if (!parent) {
    perror("malloc");
    exit(1);
  }

  for (int i = 0; i < n; i++) {
    parent[i] = i;
  }

  bool changed = true;

  while (changed) {
    changed = false;

    struct InterferenceGraph aliased =
        build_aliased_interference_graph(orig, parent);

    for (int i = 0; i < moves->len; i++) {
      int src_idx = moves->data[i].src_idx;
      int dst_idx = moves->data[i].dst_idx;

      int src_rep_idx = uf_find(parent, src_idx);
      int dst_rep_idx = uf_find(parent, dst_idx);

      if (src_rep_idx == dst_rep_idx) {
        continue;
      }

      int src_alias_idx =
          aliased_index_for_orig_rep(&aliased, orig, src_rep_idx);
      int dst_alias_idx =
          aliased_index_for_orig_rep(&aliased, orig, dst_rep_idx);

      if (src_alias_idx < 0 || dst_alias_idx < 0) {
        continue;
      }

      bool src_pre = orig->nodes.data[src_rep_idx].is_precolored;
      bool dst_pre = orig->nodes.data[dst_rep_idx].is_precolored;

      /*
       * If they already interfere, the move is constrained.
       */
      if (interference_has_edge_idx(&aliased, src_alias_idx, dst_alias_idx)) {
        continue;
      }

      /*
       * precolored <-> precolored: never coalesce.
       */
      if (src_pre && dst_pre) {
        continue;
      }

      /*
       * George: pseudo <-> precolored.
       */
      if (src_pre || dst_pre) {
        int pre_orig_idx = src_pre ? src_rep_idx : dst_rep_idx;
        int pseudo_orig_idx = src_pre ? dst_rep_idx : src_rep_idx;

        int pre_alias_idx = src_pre ? src_alias_idx : dst_alias_idx;
        int pseudo_alias_idx = src_pre ? dst_alias_idx : src_alias_idx;

        if (george_can_coalesce(&aliased, pseudo_alias_idx, pre_alias_idx,
                                NUM_ALLOCATABLE_INT_REGS)) {
#ifdef DEBUG_COALESCE
          printf("george coalesce %s <- %s\n",
                 orig->nodes.data[pre_orig_idx].pseudo,
                 orig->nodes.data[pseudo_orig_idx].pseudo);
#endif

          /*
           * Keep the precolored representative.
           */
          uf_union_into(parent, pre_orig_idx, pseudo_orig_idx);
          changed = true;
          break;
        }

        continue;
      }

      /*
       * Briggs: pseudo <-> pseudo.
       */
      {
        char *src_rep = orig->nodes.data[src_rep_idx].pseudo;
        char *dst_rep = orig->nodes.data[dst_rep_idx].pseudo;

        struct AsmType *src_type = pseudo_type_get(types, src_rep);
        struct AsmType *dst_type = pseudo_type_get(types, dst_rep);

        assert(src_type && dst_type);

        if (!asm_types_coalesce_compatible(*src_type, *dst_type)) {
          continue;
        }

        if (briggs_can_coalesce(&aliased, src_rep, dst_rep,
                                NUM_ALLOCATABLE_INT_REGS)) {
#ifdef DEBUG_COALESCE
          printf("briggs coalesce %s <- %s\n", src_rep, dst_rep);
#endif

          uf_union_into(parent, src_rep_idx, dst_rep_idx);
          changed = true;
          break;
        }
      }
    }

    free_interference_graph(&aliased);
  }

  return parent;
}

VecPseudoHome expand_coalesced_homes(struct InterferenceGraph *orig,
                                            int *parent,
                                            VecPseudoHome *coalesced_homes)
{
  VecPseudoHome homes = {0};

  for (int i = 0; i < orig->nodes.len; i++) {
    if (orig->nodes.data[i].is_precolored) {
      continue;
    }

    int rep_idx = uf_find(parent, i);

    char *pseudo = orig->nodes.data[i].pseudo;

    if (orig->nodes.data[rep_idx].is_precolored) {
      pseudo_home_add_reg(&homes, pseudo,
                          orig->nodes.data[rep_idx].precolored_reg);
      continue;
    }

    char *rep = orig->nodes.data[rep_idx].pseudo;

    struct PseudoHome *rep_home = pseudo_home_get(coalesced_homes, rep);

    if (!rep_home) {
      fprintf(stderr,
              "expand_coalesced_homes bug:\n"
              "  original pseudo: %s\n"
              "  representative: %s\n"
              "  rep_idx: %d\n"
              "  is_precolored: %d\n"
              "  available coalesced homes:\n",
              pseudo, rep, rep_idx, orig->nodes.data[rep_idx].is_precolored);

      for (int h = 0; h < coalesced_homes->len; h++) {
        fprintf(stderr, "    %s\n", coalesced_homes->data[h].pseudo);
      }

      abort();
    }

    if (rep_home->kind == PSEUDO_HOME_REG) {
      pseudo_home_add_reg(&homes, pseudo, rep_home->as.reg);
    } else {
      pseudo_home_add_stack(&homes, pseudo, rep_home->as.stack_offset);
    }
  }

  return homes;
}

bool same_asm_operand(struct AsmOperand *a, struct AsmOperand *b)
{
  if (a->kind != b->kind) {
    return false;
  }

  switch (a->kind) {
    case AsmOperand_REG:
      return a->as.reg == b->as.reg;

    case AsmOperand_STACK:
      return a->as.stack_offset == b->as.stack_offset;

    case AsmOperand_IMM:
      return a->as.imm == b->as.imm;

    case AsmOperand_PSEUDO:
      return strcmp(a->as.pseudo, b->as.pseudo) == 0;

    default:
      return false;
  }
}

void remove_redundant_moves(struct AsmFunction *fn)
{
  int out = 0;

  for (int i = 0; i < fn->body.len; i++) {
    struct AsmInstr *instr = &fn->body.data[i];

    if (instr->kind == AsmInstr_MOV &&
        same_asm_operand(&instr->as.mov.src, &instr->as.mov.dst)) {
      continue;
    }

    fn->body.data[out++] = fn->body.data[i];
  }

  fn->body.len = out;
}

bool instr_is_call(struct AsmInstr *instr)
{
  return instr->kind == AsmInstr_CALL;
}

bool pseudo_set_any_contains(VecPseudo *set, char *pseudo)
{
  return pseudo_set_contains(set, pseudo);
}

VecPseudo collect_call_live_pseudos(struct AsmFunction *fn,
                                           struct InstrLiveness *lv)
{
  VecPseudo call_live = {0};

  for (int i = 0; i < fn->body.len; i++) {
    if (!instr_is_call(&fn->body.data[i])) {
      continue;
    }

    for (int j = 0; j < lv[i].live_after.len; j++) {
      pseudo_set_add(&call_live, lv[i].live_after.data[j]);
    }
  }

  return call_live;
}

bool is_callee_saved_reg(enum AsmRegister reg)
{
  for (int i = 0; i < NUM_CALLEE_SAVED_INT_REGS; i++) {
    if (callee_saved_int_regs[i] == reg) {
      return true;
    }
  }

  return false;
}

bool reg_vec_contains(VecInt *regs, enum AsmRegister reg)
{
  for (int i = 0; i < regs->len; i++) {
    if (regs->data[i] == (int) reg) {
      return true;
    }
  }

  return false;
}

void reg_vec_add(VecInt *regs, enum AsmRegister reg)
{
  if (!reg_vec_contains(regs, reg)) {
    vec_insert(regs, reg);
  }
}

VecInt collect_used_callee_saved_regs(VecPseudoHome *homes)
{
  VecInt used = {0};

  for (int i = 0; i < homes->len; i++) {
    struct PseudoHome *home = &homes->data[i];

    if (home->kind != PSEUDO_HOME_REG) {
      continue;
    }

    if (is_callee_saved_reg(home->as.reg)) {
      reg_vec_add(&used, home->as.reg);
    }
  }

  return used;
}

struct AsmOperand make_reg_operand(enum AsmRegister reg,
                                          struct AsmType asm_type)
{
  return (struct AsmOperand){
      .kind = AsmOperand_REG,
      .asm_type = asm_type,
      .as.reg = reg,
  };
}

struct AsmOperand make_pseudo_operand(char *pseudo,
                                             struct AsmType asm_type)
{
  return (struct AsmOperand){
      .kind = AsmOperand_PSEUDO,
      .asm_type = asm_type,
      .as.pseudo = strdup(pseudo),
  };
}

struct AsmOperand make_stack_operand(int offset, struct AsmType asm_type)
{
  return (struct AsmOperand){
      .kind = AsmOperand_STACK,
      .asm_type = asm_type,
      .as.stack_offset = offset,
  };
}

struct AsmInstr make_mov_instr(struct AsmOperand src,
                                      struct AsmOperand dst,
                                      struct AsmType asm_type)
{
  struct AsmInstr instr = {0};

  instr.kind = AsmInstr_MOV;
  instr.asm_type = asm_type;
  instr.as.mov.src = src;
  instr.as.mov.dst = dst;

  return instr;
}

bool is_spilled_pseudo_operand(struct AsmOperand *op, VecPseudo *spilled)
{
  return op->kind == AsmOperand_PSEUDO &&
         pseudo_set_contains(spilled, op->as.pseudo);
}

struct AsmInstr make_push_reg(enum AsmRegister reg)
{
  return (struct AsmInstr){
      .kind = AsmInstr_PUSH,
      .asm_type = (struct AsmType){.kind = AsmType_QUADWORD},
      .as.push =
          {
              .op = make_reg_operand(
                  reg, (struct AsmType){.kind = AsmType_QUADWORD}),
          },
  };
}

struct AsmInstr make_pop_reg(enum AsmRegister reg)
{
  return (struct AsmInstr){
      .kind = AsmInstr_POP,
      .asm_type = (struct AsmType){.kind = AsmType_QUADWORD},
      .as.pop =
          {
              .op = make_reg_operand(
                  reg, (struct AsmType){.kind = AsmType_QUADWORD}),
          },
  };
}

bool is_reg_operand(struct AsmOperand *op, enum AsmRegister reg)
{
  return op->kind == AsmOperand_REG && op->as.reg == reg;
}

bool is_restore_sp_from_bp(struct AsmInstr *instr)
{
  return instr->kind == AsmInstr_MOV &&
         is_reg_operand(&instr->as.mov.src, BP) &&
         is_reg_operand(&instr->as.mov.dst, SP);
}

bool is_pop_bp(struct AsmInstr *instr)
{
  return instr->kind == AsmInstr_POP && is_reg_operand(&instr->as.pop.op, BP);
}

bool is_stack_alloc_instr(struct AsmInstr *instr)
{
  return instr->kind == AsmInstr_BIN &&
         instr->as.binary.kind == AsmInstrBinary_SUB &&
         instr->as.binary.lhs.kind == AsmOperand_IMM &&
         is_reg_operand(&instr->as.binary.rhs, SP);
}

void save_restore_callee_saved_regs(struct AsmFunction *fn,
                                           VecInt *used_regs)
{
  if (used_regs->len == 0) {
    return;
  }

  VecAsmInstr new_body = {0};

  for (int i = 0; i < fn->body.len; i++) {
    /*
     * Before every epilogue `mov %rbp, %rsp`, restore in reverse order.
     */
    if (is_restore_sp_from_bp(&fn->body.data[i])) {
      for (int r = used_regs->len - 1; r >= 0; r--) {
        vec_insert(&new_body,
                   make_pop_reg((enum AsmRegister) used_regs->data[r]));
      }
    }

    vec_insert(&new_body, fn->body.data[i]);

    /*
     * After stack allocation, save all used callee-saved registers.
     */
    if (is_stack_alloc_instr(&fn->body.data[i])) {
      for (int r = 0; r < used_regs->len; r++) {
        vec_insert(&new_body,
                   make_push_reg((enum AsmRegister) used_regs->data[r]));
      }
    }
  }

  vec_free(&fn->body);
  fn->body = new_body;
}

void patch_stack_alloc(struct AsmFunction *fn, int stack_size)
{
  for (int i = 0; i < fn->body.len; i++) {
    struct AsmInstr *instr = &fn->body.data[i];

    if (instr->kind == AsmInstr_BIN &&
        instr->as.binary.kind == AsmInstrBinary_SUB &&
        instr->as.binary.lhs.kind == AsmOperand_IMM &&
        is_reg_operand(&instr->as.binary.rhs, SP)) {
      instr->as.binary.lhs.as.imm = stack_size;
      return;
    }
  }

  assert(0 && "could not find prologue stack allocation");
}

void rewrite_spilled_use(VecAsmInstr *before, struct AsmOperand *op,
                                VecPseudo *spilled, struct Map *map,
                                int *used_stack_bytes)
{
  if (!is_spilled_pseudo_operand(op, spilled)) {
    return;
  }

  struct AsmType asm_type = op->asm_type;
  int offset = get_offset(map, op->as.pseudo, asm_type, used_stack_bytes);

  char *tmp = fresh_spill_tmp();

  struct AsmOperand stack_op = make_stack_operand(offset, asm_type);
  struct AsmOperand tmp_dst = make_pseudo_operand(tmp, asm_type);

  vec_insert(before, make_mov_instr(stack_op, tmp_dst, asm_type));

  *op = make_pseudo_operand(tmp, asm_type);

  free(tmp);
}

void rewrite_spilled_def(VecAsmInstr *after, struct AsmOperand *op,
                                VecPseudo *spilled, struct Map *map,
                                int *used_stack_bytes)
{
  if (!is_spilled_pseudo_operand(op, spilled)) {
    return;
  }

  struct AsmType asm_type = op->asm_type;
  int offset = get_offset(map, op->as.pseudo, asm_type, used_stack_bytes);

  char *tmp = fresh_spill_tmp();

  struct AsmOperand tmp_src = make_pseudo_operand(tmp, asm_type);
  struct AsmOperand stack_op = make_stack_operand(offset, asm_type);

  *op = make_pseudo_operand(tmp, asm_type);

  vec_insert(after, make_mov_instr(tmp_src, stack_op, asm_type));

  free(tmp);
}

void rewrite_spilled_use_def(VecAsmInstr *before, VecAsmInstr *after,
                                    struct AsmOperand *op, VecPseudo *spilled,
                                    struct Map *map, int *used_stack_bytes)
{
  if (!is_spilled_pseudo_operand(op, spilled)) {
    return;
  }

  struct AsmType asm_type = op->asm_type;
  int offset = get_offset(map, op->as.pseudo, asm_type, used_stack_bytes);

  char *tmp = fresh_spill_tmp();

  struct AsmOperand stack_load = make_stack_operand(offset, asm_type);
  struct AsmOperand tmp_load_dst = make_pseudo_operand(tmp, asm_type);

  vec_insert(before, make_mov_instr(stack_load, tmp_load_dst, asm_type));

  *op = make_pseudo_operand(tmp, asm_type);

  struct AsmOperand tmp_store_src = make_pseudo_operand(tmp, asm_type);
  struct AsmOperand stack_store = make_stack_operand(offset, asm_type);

  vec_insert(after, make_mov_instr(tmp_store_src, stack_store, asm_type));

  free(tmp);
}

void append_instrs(VecAsmInstr *dst, VecAsmInstr *src)
{
  for (int i = 0; i < src->len; i++) {
    vec_insert(dst, src->data[i]);
  }
}

void rewrite_spills(struct AsmFunction *fn, VecPseudo *spilled,
                           struct Map *map, int *used_stack_bytes)
{
  if (spilled->len == 0) {
    return;
  }

  VecAsmInstr new_body = {0};

  for (int i = 0; i < fn->body.len; i++) {
    struct AsmInstr instr = fn->body.data[i];

    VecAsmInstr before = {0};
    VecAsmInstr after = {0};

    switch (instr.kind) {
      case AsmInstr_MOV:
        rewrite_spilled_use(&before, &instr.as.mov.src, spilled, map,
                            used_stack_bytes);

        rewrite_spilled_def(&after, &instr.as.mov.dst, spilled, map,
                            used_stack_bytes);
        break;

      case AsmInstr_BIN:
        /*
         * x86-style binary:
         *   rhs = rhs OP lhs
         *
         * lhs is use-only.
         * rhs is use+def.
         */
        rewrite_spilled_use(&before, &instr.as.binary.lhs, spilled, map,
                            used_stack_bytes);

        rewrite_spilled_use_def(&before, &after, &instr.as.binary.rhs, spilled,
                                map, used_stack_bytes);
        break;

      case AsmInstr_CMP:
        rewrite_spilled_use(&before, &instr.as.cmp.lhs, spilled, map,
                            used_stack_bytes);

        rewrite_spilled_use(&before, &instr.as.cmp.rhs, spilled, map,
                            used_stack_bytes);
        break;

      case AsmInstr_CVT:
        rewrite_spilled_use(&before, &instr.as.cvt.src, spilled, map,
                            used_stack_bytes);

        rewrite_spilled_def(&after, &instr.as.cvt.dst, spilled, map,
                            used_stack_bytes);
        break;

      case AsmInstr_SetCC:
        rewrite_spilled_def(&after, &instr.as.setcc.op, spilled, map,
                            used_stack_bytes);
        break;

      case AsmInstr_UNARY:
        rewrite_spilled_use_def(&before, &after, &instr.as.unary.op, spilled,
                                map, used_stack_bytes);
        break;

      case AsmInstr_PUSH:
        rewrite_spilled_use(&before, &instr.as.push.op, spilled, map,
                            used_stack_bytes);
        break;

      case AsmInstr_POP:
        rewrite_spilled_def(&after, &instr.as.pop.op, spilled, map,
                            used_stack_bytes);
        break;

      case AsmInstr_LEA:
        rewrite_spilled_use(&before, &instr.as.lea.src, spilled, map,
                            used_stack_bytes);

        rewrite_spilled_def(&after, &instr.as.lea.dst, spilled, map,
                            used_stack_bytes);
        break;

      case AsmInstr_CALL:
      case AsmInstr_RET:
      case AsmInstr_JMP:
      case AsmInstr_JmpCC:
      case AsmInstr_LBL:
      case AsmInstr_REP_MOVSB:
        break;

      default:
        assert(0 && "Unhandled instruction in rewrite_spills");
    }

    append_instrs(&new_body, &before);
    vec_insert(&new_body, instr);
    append_instrs(&new_body, &after);

    vec_free(&before);
    vec_free(&after);
  }

  vec_free(&fn->body);
  fn->body = new_body;
}

void print_spilled_pseudos(VecPseudo *spilled)
{
  printf("Spilled pseudos:\n");

  for (int i = 0; i < spilled->len; i++) {
    printf("  %s\n", spilled->data[i]);
  }
}

#define MAX_REGALLOC_ATTEMPTS 20

VecPseudo expand_spilled_reps_to_original_pseudos(
    struct InterferenceGraph *orig, int *parent, VecPseudo *spilled_reps)
{
  VecPseudo out = {0};

  for (int i = 0; i < orig->nodes.len; i++) {
    if (orig->nodes.data[i].is_precolored) {
      continue;
    }

    int rep_idx = uf_find(parent, i);
    char *rep_name = orig->nodes.data[rep_idx].pseudo;

    if (pseudo_set_contains(spilled_reps, rep_name)) {
      pseudo_set_add(&out, orig->nodes.data[i].pseudo);
    }
  }

  return out;
}

void add_call_clobber_interference(struct InterferenceGraph *graph,
                                          VecPseudoType *types,
                                          struct InstrLiveness *lv)
{
  for (int i = 0; i < lv->live_after.len; i++) {
    char *pseudo = lv->live_after.data[i];
    struct AsmType *asm_type;
    enum RegClass cls;
    int pseudo_idx;

    asm_type = pseudo_type_get(types, pseudo);
    if (!asm_type) {
      continue;
    }

    cls = asm_type_reg_class(*asm_type);

    if (cls == REGCLASS_NONE) {
      continue;
    }

    pseudo_idx = interference_add_node(graph, pseudo);

    if (cls == REGCLASS_INT) {
      for (int r = 0; r < NUM_CALLER_SAVED_INT_REGS; r++) {
        int reg_idx =
            interference_add_precolored_reg(graph, caller_saved_int_regs[r]);

        interference_add_edge_by_index(graph, pseudo_idx, reg_idx);
      }
    } else if (cls == REGCLASS_SSE) {
      for (int r = 0; r < NUM_CALLER_SAVED_SSE_REGS; r++) {
        int reg_idx =
            interference_add_precolored_reg(graph, caller_saved_sse_regs[r]);

        interference_add_edge_by_index(graph, pseudo_idx, reg_idx);
      }
    }
  }
}

struct InterferenceGraph build_interference_graph(
    struct AsmFunction *fn, VecPseudoType *types, struct InstrLiveness *lv)
{
  struct InterferenceGraph graph = {0};

  add_precolored_register_nodes(&graph);

  for (int i = 0; i < fn->body.len; i++) {
    add_interferences_for_instr(&graph, &fn->body.data[i], &lv[i]);

    if (fn->body.data[i].kind == AsmInstr_CALL) {
      add_call_clobber_interference(&graph, types, &lv[i]);
    }
  }

  add_abi_param_copy_interference(&graph, fn);

  return graph;
}

struct AsmFunction *regalloc_fn(struct AsmFunction *fn, struct Map *map,
                                int *used_stack_bytes,
                                int *used_callee_saved_count)
{
  *used_callee_saved_count = 0;

  for (int attempt = 0; attempt < MAX_REGALLOC_ATTEMPTS; attempt++) {
    struct InstrLiveness *lv;
    int original_instr_count;

    VecPseudoType types;
    VecMove moves;
    VecPseudoHome coalesced_homes;
    VecPseudoHome homes;
    VecPseudo spilled = {0};
    VecSpillCost spill_costs;

    struct InterferenceGraph interference;
    struct InterferenceGraph coalesced_graph;

    int *coalesce_parent;

#ifdef DEBUG_INTERVALS
    VecLiveInterval intervals;
#endif

#ifdef DEBUG_REGALLOC
    printf("regalloc attempt %d\n", attempt);
#else
    printf("regalloc attempt %d\n", attempt);
#endif

    /*
     * 1. Compute liveness for the current pseudo-based asm.
     */
    original_instr_count = fn->body.len;
    lv = compute_liveness_cfg(fn);

    /*
     * 2. Compute spill costs from the current liveness result.
     */
    spill_costs = compute_spill_costs(lv, original_instr_count);

#ifdef DEBUG_SPILL_COSTS
    print_spill_costs(&spill_costs);
#endif

    /*
     * 3. Collect pseudo types before operands are rewritten.
     */
    types = collect_pseudo_types(fn);

    /*
     * 4. Build interference graph.
     */
    interference = build_interference_graph(fn, &types, lv);

#ifdef DEBUG_INTERFERENCE
    print_interference_graph(&interference);
#endif

#ifdef DEBUG_INTERFERENCE_DOT
    {
      char *path = mkstr("%s.interference.%d.dot", fn->name, attempt);
      write_interference_graph_dot(&interference, path);
      free(path);
    }
#endif

    /*
     * 5. Collect moves using graph node indices.
     */
    moves = collect_moves_from_graph(fn, &interference);

#ifdef DEBUG_MOVES
    print_moves(&interference, &moves);
#endif

#ifdef DEBUG_MOVES_DOT
    {
      char *path = mkstr("%s.moves.%d.dot", fn->name, attempt);
      write_moves_dot(&interference, &moves, path);
      free(path);
    }
#endif

#ifdef DEBUG_BRIGGS
    debug_briggs_moves(&interference, &moves);
#endif

#ifdef DEBUG_INTERVALS
    intervals = build_live_intervals(lv, original_instr_count, &types);
    sort_live_intervals(&intervals);
    print_live_intervals(&intervals);
    free_live_intervals(&intervals);
#endif

    /*
     * 6. Briggs/George coalescing.
     */
    coalesce_parent = coalesce_briggs_george(&interference, &types, &moves);

    /*
     * 7. Build coalesced graph.
     */
    coalesced_graph =
        build_aliased_interference_graph(&interference, coalesce_parent);

#ifdef DEBUG_COALESCED_INTERFERENCE
    printf("Coalesced ");
    print_interference_graph(&coalesced_graph);
#endif

#ifdef DEBUG_COALESCED_INTERFERENCE_DOT
    {
      char *path = mkstr("%s.coalesced.interference.%d.dot", fn->name, attempt);
      write_interference_graph_dot(&coalesced_graph, path);
      free(path);
    }
#endif

    /*
     * 8. Color the coalesced graph.
     *
     * `spilled` contains names from the coalesced graph.
     */
    coalesced_homes =
        color_interference_graph(&coalesced_graph, &types, NULL, &spilled,
                                 &spill_costs, map, used_stack_bytes);

#ifdef DEBUG_SPILLS
    printf("After coloring: spilled.len = %d\n", spilled.len);
    for (int i = 0; i < spilled.len; i++) {
      printf("  spilled rep: %s\n", spilled.data[i]);
    }
#endif

    /*
     * 9. If anything spilled, rewrite the original pseudos and retry.
     *
     * IMPORTANT:
     * Do NOT call expand_coalesced_homes() before this block.
     */
    if (spilled.len > 0) {
      VecPseudo original_spilled;

      original_spilled = expand_spilled_reps_to_original_pseudos(
          &interference, coalesce_parent, &spilled);

#ifdef DEBUG_SPILLS
      printf("Spilled original pseudos:\n");
      for (int i = 0; i < original_spilled.len; i++) {
        printf("  %s\n", original_spilled.data[i]);
      }
#endif

      rewrite_spills(fn, &original_spilled, map, used_stack_bytes);

      pseudo_set_free(&original_spilled);

      free_pseudo_homes(&coalesced_homes);

      free_interference_graph(&coalesced_graph);
      free_interference_graph(&interference);

      free(coalesce_parent);

      free_moves(&moves);
      free_pseudo_types(&types);
      free_spill_costs(&spill_costs);
      free_liveness(lv, original_instr_count);
      pseudo_set_free(&spilled);

      continue;
    }

    /*
     * 10. No spills. Now, and only now, expand homes.
     */
    homes = expand_coalesced_homes(&interference, coalesce_parent,
                                   &coalesced_homes);

    /*
     * 11. Verify final coloring.
     */
    verify_coloring(&interference, &homes);

#ifdef DEBUG_HOMES
    print_pseudo_homes(&homes);
#endif

    /*
     * 12. Track used callee-saved registers.
     */
    {
      VecInt used_callee_saved;

      used_callee_saved = collect_used_callee_saved_regs(&homes);
      *used_callee_saved_count = used_callee_saved.len;

      /*
       * 13. Rewrite pseudo operands to final homes.
       */
      for (int i = 0; i < fn->body.len; i++) {
        regalloc_instr_from_homes(&fn->body.data[i], &homes, map,
                                  used_stack_bytes);
      }

      /*
       * 14. Final cleanup.
       */
      remove_redundant_moves(fn);
      save_restore_callee_saved_regs(fn, &used_callee_saved);

      vec_free(&used_callee_saved);
    }

    /*
     * 15. Cleanup successful attempt.
     */
    free_pseudo_homes(&homes);
    free_pseudo_homes(&coalesced_homes);

    free_interference_graph(&coalesced_graph);
    free_interference_graph(&interference);

    free(coalesce_parent);

    free_moves(&moves);
    free_pseudo_types(&types);
    free_spill_costs(&spill_costs);
    free_liveness(lv, original_instr_count);
    pseudo_set_free(&spilled);

    return fn;
  }

  fprintf(stderr, "register allocation did not converge after %d attempts\n",
          MAX_REGALLOC_ATTEMPTS);
  abort();
}

int compute_frame_stack_adjustment(int local_stack_bytes,
                                          int used_callee_saved_count)
{
  int callee_saved_bytes = 8 * used_callee_saved_count;
  int frame_bytes = local_stack_bytes + callee_saved_bytes;

  int padding = frame_bytes % 16 != 0 ? 16 - (frame_bytes % 16) : 0;

  /*
   * Only return the amount for `subq $N, %rsp`.
   * The callee-saved bytes are created by actual PUSH instructions.
   */
  return local_stack_bytes + padding;
}

struct AsmProgram *regalloc(struct AsmProgram *asmcode)
{
  for (int i = 0; i < asmcode->funcs.len; i++) {
    struct Map *map = malloc(sizeof(struct Map));
    memset(map, 0, sizeof(struct Map));
    map->next = NULL;

    int used_stack_bytes = 0;
    int used_callee_saved_count = 0;

    regalloc_fn(&asmcode->funcs.data[i], map, &used_stack_bytes,
                &used_callee_saved_count);

    int stack_size = compute_frame_stack_adjustment(used_stack_bytes,
                                                    used_callee_saved_count);
    patch_stack_alloc(&asmcode->funcs.data[i], stack_size);
  }
  return asmcode;
}

