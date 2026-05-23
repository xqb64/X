#ifndef MINI_COMPILER_IR_OPT_H
#define MINI_COMPILER_IR_OPT_H

#include "common.h"

/* ir_opt */

bool is_int_type(enum TypeKind kind);
unsigned long long const_as_u64(struct IRValue *v);
long long const_as_i64(struct IRValue *v);
struct IRValue *mk_const_bool(bool b);
struct IRValue *mk_const_int(unsigned long long n, Type type);
struct IRValue *fold_binary_const(enum IRInstrBinaryKind kind,
                                         struct IRValue *lhs,
                                         struct IRValue *rhs, Type dst_type);
struct IRValue *fold_unary_const(enum IRInstrUnaryKind kind,
                                        struct IRValue *src, Type dst_type);
struct IRValue *const_env_get(VecConstBinding *env, char *name);
void const_env_remove(VecConstBinding *env, char *name);
void const_env_set(VecConstBinding *env, char *name,
                          struct IRValue *konst);
void const_env_clear(VecConstBinding *env);
bool replace_const_use(VecConstBinding *env, struct IRValue **val);
bool constant_propagate_ir(struct IRProgram *prog);
char *copy_env_get(VecCopyBinding *env, char *name);
char *copy_env_resolve(VecCopyBinding *env, char *name);
void copy_env_remove_at(VecCopyBinding *env, int idx);
void copy_env_kill_var(VecCopyBinding *env, char *name);
void copy_env_set(VecCopyBinding *env, char *dst, char *src);
void copy_env_clear(VecCopyBinding *env);
bool replace_copy_use(VecCopyBinding *env, struct IRValue **val);
bool replace_copy_use_in_place(VecCopyBinding *env, struct IRValue *val);
bool copy_propagate_ir(struct IRProgram *prog);
bool ir_name_set_contains(VecIRNameSet *set, char *name);
bool ir_name_set_equal(VecIRNameSet *a, VecIRNameSet *b);
void ir_name_set_add(VecIRNameSet *set, char *name);
void ir_name_set_remove(VecIRNameSet *set, char *name);
void ir_name_set_copy(VecIRNameSet *dst, VecIRNameSet *src);
void ir_name_set_union_into(VecIRNameSet *dst, VecIRNameSet *src);
void ir_name_set_subtract(VecIRNameSet *dst, VecIRNameSet *to_remove);
void ir_name_set_free(VecIRNameSet *set);
void ir_int_set_add(VecInt *set, int value);
int find_ir_label_instr(struct IRFunction *fn, char *label);
int ir_block_after(struct IRCFG *cfg, int block_idx);
int ir_block_for_label(struct IRFunction *fn, struct IRCFG *cfg,
                              char *label);
struct IRCFG build_ir_cfg(struct IRFunction *fn);
void free_ir_cfg(struct IRCFG *cfg);
void ir_add_use_val(VecIRNameSet *use, struct IRValue *val);
void ir_add_def_val(VecIRNameSet *def, struct IRValue *val);
void compute_ir_instr_use_def(struct IRInstr *instr, VecIRNameSet *use,
                                     VecIRNameSet *def);
void compute_ir_block_use_def(struct IRFunction *fn,
                                     struct IRBasicBlock *block,
                                     struct IRInstrLiveness *lv);
void solve_ir_block_liveness(struct IRCFG *cfg);
void compute_ir_instr_liveness_from_blocks(struct IRFunction *fn,
                                                  struct IRCFG *cfg,
                                                  struct IRInstrLiveness *lv);
struct IRInstrLiveness *compute_ir_liveness_cfg(struct IRFunction *fn);
void free_ir_liveness(struct IRInstrLiveness *lv, int n);
struct IRValue *ir_instr_dst(struct IRInstr *instr);
bool ir_instr_is_removable_dead_def(struct IRInstr *instr);
void remove_ir_instr_at(VecIRInstr *instrs, int idx);
void free_embedded_ir_val(struct IRValue *val);
bool ir_const_is_zero(struct IRValue *val, bool *out_is_zero);
bool simplify_constant_ir_branches(struct IRFunction *fn);
void mark_reachable_ir_blocks(struct IRCFG *cfg, int block_idx,
                                     bool *reachable);
bool optimize_ir_once(struct IRProgram *prog);
void optimize_ir(struct IRProgram *prog);

#endif /* MINI_COMPILER_IR_OPT_H */
