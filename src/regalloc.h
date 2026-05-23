#ifndef MINI_COMPILER_REGALLOC_H
#define MINI_COMPILER_REGALLOC_H

#include "common.h"

/* regalloc */

bool map_get_offset(struct Map *map, char *name, int *out_offset);
int get_offset(struct Map *map, char *name, struct AsmType asm_type,
               int *used_stack_bytes);
void replace_operand_pseudo(struct AsmOperand *op,
                                          struct Map *map,
                                          int *used_stack_bytes);
bool is_tmp(char *name);
struct AsmType *pseudo_type_get(VecPseudoType *types, char *pseudo);
void pseudo_type_add(VecPseudoType *types, char *pseudo,
                            struct AsmType asm_type);
void remember_operand_type(VecPseudoType *types, struct AsmOperand *op);
VecPseudoType collect_pseudo_types(struct AsmFunction *fn);
void free_pseudo_types(VecPseudoType *types);
int find_live_interval(VecLiveInterval *intervals, char *pseudo);
void touch_live_interval(VecLiveInterval *intervals, char *pseudo,
                                struct AsmType asm_type, int instr_idx);
void touch_pseudo_set(VecLiveInterval *intervals, VecPseudoType *types,
                             VecPseudo *set, int instr_idx);
VecLiveInterval build_live_intervals(struct InstrLiveness *lv, int n,
                                            VecPseudoType *types);
int compare_live_intervals(const void *a, const void *b);
void sort_live_intervals(VecLiveInterval *intervals);
void print_live_intervals(VecLiveInterval *intervals);
void free_live_intervals(VecLiveInterval *intervals);
void init_regalloc_state(struct RegAllocState *state);
void free_regalloc_state(struct RegAllocState *state);
int allocatable_reg_index(enum AsmRegister reg);
void free_reg(struct RegAllocState *state, enum AsmRegister reg);
void int_set_add(VecInt *set, int value);
int find_label_instr(struct AsmFunction *fn, char *label);
int block_after(struct CFG *cfg, int block_idx);
bool pseudo_set_contains(VecPseudo *set, char *name);
bool pseudo_set_equal(VecPseudo *a, VecPseudo *b);
void pseudo_set_add(VecPseudo *set, char *name);
void pseudo_set_copy(VecPseudo *dst, VecPseudo *src);
void pseudo_set_union_into(VecPseudo *dst, VecPseudo *src);
void pseudo_set_remove(VecPseudo *set, char *name);
void pseudo_set_subtract(VecPseudo *dst, VecPseudo *to_remove);
void pseudo_set_free(VecPseudo *set);
void pseudo_set_print(VecPseudo *set);
int block_for_label(struct AsmFunction *fn, struct CFG *cfg, char *label);
void dot_escape(FILE *out, const char *s);
const char *asm_instr_kind_name(enum AsmInstrKind kind);
void dot_print_pseudo_set(FILE *out, VecPseudo *set);
struct CFG build_cfg(struct AsmFunction *fn);
void compute_block_use_def(struct AsmFunction *fn,
                                  struct BasicBlock *block,
                                  struct InstrLiveness *lv);
void solve_block_liveness(struct CFG *cfg);
void dot_print_instr_summary(FILE *out, int idx, struct AsmInstr *instr);
void dump_cfg_dot(FILE *out, struct AsmFunction *fn, struct CFG *cfg,
                         struct InstrLiveness *lv);
void free_cfg(struct CFG *cfg);
void write_cfg_dot(struct AsmFunction *fn, struct InstrLiveness *lv,
                          const char *path);
void release_dead_regs(struct RegAllocState *state,
                              VecPseudo *live_after);
bool alloc_reg(struct RegAllocState *state, enum AsmRegister *out_reg);
void allocated_reg_append(struct RegAllocState *state, char *pseudo,
                          enum AsmRegister reg);
bool allocated_reg_get(struct RegAllocState *state, char *pseudo,
                       enum AsmRegister *out_reg);
void add_use_operand(VecPseudo *use, struct AsmOperand *op);
void add_def_operand(VecPseudo *def, struct AsmOperand *op);
void compute_instr_use_def(struct AsmInstr *instr, VecPseudo *use,
                                  VecPseudo *def);
void compute_instr_liveness_from_blocks(struct AsmFunction *fn,
                                               struct CFG *cfg,
                                               struct InstrLiveness *lv);
struct InstrLiveness *compute_liveness_cfg(struct AsmFunction *fn);
void free_liveness(struct InstrLiveness *lv, int n);
void print_liveness(struct AsmFunction *fn, struct InstrLiveness *lv);
struct PseudoHome *pseudo_home_get(VecPseudoHome *homes, char *pseudo);
void pseudo_home_add_reg(VecPseudoHome *homes, char *pseudo,
                                enum AsmRegister reg);
void pseudo_home_add_stack(VecPseudoHome *homes, char *pseudo,
                                  int stack_offset);
void free_pseudo_homes(VecPseudoHome *homes);
int compare_active_intervals_by_end(const void *a, const void *b);
void sort_active_intervals(VecActiveInterval *active);
void active_remove_at(VecActiveInterval *active, int idx);
bool fixed_reg_is_used(VecActiveInterval *active, enum AsmRegister reg);
bool fixed_find_free_reg(VecActiveInterval *active,
                                enum AsmRegister *out_reg);
void expire_old_intervals(VecActiveInterval *active, int start);
VecPseudoHome allocate_pseudo_homes(VecLiveInterval *intervals,
                                           struct Map *map,
                                           int *used_stack_bytes,
                                           struct AsmFunction *fn);
int interference_node_index(struct InterferenceGraph *graph,
                                   char *pseudo);
int interference_add_node(struct InterferenceGraph *graph, char *pseudo);
bool int_vec_contains(VecInt *vec, int value);
char *precolored_reg_name(enum AsmRegister reg);
int interference_add_precolored_reg(struct InterferenceGraph *graph,
                                           enum AsmRegister reg);
void add_precolored_register_nodes(struct InterferenceGraph *graph);
void interference_add_edge_by_index(struct InterferenceGraph *graph,
                                           int ai, int bi);
void interference_add_edge(struct InterferenceGraph *graph, char *a,
                                  char *b);
#define NUM_CALLEE_SAVED_INT_REGS \
  ((int) (sizeof(callee_saved_int_regs) / sizeof(callee_saved_int_regs[0])))

void interference_add_nodes_from_set(struct InterferenceGraph *graph,
                                            VecPseudo *set);
char *mov_pseudo_src(struct AsmInstr *instr);
void add_interferences_for_instr(struct InterferenceGraph *graph,
                                        struct AsmInstr *instr,
                                        struct InstrLiveness *lv);
bool is_abi_int_arg_reg(enum AsmRegister reg);
bool is_abi_sse_arg_reg(enum AsmRegister reg);
bool is_abi_arg_reg(enum AsmRegister reg);
bool same_reg_class(enum AsmRegister a, enum AsmRegister b);
void add_abi_param_copy_interference(struct InterferenceGraph *graph,
                                            struct AsmFunction *fn);
void print_interference_graph(struct InterferenceGraph *graph);
void dump_interference_graph_dot(FILE *out,
                                        struct InterferenceGraph *graph);
void write_interference_graph_dot(struct InterferenceGraph *graph,
                                         const char *path);
void free_interference_graph(struct InterferenceGraph *graph);
bool pseudo_is_global_constant(char *pseudo);
bool convert_global_constant_operand(struct AsmOperand *op);
void regalloc_addr_op_from_homes(struct AsmOperand *op,
                                        VecPseudoHome *homes, struct Map *map,
                                        int *used_stack_bytes);
void regalloc_op_from_homes(struct AsmOperand *op, VecPseudoHome *homes,
                                   struct Map *map, int *used_stack_bytes);
struct AsmInstr *regalloc_instr_from_homes(struct AsmInstr *instr,
                                                  VecPseudoHome *homes,
                                                  struct Map *map,
                                                  int *used_stack_bytes);
bool reg_is_sse(enum AsmRegister reg);
enum RegClass reg_class_of_reg(enum AsmRegister reg);
int allocatable_reg_count(enum RegClass cls);
enum AsmRegister allocatable_reg_at(enum RegClass cls, int i);
int allocatable_reg_index_in_class(enum RegClass cls,
                                          enum AsmRegister reg);
struct SpillCost *spill_cost_get(VecSpillCost *costs, char *pseudo);
void spill_cost_add(VecSpillCost *costs, char *pseudo, double amount);
void free_spill_costs(VecSpillCost *costs);
VecSpillCost compute_spill_costs(struct InstrLiveness *lv,
                                        int instr_count);
double spill_cost_of(VecSpillCost *costs, char *pseudo);
char *fresh_spill_tmp(void);
bool is_spill_tmp_name(char *pseudo);
enum RegClass asm_type_reg_class(struct AsmType type);
enum RegClass graph_node_reg_class(struct InterferenceGraph *graph,
                                          VecPseudoType *types, int idx);
int graph_degree_after_removal(struct InterferenceGraph *graph,
                                      VecPseudoType *types, bool *removed,
                                      int node_idx);
int pick_low_degree_node(struct InterferenceGraph *graph,
                                VecPseudoType *types, bool *removed,
                                VecSpillCost *spill_costs);
double spill_score_for_node(struct InterferenceGraph *graph,
                                   VecPseudoType *types, bool *removed,
                                   int node_idx, VecSpillCost *spill_costs);
int pick_spill_candidate(struct InterferenceGraph *graph,
                                VecPseudoType *types, bool *removed,
                                VecSpillCost *spill_costs);
bool choose_color_for_node(struct InterferenceGraph *graph,
                                  VecPseudoType *types, VecPseudoHome *homes,
                                  int node_idx, enum AsmRegister *out_reg);
void assign_stack_home(VecPseudoHome *homes, VecPseudoType *types,
                              struct Map *map, int *used_stack_bytes,
                              char *pseudo);
VecPseudoHome color_interference_graph(
    struct InterferenceGraph *graph, VecPseudoType *types,
    VecPseudo *force_stack, VecPseudo *out_spilled, VecSpillCost *spill_costs,
    struct Map *map, int *used_stack_bytes);
void print_pseudo_homes(VecPseudoHome *homes);
void verify_coloring(struct InterferenceGraph *graph,
                            VecPseudoHome *homes);
char *move_node_name(struct InterferenceGraph *graph, int idx);
bool interference_has_edge_idx(struct InterferenceGraph *graph,
                                      int a_idx, int b_idx);
bool move_operand_node(struct InterferenceGraph *graph,
                              struct AsmOperand *op, int *out_idx);
bool is_pseudo_to_pseudo_move(struct AsmInstr *instr, char **out_src,
                                     char **out_dst);
VecMove collect_moves_from_graph(struct AsmFunction *fn,
                                        struct InterferenceGraph *graph);
void print_moves(struct InterferenceGraph *graph, VecMove *moves);
void free_moves(VecMove *moves);
void dump_moves_dot(FILE *out, struct InterferenceGraph *graph,
                           VecMove *moves);
void write_moves_dot(struct InterferenceGraph *graph, VecMove *moves,
                            const char *path);
bool is_same_reg_operand(struct AsmOperand *a, struct AsmOperand *b);
bool is_redundant_mov(struct AsmInstr *instr);
bool interference_has_edge(struct InterferenceGraph *graph, char *a,
                                  char *b);
int interference_degree(struct InterferenceGraph *graph, char *pseudo);
bool briggs_can_coalesce(struct InterferenceGraph *graph, char *a,
                                char *b, int k);
int aliased_index_for_orig_rep(struct InterferenceGraph *aliased,
                                      struct InterferenceGraph *orig,
                                      int orig_rep_idx);
int uf_find(int *parent, int x);
void uf_union(int *parent, int a, int b);
bool george_ok(struct InterferenceGraph *graph, int t_idx, int r_idx,
                      int k);
bool george_can_coalesce(struct InterferenceGraph *graph, int pseudo_idx,
                                int precolored_idx, int k);
void uf_union_into(int *parent, int keep, int discard);
void debug_briggs_moves(struct InterferenceGraph *graph, VecMove *moves);
bool asm_types_coalesce_compatible(struct AsmType a, struct AsmType b);
struct InterferenceGraph build_aliased_interference_graph(
    struct InterferenceGraph *orig, int *parent);
int *coalesce_briggs_george(struct InterferenceGraph *orig,
                                   VecPseudoType *types, VecMove *moves);
VecPseudoHome expand_coalesced_homes(struct InterferenceGraph *orig,
                                            int *parent,
                                            VecPseudoHome *coalesced_homes);
bool same_asm_operand(struct AsmOperand *a, struct AsmOperand *b);
void remove_redundant_moves(struct AsmFunction *fn);
bool instr_is_call(struct AsmInstr *instr);
bool pseudo_set_any_contains(VecPseudo *set, char *pseudo);
VecPseudo collect_call_live_pseudos(struct AsmFunction *fn,
                                           struct InstrLiveness *lv);
bool is_callee_saved_reg(enum AsmRegister reg);
bool reg_vec_contains(VecInt *regs, enum AsmRegister reg);
void reg_vec_add(VecInt *regs, enum AsmRegister reg);
VecInt collect_used_callee_saved_regs(VecPseudoHome *homes);
struct AsmOperand make_reg_operand(enum AsmRegister reg,
                                          struct AsmType asm_type);
struct AsmOperand make_pseudo_operand(char *pseudo,
                                             struct AsmType asm_type);
struct AsmOperand make_stack_operand(int offset, struct AsmType asm_type);
struct AsmInstr make_mov_instr(struct AsmOperand src,
                                      struct AsmOperand dst,
                                      struct AsmType asm_type);
bool is_spilled_pseudo_operand(struct AsmOperand *op, VecPseudo *spilled);
struct AsmInstr make_push_reg(enum AsmRegister reg);
struct AsmInstr make_pop_reg(enum AsmRegister reg);
bool is_reg_operand(struct AsmOperand *op, enum AsmRegister reg);
bool is_restore_sp_from_bp(struct AsmInstr *instr);
bool is_pop_bp(struct AsmInstr *instr);
bool is_stack_alloc_instr(struct AsmInstr *instr);
void save_restore_callee_saved_regs(struct AsmFunction *fn,
                                           VecInt *used_regs);
void patch_stack_alloc(struct AsmFunction *fn, int stack_size);
void rewrite_spilled_use(VecAsmInstr *before, struct AsmOperand *op,
                                VecPseudo *spilled, struct Map *map,
                                int *used_stack_bytes);
void rewrite_spilled_def(VecAsmInstr *after, struct AsmOperand *op,
                                VecPseudo *spilled, struct Map *map,
                                int *used_stack_bytes);
void rewrite_spilled_use_def(VecAsmInstr *before, VecAsmInstr *after,
                                    struct AsmOperand *op, VecPseudo *spilled,
                                    struct Map *map, int *used_stack_bytes);
void append_instrs(VecAsmInstr *dst, VecAsmInstr *src);
void rewrite_spills(struct AsmFunction *fn, VecPseudo *spilled,
                           struct Map *map, int *used_stack_bytes);
void print_spilled_pseudos(VecPseudo *spilled);
#define MAX_REGALLOC_ATTEMPTS 20

VecPseudo expand_spilled_reps_to_original_pseudos(
    struct InterferenceGraph *orig, int *parent, VecPseudo *spilled_reps);
void add_call_clobber_interference(struct InterferenceGraph *graph,
                                          VecPseudoType *types,
                                          struct InstrLiveness *lv);
struct InterferenceGraph build_interference_graph(
    struct AsmFunction *fn, VecPseudoType *types, struct InstrLiveness *lv);
struct AsmFunction *regalloc_fn(struct AsmFunction *fn, struct Map *map,
                                int *used_stack_bytes,
                                int *used_callee_saved_count);
int compute_frame_stack_adjustment(int local_stack_bytes,
                                          int used_callee_saved_count);
struct AsmProgram *regalloc(struct AsmProgram *asmcode);

#endif /* MINI_COMPILER_REGALLOC_H */
