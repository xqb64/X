#include "ir_opt.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ir.h"
#include "parser.h"
#include "typechecker.h"

static unsigned long long const_as_u64(struct IRValue *v)
{
  switch (v->type.kind) {
    case I8_T:
      return (unsigned long long) v->as.konst.as.i8;
    case U8_T:
      return v->as.konst.as.u8;
    case I16_T:
      return (unsigned long long) v->as.konst.as.i16;
    case U16_T:
      return v->as.konst.as.u16;
    case I32_T:
      return (unsigned long long) v->as.konst.as.i32;
    case U32_T:
      return v->as.konst.as.u32;
    case I64_T:
      return (unsigned long long) v->as.konst.as.i64;
    case U64_T:
      return v->as.konst.as.u64;
    case BOOL_T:
      return v->as.konst.as.boolean ? 1 : 0;
    default:
      assert(0 && "const_as_u64 called on non-integer const");
  }
}

static long long const_as_i64(struct IRValue *v)
{
  switch (v->type.kind) {
    case I8_T:
      return v->as.konst.as.i8;
    case U8_T:
      return v->as.konst.as.u8;
    case I16_T:
      return v->as.konst.as.i16;
    case U16_T:
      return v->as.konst.as.u16;
    case I32_T:
      return v->as.konst.as.i32;
    case U32_T:
      return v->as.konst.as.u32;
    case I64_T:
      return v->as.konst.as.i64;
    case U64_T:
      return (long long) v->as.konst.as.u64;
    case BOOL_T:
      return v->as.konst.as.boolean ? 1 : 0;
    default:
      assert(0 && "const_as_i64 called on non-integer const");
  }
}

static struct IRValue *mk_const_bool(bool b)
{
  struct IRValue *v = malloc(sizeof(*v));

  v->kind = IRValue_CONST;
  v->type = (Type){.kind = BOOL_T};
  v->as.konst.kind = LITERAL_BOOL;
  v->as.konst.type = v->type;
  v->as.konst.as.boolean = b;

  return v;
}

static struct IRValue *mk_const_int(unsigned long long n, Type type)
{
  struct IRValue *v = malloc(sizeof(*v));

  v->kind = IRValue_CONST;
  v->type = type;
  v->as.konst.kind = LITERAL_NUM;
  v->as.konst.type = type;

  switch (type.kind) {
    case I8_T:
      v->as.konst.as.i8 = (char) n;
      break;
    case U8_T:
      v->as.konst.as.u8 = (unsigned char) n;
      break;
    case I16_T:
      v->as.konst.as.i16 = (short) n;
      break;
    case U16_T:
      v->as.konst.as.u16 = (unsigned short) n;
      break;
    case I32_T:
      v->as.konst.as.i32 = (int) n;
      break;
    case U32_T:
      v->as.konst.as.u32 = (unsigned int) n;
      break;
    case I64_T:
      v->as.konst.as.i64 = (long long) n;
      break;
    case U64_T:
      v->as.konst.as.u64 = n;
      break;
    default:
      free(v);
      return NULL;
  }

  return v;
}

static struct IRValue *fold_binary_const(enum IRInstrBinaryKind kind,
                                         struct IRValue *lhs,
                                         struct IRValue *rhs, Type dst_type)
{
  if (!is_integer_type(lhs->type.kind) && lhs->type.kind != BOOL_T) {
    return NULL;
  }

  if (!is_integer_type(rhs->type.kind) && rhs->type.kind != BOOL_T) {
    return NULL;
  }

  unsigned long long l = const_as_u64(lhs);
  unsigned long long r = const_as_u64(rhs);

  switch (kind) {
    case IRInstrBinary_ADD:
      return mk_const_int(l + r, dst_type);
    case IRInstrBinary_SUB:
      return mk_const_int(l - r, dst_type);
    case IRInstrBinary_MUL:
      return mk_const_int(l * r, dst_type);
    case IRInstrBinary_DIV:
      if (r == 0) {
        return NULL;
      }
      if (is_unsigned(lhs->type.kind)) {
        return mk_const_int(l / r, dst_type);
      } else {
        return mk_const_int(
            (unsigned long long) (const_as_i64(lhs) / const_as_i64(rhs)),
            dst_type);
      }

    case IRInstrBinary_BIT_AND:
      return mk_const_int(l & r, dst_type);
    case IRInstrBinary_BIT_XOR:
      return mk_const_int(l ^ r, dst_type);
    case IRInstrBinary_BIT_OR:
      return mk_const_int(l | r, dst_type);

    case IRInstrBinary_SHL:
      if (r >= 64) {
        return NULL;
      }
      return mk_const_int(l << r, dst_type);
    case IRInstrBinary_SHR:
      if (r >= 64) {
        return NULL;
      }
      return mk_const_int(l >> r, dst_type);
    case IRInstrBinary_SAR:
      if (r >= 64) {
        return NULL;
      }
      return mk_const_int((unsigned long long) (const_as_i64(lhs) >> r),
                          dst_type);

    case IRInstrBinary_E:
      return mk_const_bool(l == r);
    case IRInstrBinary_NE:
      return mk_const_bool(l != r);
    case IRInstrBinary_L:
      return mk_const_bool(is_unsigned(lhs->type.kind)
                               ? l < r
                               : const_as_i64(lhs) < const_as_i64(rhs));
    case IRInstrBinary_LE:
      return mk_const_bool(is_unsigned(lhs->type.kind)
                               ? l <= r
                               : const_as_i64(lhs) <= const_as_i64(rhs));
    case IRInstrBinary_G:
      return mk_const_bool(is_unsigned(lhs->type.kind)
                               ? l > r
                               : const_as_i64(lhs) > const_as_i64(rhs));
    case IRInstrBinary_GE:
      return mk_const_bool(is_unsigned(lhs->type.kind)
                               ? l >= r
                               : const_as_i64(lhs) >= const_as_i64(rhs));
    default:
      assert(0);
  }

  return NULL;
}

static struct IRValue *fold_unary_const(enum IRInstrUnaryKind kind,
                                        struct IRValue *src, Type dst_type)
{
  if (!is_integer_type(src->type.kind) && src->type.kind != BOOL_T) {
    return NULL;
  }

  switch (kind) {
    case IRInstrUnary_NEG:
      return mk_const_int((unsigned long long) (-const_as_i64(src)), dst_type);

    case IRInstrUnary_NOT:
      return mk_const_bool(const_as_u64(src) == 0);

    case IRInstrUnary_BIT_NOT:
      return mk_const_int(~const_as_u64(src), dst_type);

    default:
      assert(0);
  }

  return NULL;
}

static struct IRValue *const_env_get(VecConstBinding *env, char *name)
{
  for (int i = env->len - 1; i >= 0; i--) {
    if (strcmp(env->data[i].name, name) == 0) {
      return env->data[i].konst;
    }
  }

  return NULL;
}

static void const_env_remove(VecConstBinding *env, char *name)
{
  int write = 0;

  for (int read = 0; read < env->len; read++) {
    if (strcmp(env->data[read].name, name) == 0) {
      free(env->data[read].name);
      free_ir_val(env->data[read].konst);
      continue;
    }

    env->data[write++] = env->data[read];
  }

  env->len = write;
}

static void const_env_set(VecConstBinding *env, char *name,
                          struct IRValue *konst)
{
  const_env_remove(env, name);

  vec_insert(env, ((struct ConstBinding){
                      .name = strdup(name),
                      .konst = clone_irval(konst),
                  }));
}

static void const_env_clear(VecConstBinding *env)
{
  for (int i = 0; i < env->len; i++) {
    free(env->data[i].name);
    free_ir_val(env->data[i].konst);
  }

  env->len = 0;
}

static bool replace_const_use(VecConstBinding *env, struct IRValue **val)
{
  struct IRValue *konst;

  if (!*val || (*val)->kind != IRValue_VAR) {
    return false;
  }

  konst = const_env_get(env, (*val)->as.var);
  if (!konst) {
    return false;
  }

  free_ir_val(*val);
  *val = clone_irval(konst);

  return true;
}

static bool constant_propagate_ir(struct IRProgram *prog)
{
  bool changed = false;

  for (int i = 0; i < prog->funcs.len; i++) {
    struct IRFunction *fn = prog->funcs.data[i];
    VecConstBinding env = {0};

    for (int j = 0; j < fn->body.len; j++) {
      struct IRInstr *instr = &fn->body.data[j];

      switch (instr->kind) {
        case IRInstr_BIN: {
          changed |= replace_const_use(&env, &instr->as.binary.lhs);
          changed |= replace_const_use(&env, &instr->as.binary.rhs);

          if (instr->as.binary.dst &&
              instr->as.binary.dst->kind == IRValue_VAR) {
            const_env_remove(&env, instr->as.binary.dst->as.var);
          }

          break;
        }

        case IRInstr_UNARY: {
          changed |= replace_const_use(&env, &instr->as.unary.src);

          if (instr->as.unary.dst && instr->as.unary.dst->kind == IRValue_VAR) {
            const_env_remove(&env, instr->as.unary.dst->as.var);
          }

          break;
        }

        case IRInstr_CPY: {
          changed |= replace_const_use(&env, &instr->as.copy.src);

          if (instr->as.copy.dst && instr->as.copy.dst->kind == IRValue_VAR) {
            if (instr->as.copy.src &&
                instr->as.copy.src->kind == IRValue_CONST) {
              const_env_set(&env, instr->as.copy.dst->as.var,
                            instr->as.copy.src);
            } else {
              const_env_remove(&env, instr->as.copy.dst->as.var);
            }
          }

          break;
        }

        case IRInstr_RET: {
          changed |= replace_const_use(&env, &instr->as.ret.val);
          break;
        }
        case IRInstr_LBL:
        case IRInstr_JMP:
        case IRInstr_JZ:
        case IRInstr_CALL:
        case IRInstr_LOAD:
        case IRInstr_STORE:
        case IRInstr_GETADDR:
        case IRInstr_CAST:
        case IRInstr_CPY_TO_OFFSET:
        case IRInstr_CPY_FROM_OFFSET:
        case IRInstr_ADD_PTR: {
          const_env_clear(&env);
          break;
        }
        default:
          assert(0);
      }
    }

    const_env_clear(&env);
    vec_free(&env);
  }

  return changed;
}

static char *copy_env_get(VecCopyBinding *env, char *name)
{
  for (int i = env->len - 1; i >= 0; i--) {
    if (strcmp(env->data[i].dst, name) == 0) {
      return env->data[i].src;
    }
  }

  return NULL;
}

static char *copy_env_resolve(VecCopyBinding *env, char *name)
{
  char *current = name;

  /*
   * Follow chains:
   *
   *   c -> b
   *   b -> a
   *
   * so uses of c become a, not just b.
   *
   * The env->len bound prevents infinite loops if a bug ever creates a cycle.
   */
  for (int i = 0; i < env->len; i++) {
    char *next = copy_env_get(env, current);

    if (!next) {
      break;
    }

    current = next;
  }

  return current;
}

static void copy_env_remove_at(VecCopyBinding *env, int idx)
{
  free(env->data[idx].dst);
  free(env->data[idx].src);

  for (int i = idx; i < env->len - 1; i++) {
    env->data[i] = env->data[i + 1];
  }

  env->len--;
}

static void copy_env_kill_var(VecCopyBinding *env, char *name)
{
  /*
   * If `name` is assigned, remove:
   *
   *   name -> something
   *
   * and also remove bindings that depend on the old value of `name`:
   *
   *   something -> name
   */
  for (int i = 0; i < env->len;) {
    if (strcmp(env->data[i].dst, name) == 0 ||
        strcmp(env->data[i].src, name) == 0) {
      copy_env_remove_at(env, i);
    } else {
      i++;
    }
  }
}

static void copy_env_set(VecCopyBinding *env, char *dst, char *src)
{
  char *resolved_src;

  resolved_src = copy_env_resolve(env, src);

  copy_env_kill_var(env, dst);

  if (strcmp(dst, resolved_src) == 0) {
    return;
  }

  vec_insert(env, ((struct CopyBinding){
                      .dst = strdup(dst),
                      .src = strdup(resolved_src),
                  }));
}

static void copy_env_clear(VecCopyBinding *env)
{
  for (int i = 0; i < env->len; i++) {
    free(env->data[i].dst);
    free(env->data[i].src);
  }

  env->len = 0;
}

static bool replace_copy_use(VecCopyBinding *env, struct IRValue **val)
{
  char *replacement;
  struct IRValue *new_val;

  if (!*val || (*val)->kind != IRValue_VAR) {
    return false;
  }

  replacement = copy_env_resolve(env, (*val)->as.var);

  if (strcmp(replacement, (*val)->as.var) == 0) {
    return false;
  }

  new_val = malloc(sizeof(*new_val));
  new_val->kind = IRValue_VAR;
  new_val->type = clone_type((*val)->type);
  new_val->as.var = strdup(replacement);

  free_ir_val(*val);
  *val = new_val;

  return true;
}

static bool replace_copy_use_in_place(VecCopyBinding *env, struct IRValue *val)
{
  char *replacement;

  if (val->kind != IRValue_VAR) {
    return false;
  }

  replacement = copy_env_resolve(env, val->as.var);

  if (strcmp(replacement, val->as.var) == 0) {
    return false;
  }

  free(val->as.var);
  val->as.var = strdup(replacement);

  return true;
}

static bool copy_propagate_ir(struct IRProgram *prog)
{
  bool changed = false;

  for (int i = 0; i < prog->funcs.len; i++) {
    struct IRFunction *fn = prog->funcs.data[i];
    VecCopyBinding env = {0};

    for (int j = 0; j < fn->body.len; j++) {
      struct IRInstr *instr = &fn->body.data[j];

      switch (instr->kind) {
        case IRInstr_BIN: {
          changed |= replace_copy_use(&env, &instr->as.binary.lhs);
          changed |= replace_copy_use(&env, &instr->as.binary.rhs);

          if (instr->as.binary.dst &&
              instr->as.binary.dst->kind == IRValue_VAR) {
            copy_env_kill_var(&env, instr->as.binary.dst->as.var);
          }

          break;
        }

        case IRInstr_UNARY: {
          changed |= replace_copy_use(&env, &instr->as.unary.src);

          if (instr->as.unary.dst && instr->as.unary.dst->kind == IRValue_VAR) {
            copy_env_kill_var(&env, instr->as.unary.dst->as.var);
          }

          break;
        }

        case IRInstr_CPY: {
          changed |= replace_copy_use(&env, &instr->as.copy.src);

          if (instr->as.copy.dst && instr->as.copy.dst->kind == IRValue_VAR) {
            copy_env_kill_var(&env, instr->as.copy.dst->as.var);

            if (instr->as.copy.src && instr->as.copy.src->kind == IRValue_VAR &&
                strcmp(instr->as.copy.dst->as.var,
                       instr->as.copy.src->as.var) != 0) {
              copy_env_set(&env, instr->as.copy.dst->as.var,
                           instr->as.copy.src->as.var);
            }
          }

          break;
        }

        case IRInstr_RET: {
          changed |= replace_copy_use(&env, &instr->as.ret.val);
          break;
        }

        case IRInstr_CALL: {
          for (int k = 0; k < instr->as.call.args.len; k++) {
            changed |= replace_copy_use(&env, &instr->as.call.args.data[k]);
          }

          /*
           * Calls may mutate memory or globals, and the call destination is
           * newly assigned. Be conservative for now.
           */
          copy_env_clear(&env);

          if (instr->as.call.dst && instr->as.call.dst->kind == IRValue_VAR) {
            copy_env_kill_var(&env, instr->as.call.dst->as.var);
          }

          break;
        }

        case IRInstr_JZ: {
          changed |= replace_copy_use_in_place(&env, &instr->as.jz.cond);

          /*
           * This linear pass is not control-flow aware. Do not propagate facts
           * across branches yet.
           */
          copy_env_clear(&env);
          break;
        }

        case IRInstr_JMP:
        case IRInstr_LBL: {
          copy_env_clear(&env);
          break;
        }

        case IRInstr_CAST: {
          changed |= replace_copy_use(&env, &instr->as.cast.src);

          if (instr->as.cast.dst && instr->as.cast.dst->kind == IRValue_VAR) {
            copy_env_kill_var(&env, instr->as.cast.dst->as.var);
          }

          break;
        }

        case IRInstr_LOAD: {
          changed |= replace_copy_use(&env, &instr->as.load.src);

          if (instr->as.load.dst && instr->as.load.dst->kind == IRValue_VAR) {
            copy_env_kill_var(&env, instr->as.load.dst->as.var);
          }

          break;
        }

        case IRInstr_STORE: {
          changed |= replace_copy_use(&env, &instr->as.store.val);
          changed |= replace_copy_use(&env, &instr->as.store.dst);

          /*
           * Store may affect memory visible through aliases.
           * Conservative barrier for now.
           */
          copy_env_clear(&env);
          break;
        }

        case IRInstr_GETADDR: {
          /*
           * Do not rewrite getaddr src:
           *
           *   &x
           *
           * is not equivalent to:
           *
           *   &y
           *
           * just because x currently has y's value.
           */
          if (instr->as.getaddr.dst &&
              instr->as.getaddr.dst->kind == IRValue_VAR) {
            copy_env_kill_var(&env, instr->as.getaddr.dst->as.var);
          }

          /*
           * Taking an address makes later aliasing harder to reason about.
           */
          copy_env_clear(&env);
          break;
        }

        case IRInstr_ADD_PTR: {
          changed |= replace_copy_use(&env, &instr->as.add_ptr.ptr);
          changed |= replace_copy_use(&env, &instr->as.add_ptr.index);

          if (instr->as.add_ptr.dst &&
              instr->as.add_ptr.dst->kind == IRValue_VAR) {
            copy_env_kill_var(&env, instr->as.add_ptr.dst->as.var);
          }

          break;
        }

        case IRInstr_CPY_FROM_OFFSET: {
          /*
           * Conservative: aggregate/subobject identity matters, so don't
           * rewrite src yet.
           */
          if (instr->as.cpy_from_offset.dst &&
              instr->as.cpy_from_offset.dst->kind == IRValue_VAR) {
            copy_env_kill_var(&env, instr->as.cpy_from_offset.dst->as.var);
          }

          copy_env_clear(&env);
          break;
        }

        case IRInstr_CPY_TO_OFFSET: {
          changed |= replace_copy_use(&env, &instr->as.cpy_to_offset.src);

          /*
           * Mutates part of an aggregate. Be conservative.
           */
          copy_env_clear(&env);
          break;
        }

        default:
          assert(0);
      }
    }

    copy_env_clear(&env);
    vec_free(&env);
  }

  return changed;
}

static bool ir_name_set_contains(VecIRNameSet *set, char *name)
{
  for (int i = 0; i < set->len; i++) {
    if (strcmp(set->data[i], name) == 0) {
      return true;
    }
  }

  return false;
}

static bool ir_name_set_equal(VecIRNameSet *a, VecIRNameSet *b)
{
  if (a->len != b->len) {
    return false;
  }

  for (int i = 0; i < a->len; i++) {
    if (!ir_name_set_contains(b, a->data[i])) {
      return false;
    }
  }

  return true;
}

static void ir_name_set_add(VecIRNameSet *set, char *name)
{
  if (!name || ir_name_set_contains(set, name)) {
    return;
  }

  vec_insert(set, name);
}

static void ir_name_set_remove(VecIRNameSet *set, char *name)
{
  for (int i = 0; i < set->len; i++) {
    if (strcmp(set->data[i], name) == 0) {
      set->data[i] = set->data[set->len - 1];
      set->len--;
      return;
    }
  }
}

static void ir_name_set_copy(VecIRNameSet *dst, VecIRNameSet *src)
{
  dst->len = 0;

  for (int i = 0; i < src->len; i++) {
    ir_name_set_add(dst, src->data[i]);
  }
}

static void ir_name_set_union_into(VecIRNameSet *dst, VecIRNameSet *src)
{
  for (int i = 0; i < src->len; i++) {
    ir_name_set_add(dst, src->data[i]);
  }
}

static void ir_name_set_subtract(VecIRNameSet *dst, VecIRNameSet *to_remove)
{
  for (int i = 0; i < to_remove->len; i++) {
    ir_name_set_remove(dst, to_remove->data[i]);
  }
}

static void ir_name_set_free(VecIRNameSet *set)
{
  vec_free(set);
  set->capacity = 0;
  set->len = 0;
  set->data = NULL;
}

static void ir_int_set_add(VecInt *set, int value)
{
  for (int i = 0; i < set->len; i++) {
    if (set->data[i] == value) {
      return;
    }
  }

  vec_insert(set, value);
}

static int find_ir_label_instr(struct IRFunction *fn, char *label)
{
  for (int i = 0; i < fn->body.len; i++) {
    struct IRInstr *instr = &fn->body.data[i];

    if (instr->kind == IRInstr_LBL &&
        strcmp(instr->as.label.name, label) == 0) {
      return i;
    }
  }

  return -1;
}

int ir_block_after(struct IRCFG *cfg, int block_idx)
{
  int next = block_idx + 1;

  if (next >= cfg->block_count) {
    return -1;
  }

  return next;
}

static int ir_block_for_label(struct IRFunction *fn, struct IRCFG *cfg,
                              char *label)
{
  int instr_idx = find_ir_label_instr(fn, label);

  assert(instr_idx >= 0 && "IR jump target label not found");

  return cfg->instr_to_block[instr_idx];
}

static struct IRCFG build_ir_cfg(struct IRFunction *fn)
{
  int n = fn->body.len;
  bool *leader;
  struct IRCFG cfg = {0};

  if (n == 0) {
    return cfg;
  }

  leader = calloc(n, sizeof(*leader));
  if (!leader) {
    perror("calloc");
    exit(1);
  }

  leader[0] = true;

  /*
   * First pass: labels and instructions after terminators are leaders.
   */
  for (int i = 0; i < n; i++) {
    struct IRInstr *instr = &fn->body.data[i];

    if (instr->kind == IRInstr_LBL) {
      leader[i] = true;
    }

    if (instr->kind == IRInstr_JMP || instr->kind == IRInstr_JZ ||
        instr->kind == IRInstr_RET) {
      if (i + 1 < n) {
        leader[i + 1] = true;
      }
    }
  }

  /*
   * Second pass: jump targets are leaders.
   */
  for (int i = 0; i < n; i++) {
    struct IRInstr *instr = &fn->body.data[i];

    if (instr->kind == IRInstr_JMP) {
      int target_idx = find_ir_label_instr(fn, instr->as.jmp.target);
      assert(target_idx >= 0 && "IR JMP target not found");
      leader[target_idx] = true;
    } else if (instr->kind == IRInstr_JZ) {
      int target_idx = find_ir_label_instr(fn, instr->as.jz.target);
      assert(target_idx >= 0 && "IR JZ target not found");
      leader[target_idx] = true;
    }
  }

  for (int i = 0; i < n; i++) {
    if (leader[i]) {
      cfg.block_count++;
    }
  }

  cfg.blocks = calloc(cfg.block_count, sizeof(*cfg.blocks));
  cfg.instr_to_block = calloc(n, sizeof(*cfg.instr_to_block));

  if (!cfg.blocks || !cfg.instr_to_block) {
    perror("calloc");
    exit(1);
  }

  int bi = -1;

  for (int i = 0; i < n; i++) {
    if (leader[i]) {
      if (bi >= 0) {
        cfg.blocks[bi].end = i - 1;
      }

      bi++;
      cfg.blocks[bi].start = i;
    }

    cfg.instr_to_block[i] = bi;
  }

  cfg.blocks[bi].end = n - 1;

  /*
   * Compute CFG successors.
   */
  for (int b = 0; b < cfg.block_count; b++) {
    struct IRBasicBlock *block = &cfg.blocks[b];
    struct IRInstr *last = &fn->body.data[block->end];

    if (last->kind == IRInstr_JMP) {
      int target = ir_block_for_label(fn, &cfg, last->as.jmp.target);
      ir_int_set_add(&block->succs, target);
    } else if (last->kind == IRInstr_JZ) {
      int target = ir_block_for_label(fn, &cfg, last->as.jz.target);
      int fallthrough = ir_block_after(&cfg, b);

      ir_int_set_add(&block->succs, target);

      if (fallthrough >= 0) {
        ir_int_set_add(&block->succs, fallthrough);
      }
    } else if (last->kind == IRInstr_RET) {
      /*
       * No successors.
       */
    } else {
      int fallthrough = ir_block_after(&cfg, b);

      if (fallthrough >= 0) {
        ir_int_set_add(&block->succs, fallthrough);
      }
    }
  }

  free(leader);

  return cfg;
}

static void free_ir_cfg(struct IRCFG *cfg)
{
  for (int i = 0; i < cfg->block_count; i++) {
    vec_free(&cfg->blocks[i].succs);

    ir_name_set_free(&cfg->blocks[i].use);
    ir_name_set_free(&cfg->blocks[i].def);
    ir_name_set_free(&cfg->blocks[i].live_in);
    ir_name_set_free(&cfg->blocks[i].live_out);
  }

  free(cfg->blocks);
  free(cfg->instr_to_block);

  cfg->blocks = NULL;
  cfg->instr_to_block = NULL;
  cfg->block_count = 0;
}

static void ir_add_use_val(VecIRNameSet *use, struct IRValue *val)
{
  if (val && val->kind == IRValue_VAR) {
    ir_name_set_add(use, val->as.var);
  }
}

static void ir_add_def_val(VecIRNameSet *def, struct IRValue *val)
{
  if (val && val->kind == IRValue_VAR) {
    ir_name_set_add(def, val->as.var);
  }
}

static void compute_ir_instr_use_def(struct IRInstr *instr, VecIRNameSet *use,
                                     VecIRNameSet *def)
{
  switch (instr->kind) {
    case IRInstr_BIN:
      ir_add_use_val(use, instr->as.binary.lhs);
      ir_add_use_val(use, instr->as.binary.rhs);
      ir_add_def_val(def, instr->as.binary.dst);
      break;

    case IRInstr_UNARY:
      ir_add_use_val(use, instr->as.unary.src);
      ir_add_def_val(def, instr->as.unary.dst);
      break;

    case IRInstr_RET:
      ir_add_use_val(use, instr->as.ret.val);
      break;

    case IRInstr_CPY:
      ir_add_use_val(use, instr->as.copy.src);
      ir_add_def_val(def, instr->as.copy.dst);
      break;

    case IRInstr_CALL:
      for (int i = 0; i < instr->as.call.args.len; i++) {
        ir_add_use_val(use, instr->as.call.args.data[i]);
      }
      ir_add_def_val(def, instr->as.call.dst);
      break;

    case IRInstr_JMP:
      break;

    case IRInstr_JZ:
      if (instr->as.jz.cond.kind == IRValue_VAR) {
        ir_name_set_add(use, instr->as.jz.cond.as.var);
      }
      break;

    case IRInstr_LBL:
      break;

    case IRInstr_GETADDR:
      /*
       * &x uses x's storage identity.
       */
      ir_add_use_val(use, instr->as.getaddr.src);
      ir_add_def_val(def, instr->as.getaddr.dst);
      break;

    case IRInstr_LOAD:
      ir_add_use_val(use, instr->as.load.src);
      ir_add_def_val(def, instr->as.load.dst);
      break;

    case IRInstr_STORE:
      ir_add_use_val(use, instr->as.store.dst);
      ir_add_use_val(use, instr->as.store.val);
      break;

    case IRInstr_CAST:
      ir_add_use_val(use, instr->as.cast.src);
      ir_add_def_val(def, instr->as.cast.dst);
      break;

    case IRInstr_CPY_FROM_OFFSET:
      ir_add_use_val(use, instr->as.cpy_from_offset.src);
      ir_add_def_val(def, instr->as.cpy_from_offset.dst);
      break;

    case IRInstr_CPY_TO_OFFSET:
      /*
       * Partial aggregate update. Treat dst as used, not killed.
       * Old fields outside the written offset still matter.
       */
      ir_add_use_val(use, instr->as.cpy_to_offset.dst);
      ir_add_use_val(use, instr->as.cpy_to_offset.src);
      break;

    case IRInstr_ADD_PTR:
      ir_add_use_val(use, instr->as.add_ptr.ptr);
      ir_add_use_val(use, instr->as.add_ptr.index);
      ir_add_def_val(def, instr->as.add_ptr.dst);
      break;

    default:
      assert(0);
  }
}

static void compute_ir_block_use_def(struct IRBasicBlock *block,
                                     struct IRInstrLiveness *lv)
{
  for (int i = block->start; i <= block->end; i++) {
    /*
     * use[B]: values used before being defined in B.
     */
    for (int j = 0; j < lv[i].use.len; j++) {
      char *name = lv[i].use.data[j];

      if (!ir_name_set_contains(&block->def, name)) {
        ir_name_set_add(&block->use, name);
      }
    }

    /*
     * def[B]: values defined in B.
     */
    for (int j = 0; j < lv[i].def.len; j++) {
      ir_name_set_add(&block->def, lv[i].def.data[j]);
    }
  }
}

static void solve_ir_block_liveness(struct IRCFG *cfg)
{
  bool changed = true;

  while (changed) {
    changed = false;

    for (int bi = cfg->block_count - 1; bi >= 0; bi--) {
      struct IRBasicBlock *block = &cfg->blocks[bi];

      VecIRNameSet new_live_out = {0};
      VecIRNameSet new_live_in = {0};

      /*
       * live_out[B] = union(live_in[S]) for successors S.
       */
      for (int i = 0; i < block->succs.len; i++) {
        int succ = block->succs.data[i];
        ir_name_set_union_into(&new_live_out, &cfg->blocks[succ].live_in);
      }

      /*
       * live_in[B] = use[B] union (live_out[B] - def[B]).
       */
      ir_name_set_copy(&new_live_in, &new_live_out);
      ir_name_set_subtract(&new_live_in, &block->def);
      ir_name_set_union_into(&new_live_in, &block->use);

      if (!ir_name_set_equal(&block->live_out, &new_live_out) ||
          !ir_name_set_equal(&block->live_in, &new_live_in)) {
        changed = true;
      }

      ir_name_set_free(&block->live_out);
      ir_name_set_free(&block->live_in);

      block->live_out = new_live_out;
      block->live_in = new_live_in;
    }
  }
}

static void compute_ir_instr_liveness_from_blocks(struct IRCFG *cfg,
                                                  struct IRInstrLiveness *lv)
{
  for (int bi = 0; bi < cfg->block_count; bi++) {
    struct IRBasicBlock *block = &cfg->blocks[bi];

    VecIRNameSet live = {0};
    ir_name_set_copy(&live, &block->live_out);

    for (int i = block->end; i >= block->start; i--) {
      VecIRNameSet before = {0};

      ir_name_set_copy(&lv[i].live_after, &live);

      ir_name_set_copy(&before, &live);
      ir_name_set_subtract(&before, &lv[i].def);
      ir_name_set_union_into(&before, &lv[i].use);

      ir_name_set_copy(&lv[i].live_before, &before);

      ir_name_set_free(&live);
      live = before;
    }

    ir_name_set_free(&live);
  }
}

static struct IRInstrLiveness *compute_ir_liveness_cfg(struct IRFunction *fn)
{
  int n = fn->body.len;

  struct IRInstrLiveness *lv = calloc(n, sizeof(*lv));
  if (!lv) {
    perror("calloc");
    exit(1);
  }

  for (int i = 0; i < n; i++) {
    compute_ir_instr_use_def(&fn->body.data[i], &lv[i].use, &lv[i].def);
  }

  struct IRCFG cfg = build_ir_cfg(fn);

  for (int bi = 0; bi < cfg.block_count; bi++) {
    compute_ir_block_use_def(&cfg.blocks[bi], lv);
  }

  solve_ir_block_liveness(&cfg);
  compute_ir_instr_liveness_from_blocks(&cfg, lv);

  free_ir_cfg(&cfg);

  return lv;
}

static void free_ir_liveness(struct IRInstrLiveness *lv, int n)
{
  for (int i = 0; i < n; i++) {
    ir_name_set_free(&lv[i].use);
    ir_name_set_free(&lv[i].def);
    ir_name_set_free(&lv[i].live_before);
    ir_name_set_free(&lv[i].live_after);
  }

  free(lv);
}

static struct IRValue *ir_instr_dst(struct IRInstr *instr)
{
  switch (instr->kind) {
    case IRInstr_BIN:
      return instr->as.binary.dst;
    case IRInstr_UNARY:
      return instr->as.unary.dst;
    case IRInstr_CPY:
      return instr->as.copy.dst;
    case IRInstr_CALL:
      return instr->as.call.dst;
    case IRInstr_GETADDR:
      return instr->as.getaddr.dst;
    case IRInstr_LOAD:
      return instr->as.load.dst;
    case IRInstr_CAST:
      return instr->as.cast.dst;
    case IRInstr_CPY_FROM_OFFSET:
      return instr->as.cpy_from_offset.dst;
    case IRInstr_ADD_PTR:
      return instr->as.add_ptr.dst;

    case IRInstr_RET:
    case IRInstr_JMP:
    case IRInstr_JZ:
    case IRInstr_LBL:
    case IRInstr_STORE:
    case IRInstr_CPY_TO_OFFSET:
      return NULL;

    default:
      assert(0);
  }

  return NULL;
}

static bool ir_instr_is_removable_dead_def(struct IRInstr *instr)
{
  switch (instr->kind) {
    case IRInstr_BIN:
    case IRInstr_UNARY:
    case IRInstr_CPY:
    case IRInstr_CAST:
    case IRInstr_GETADDR:
    case IRInstr_CPY_FROM_OFFSET:
    case IRInstr_ADD_PTR:
      return true;

    /*
     * Keep these:
     * - CALL may have side effects.
     * - STORE and CPY_TO_OFFSET mutate memory/aggregates.
     * - LOAD may trap or observe memory; can be relaxed later if desired.
     * - control-flow instructions must stay.
     */
    case IRInstr_CALL:
    case IRInstr_LOAD:
    case IRInstr_STORE:
    case IRInstr_RET:
    case IRInstr_JMP:
    case IRInstr_JZ:
    case IRInstr_LBL:
    case IRInstr_CPY_TO_OFFSET:
      return false;

    default:
      assert(0);
  }

  return false;
}

static void remove_ir_instr_at(VecIRInstr *instrs, int idx)
{
  free_ir_instr(&instrs->data[idx]);

  for (int i = idx; i < instrs->len - 1; i++) {
    instrs->data[i] = instrs->data[i + 1];
  }

  instrs->len--;
}

static bool dead_store_eliminate_ir(struct IRProgram *prog)
{
  bool changed = false;

  for (int fi = 0; fi < prog->funcs.len; fi++) {
    struct IRFunction *fn = prog->funcs.data[fi];
    int old_len = fn->body.len;

    if (old_len == 0) {
      continue;
    }

    struct IRInstrLiveness *lv = compute_ir_liveness_cfg(fn);

    /*
     * Remove from back to front so old liveness indices remain valid for
     * instructions we have not visited yet.
     */
    for (int i = old_len - 1; i >= 0; i--) {
      struct IRInstr *instr = &fn->body.data[i];
      struct IRValue *dst = ir_instr_dst(instr);

      if (!dst || dst->kind != IRValue_VAR) {
        continue;
      }

      if (!ir_instr_is_removable_dead_def(instr)) {
        continue;
      }

      if (!ir_name_set_contains(&lv[i].live_after, dst->as.var)) {
        remove_ir_instr_at(&fn->body, i);
        changed = true;
      }
    }

    free_ir_liveness(lv, old_len);
  }

  return changed;
}

static void free_embedded_ir_val(struct IRValue *val)
{
  if (val->kind == IRValue_VAR) {
    free(val->as.var);
  }
}

static bool ir_const_is_zero(struct IRValue *val, bool *out_is_zero)
{
  if (!val || val->kind != IRValue_CONST) {
    return false;
  }

  switch (val->type.kind) {
    case BOOL_T:
      *out_is_zero = !val->as.konst.as.boolean;
      return true;

    case I8_T:
      *out_is_zero = val->as.konst.as.i8 == 0;
      return true;
    case U8_T:
      *out_is_zero = val->as.konst.as.u8 == 0;
      return true;

    case I16_T:
      *out_is_zero = val->as.konst.as.i16 == 0;
      return true;
    case U16_T:
      *out_is_zero = val->as.konst.as.u16 == 0;
      return true;

    case I32_T:
      *out_is_zero = val->as.konst.as.i32 == 0;
      return true;
    case U32_T:
      *out_is_zero = val->as.konst.as.u32 == 0;
      return true;

    case I64_T:
      *out_is_zero = val->as.konst.as.i64 == 0;
      return true;
    case U64_T:
      *out_is_zero = val->as.konst.as.u64 == 0;
      return true;

    case F32_T:
      *out_is_zero = val->as.konst.as.f32 == 0.0f;
      return true;
    case F64_T:
      *out_is_zero = val->as.konst.as.f64 == 0.0;
      return true;

    default:
      return false;
  }
}

static bool simplify_constant_ir_branches(struct IRFunction *fn)
{
  bool changed = false;

  for (int i = 0; i < fn->body.len; i++) {
    struct IRInstr *instr = &fn->body.data[i];

    if (instr->kind != IRInstr_JZ) {
      continue;
    }

    bool is_zero;
    if (!ir_const_is_zero(&instr->as.jz.cond, &is_zero)) {
      continue;
    }

    if (is_zero) {
      /*
       * JZ const_zero, target
       *
       * Always jumps, so rewrite to:
       *
       * JMP target
       */
      char *target = instr->as.jz.target;

      free_embedded_ir_val(&instr->as.jz.cond);

      instr->kind = IRInstr_JMP;
      instr->as.jmp.target = target;

      changed = true;
    } else {
      /*
       * JZ const_nonzero, target
       *
       * Never jumps, so delete it.
       */
      remove_ir_instr_at(&fn->body, i);
      i--;

      changed = true;
    }
  }

  return changed;
}

static void mark_reachable_ir_blocks(struct IRCFG *cfg, int block_idx,
                                     bool *reachable)
{
  if (block_idx < 0 || block_idx >= cfg->block_count) {
    return;
  }

  if (reachable[block_idx]) {
    return;
  }

  reachable[block_idx] = true;

  for (int i = 0; i < cfg->blocks[block_idx].succs.len; i++) {
    mark_reachable_ir_blocks(cfg, cfg->blocks[block_idx].succs.data[i],
                             reachable);
  }
}

static bool unreachable_code_eliminate_ir(struct IRProgram *prog)
{
  bool changed = false;

  for (int fi = 0; fi < prog->funcs.len; fi++) {
    struct IRFunction *fn = prog->funcs.data[fi];

    changed |= simplify_constant_ir_branches(fn);

    if (fn->body.len == 0) {
      continue;
    }

    struct IRCFG cfg = build_ir_cfg(fn);

    if (cfg.block_count == 0) {
      free_ir_cfg(&cfg);
      continue;
    }

    bool *reachable_blocks = calloc(cfg.block_count, sizeof(*reachable_blocks));
    bool *reachable_instrs = calloc(fn->body.len, sizeof(*reachable_instrs));

    if (!reachable_blocks || !reachable_instrs) {
      perror("calloc");
      exit(1);
    }

    mark_reachable_ir_blocks(&cfg, 0, reachable_blocks);

    for (int bi = 0; bi < cfg.block_count; bi++) {
      if (!reachable_blocks[bi]) {
        continue;
      }

      for (int i = cfg.blocks[bi].start; i <= cfg.blocks[bi].end; i++) {
        reachable_instrs[i] = true;
      }
    }

    for (int i = fn->body.len - 1; i >= 0; i--) {
      if (!reachable_instrs[i]) {
        remove_ir_instr_at(&fn->body, i);
        changed = true;
      }
    }

    free(reachable_blocks);
    free(reachable_instrs);
    free_ir_cfg(&cfg);
  }

  return changed;
}

static bool constant_fold_ir(struct IRProgram *prog)
{
  bool changed = false;

  for (int i = 0; i < prog->funcs.len; i++) {
    struct IRFunction *fn = prog->funcs.data[i];

    for (int j = 0; j < fn->body.len; j++) {
      struct IRInstr *instr = &fn->body.data[j];

      if (instr->kind == IRInstr_BIN &&
          instr->as.binary.lhs->kind == IRValue_CONST &&
          instr->as.binary.rhs->kind == IRValue_CONST) {
        struct IRValue *folded =
            fold_binary_const(instr->as.binary.kind, instr->as.binary.lhs,
                              instr->as.binary.rhs, instr->as.binary.dst->type);

        if (folded) {
          struct IRValue *dst = instr->as.binary.dst;

          free_ir_val(instr->as.binary.lhs);
          free_ir_val(instr->as.binary.rhs);

          instr->kind = IRInstr_CPY;
          instr->as.copy.src = folded;
          instr->as.copy.dst = dst;

          changed = true;
        }
      }

      if (instr->kind == IRInstr_UNARY &&
          instr->as.unary.src->kind == IRValue_CONST) {
        struct IRValue *folded =
            fold_unary_const(instr->as.unary.kind, instr->as.unary.src,
                             instr->as.unary.dst->type);

        if (folded) {
          struct IRValue *dst = instr->as.unary.dst;

          free_ir_val(instr->as.unary.src);

          instr->kind = IRInstr_CPY;
          instr->as.copy.src = folded;
          instr->as.copy.dst = dst;

          changed = true;
        }
      }
    }
  }
  return changed;
}

static bool optimize_ir_once(struct IRProgram *prog)
{
  bool changed = false;

  changed |= constant_propagate_ir(prog);
  changed |= constant_fold_ir(prog);
  changed |= copy_propagate_ir(prog);
  changed |= unreachable_code_eliminate_ir(prog);
  changed |= dead_store_eliminate_ir(prog);

  return changed;
}

void optimize_ir(struct IRProgram *prog)
{
  int pass_count = 0;

  while (optimize_ir_once(prog)) {
    pass_count++;

    if (pass_count > MAX_OPTIMIZATION_PASSES) {
      fprintf(stderr, "optimization did not converge\n");
      exit(1);
    }
  }
}
