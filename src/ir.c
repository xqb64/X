#include "ir.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "typechecker.h"
#include "util.h"

VecStaticConstant global_constants = {0};
VecStaticConstant global_variables = {0};
VecExternVariable extern_variables = {0};

static struct ExpResult irfy_expr(VecIRInstr *instrs, struct Expr *expr);

struct IRValue *clone_irval(struct IRValue *v)
{
  if (!v) {
    return NULL;
  }

  struct IRValue *clone;

  clone = malloc(sizeof(struct IRValue));
  clone->kind = v->kind;
  clone->type = clone_type(v->type);

  if (v->kind == IRValue_VAR) {
    clone->as.var = strdup(v->as.var);
  } else if (v->kind == IRValue_CONST) {
    clone->as.konst = v->as.konst;
  }

  return clone;
}

void free_ir_val(struct IRValue *val)
{
  switch (val->kind) {
    case IRValue_CONST: {
      break;
    }
    case IRValue_VAR: {
      free(val->as.var);
      break;
    }
    default:
      assert(0);
  }
  free(val);
}

static enum IRInstrBinaryKind expr_bin_to_ir_bin(enum ExprBinKind kind,
                                                 Type type)
{
  switch (kind) {
    case EXPR_BIN_ADD:
      return IRInstrBinary_ADD;
    case EXPR_BIN_SUB:
      return IRInstrBinary_SUB;
    case EXPR_BIN_MUL:
      return IRInstrBinary_MUL;
    case EXPR_BIN_DIV:
      return IRInstrBinary_DIV;
    case EXPR_BIN_EQUAL_EQUAL:
      return IRInstrBinary_E;
    case EXPR_BIN_BANG_EQUAL:
      return IRInstrBinary_NE;
    case EXPR_BIN_LESS:
      return IRInstrBinary_L;
    case EXPR_BIN_LESS_EQUAL:
      return IRInstrBinary_LE;
    case EXPR_BIN_GREATER:
      return IRInstrBinary_G;
    case EXPR_BIN_GREATER_EQUAL:
      return IRInstrBinary_GE;
    case EXPR_BIN_BITWISE_AND:
      return IRInstrBinary_BIT_AND;
    case EXPR_BIN_BITWISE_XOR:
      return IRInstrBinary_BIT_XOR;
    case EXPR_BIN_BITWISE_OR:
      return IRInstrBinary_BIT_OR;
    case EXPR_BIN_SHIFT_LEFT:
      return IRInstrBinary_SHL;
    case EXPR_BIN_SHIFT_RIGHT:
      switch (type.kind) {
        case U8_T:
        case U16_T:
        case U32_T:
        case U64_T:
          return IRInstrBinary_SHR;
        default:
          return IRInstrBinary_SAR;
      }
    default:
      assert(0 && "unhandled ExprBinKind in expr_bin_to_ir_bin");
  }
}

void free_ir_instr(struct IRInstr *instr)
{
  switch (instr->kind) {
    case IRInstr_UNARY: {
      free_ir_val(instr->as.unary.src);
      free_ir_val(instr->as.unary.dst);
      break;
    }
    case IRInstr_BIN: {
      free_ir_val(instr->as.binary.lhs);
      free_ir_val(instr->as.binary.rhs);
      free_ir_val(instr->as.binary.dst);
      break;
    }
    case IRInstr_RET: {
      if (instr->as.ret.val) {
        free_ir_val(instr->as.ret.val);
      }
      break;
    }
    case IRInstr_CPY: {
      free_ir_val(instr->as.copy.src);
      free_ir_val(instr->as.copy.dst);
      break;
    }
    case IRInstr_CALL: {
      for (int i = 0; i < instr->as.call.args.len; i++) {
        free_ir_val(instr->as.call.args.data[i]);
      }
      vec_free(&instr->as.call.args);

      if (instr->as.call.dst) {
        free_ir_val(instr->as.call.dst);
      }
      break;
    }
    case IRInstr_JMP: {
      free(instr->as.jmp.target);
      break;
    }
    case IRInstr_JZ: {
      free(instr->as.jz.target);
      if (instr->as.jz.cond.kind == IRValue_VAR) {
        free(instr->as.jz.cond.as.var);
      }
      break;
    }
    case IRInstr_LBL: {
      free(instr->as.label.name);
      break;
    }
    case IRInstr_GETADDR: {
      free_ir_val(instr->as.getaddr.src);
      free_ir_val(instr->as.getaddr.dst);
      break;
    }
    case IRInstr_CAST: {
      free_ir_val(instr->as.cast.src);
      free_ir_val(instr->as.cast.dst);
      break;
    }
    case IRInstr_LOAD: {
      free_ir_val(instr->as.load.src);
      free_ir_val(instr->as.load.dst);
      break;
    }
    case IRInstr_STORE: {
      free_ir_val(instr->as.store.dst);
      free_ir_val(instr->as.store.val);
      break;
    }
    case IRInstr_CPY_FROM_OFFSET: {
      free_ir_val(instr->as.cpy_from_offset.src);
      free_ir_val(instr->as.cpy_from_offset.dst);
      break;
    }
    case IRInstr_CPY_TO_OFFSET: {
      free_ir_val(instr->as.cpy_to_offset.dst);
      free_ir_val(instr->as.cpy_to_offset.src);
      break;
    }
    case IRInstr_ADD_PTR: {
      free_ir_val(instr->as.add_ptr.ptr);
      free_ir_val(instr->as.add_ptr.index);
      free_ir_val(instr->as.add_ptr.dst);
      break;
    }
    default:
      assert(0 && "Unhandled IR instruction in free_ir_instr");
  }
}


static void free_ir_fn(struct IRFunction *func)
{
  for (int i = 0; i < func->body.len; i++) {
    free_ir_instr(&func->body.data[i]);
  }
  vec_free(&func->body);
  free(func);
}

void free_ir_prog(struct IRProgram *prog)
{
  for (int i = 0; i < prog->funcs.len; i++) {
    free_ir_fn(prog->funcs.data[i]);
  }
  vec_free(&prog->funcs);
}

static struct IRValue *mkirvar(void)
{
  struct IRValue *var;
  int i;

  i = mktmp();

  var = malloc(sizeof(struct IRValue));
  var->kind = IRValue_VAR;
  var->as.var = mkstr("tmp.%d", i);

  return var;
}

static struct IRValue *irfy_expr_and_convert(VecIRInstr *instrs,
                                             struct Expr *expr)
{
  /* evaluates an expr and forces lvalue2rvalue conversion. */
  struct ExpResult result;

  result = irfy_expr(instrs, expr);
  switch (result.kind) {
    case EXPRESULT_PLAIN:
      /* When we evaluate a purely mathematical AST node, like `1 + 2`, the
       * result of `irfy_expr` is `EXPRESULT_PLAIN`, which means that the result
       * is already a concrete value or a temporary register holding that value.
       *
       * That's it, we do not need to do anything else to use the value in
       * further IR generation pass.  */
      return result.as.plain;
    case EXPRESULT_DEREF: {
      /* When we evaluate e.g. `y = *x + 5`, that is a ptr deref, `irfy_expr`
       * returns `EXPRESULT_DEREF`, which gives us a memory address of that
       * variable, NOT the data inside it.
       *
       * Since the mem address won't do any good and we can't add it to `5` in
       * this case, we need the VALUE sitting at that memory address.  So, we
       * create a new IR variable, `dst`, and do:  `dst = load(src_ptr)`.
       *
       * Then we clone (is all this cloning necessary?) and return `dst`. */
      struct IRValue *dst = mkirvar();
      dst->type = expr->type;

      struct IRInstr instr;
      instr.kind = IRInstr_LOAD;
      instr.as.load.src = result.as.ptr;
      instr.as.load.dst = dst;
      vec_insert(instrs, instr);

      struct IRValue *ret = malloc(sizeof(struct IRValue));
      ret->kind = IRValue_VAR;
      ret->as.var = strdup(dst->as.var);
      ret->type = dst->type;

      return ret;
    }
    case EXPRESULT_SUBOBJECT: {
      struct IRValue *dst = mkirvar();
      dst->type = expr->type;

      struct IRValue *base_var = malloc(sizeof(struct IRValue));
      base_var->kind = IRValue_VAR;
      base_var->as.var = strdup(result.as.subobject.base);
      base_var->type = result.as.subobject.base_type;

      struct IRInstr_CopyFromOffset copy_from = {
          .src = base_var,
          .offset = result.as.subobject.offset,
          .dst = clone_irval(dst)};

      struct IRInstr instr;
      instr.kind = IRInstr_CPY_FROM_OFFSET;
      instr.as.cpy_from_offset = copy_from;
      vec_insert(instrs, instr);

      return dst;
    }
    default:
      assert(0);
  }

  assert(0);
  return NULL;
}

void free_global_constants(void)
{
  for (int i = 0; i < global_constants.len; i++) {
    free(global_constants.data[i].name);
    free(global_constants.data[i].value);
  }
  vec_free(&global_constants);
}

static void add_extern_variable(char *name)
{
  for (int i = 0; i < extern_variables.len; i++) {
    if (strcmp(extern_variables.data[i], name) == 0) {
      return;
    }
  }

  vec_insert(&extern_variables, strdup(name));
}

static void add_global_variable(char *name, char *value)
{
  struct StaticConstant global;

  global.name = strdup(name);
  global.value = value;
  vec_insert(&global_variables, global);
}

static bool is_extern_variable(char *name)
{
  for (int i = 0; i < extern_variables.len; i++) {
    if (strcmp(extern_variables.data[i], name) == 0) {
      return true;
    }
  }

  return false;
}

static bool is_global_variable(char *name)
{
  for (int i = 0; i < global_variables.len; i++) {
    if (strcmp(global_variables.data[i].name, name) == 0) {
      return true;
    }
  }

  return false;
}

bool is_data_variable(char *name)
{
  return is_extern_variable(name) || is_global_variable(name);
}

void free_global_variables(void)
{
  for (int i = 0; i < global_variables.len; i++) {
    free(global_variables.data[i].name);
    free(global_variables.data[i].value);
  }
  vec_free(&global_variables);
}

void free_extern_variables(void)
{
  for (int i = 0; i < extern_variables.len; i++) {
    free(extern_variables.data[i]);
  }
  vec_free(&extern_variables);
}

static enum IRCastKind get_cast_kind(Type src, Type dst)
{
  if (types_equal(src, dst)) {
    return IRCast_None;
  }

  bool src_is_float = (src.kind == F32_T || src.kind == F64_T);
  bool dst_is_float = (dst.kind == F32_T || dst.kind == F64_T);

  if (src_is_float && dst_is_float) {
    return src.kind == F32_T ? IRCast_FloatPromote : IRCast_FloatDemote;
  }

  if (src.kind == PTR_T && is_integer_type(dst.kind)) {
    return IRCast_PtrToInt;
  }
  if (is_integer_type(src.kind) && dst.kind == PTR_T) {
    return IRCast_IntToPtr;
  }
  if (src.kind == PTR_T && dst.kind == PTR_T) {
    return IRCast_Bitcast;
  }

  bool src_unsigned = is_unsigned(src.kind) || src.kind == BOOL_T;
  bool dst_unsigned = is_unsigned(dst.kind) || dst.kind == BOOL_T;

  if (src_is_float && !dst_is_float) {
    if (dst_unsigned) {
      return src.kind == F32_T ? IRCast_FloatToUInt : IRCast_DoubleToUInt;
    } else {
      return src.kind == F32_T ? IRCast_FloatToInt : IRCast_DoubleToInt;
    }
  }

  if (!src_is_float && dst_is_float) {
    if (src_unsigned) {
      return dst.kind == F32_T ? IRCast_UIntToFloat : IRCast_UIntToDouble;
    } else {
      return dst.kind == F32_T ? IRCast_IntToFloat : IRCast_IntToDouble;
    }
  }

  int src_sz = get_type_size(src);
  int dst_sz = get_type_size(dst);

  if (src_sz == dst_sz) {
    return IRCast_Bitcast;
  }
  if (src_sz > dst_sz) {
    return IRCast_Truncate;
  }
  return src_unsigned ? IRCast_ZeroExtend : IRCast_SignExtend;
}

static struct ExpResult irfy_expr(VecIRInstr *instrs, struct Expr *expr)
{
  switch (expr->kind) {
    case EXPR_LITERAL: {
      if (expr->as.literal.kind == LITERAL_STR) {
        char *name;
        struct IRValue *src, *dst;

        name = mklbl("str", mktmp());

        struct StaticConstant sc;
        sc.name = strdup(name);
        sc.value = strdup(expr->as.literal.as.str);
        vec_insert(&global_constants, sc);

        src = malloc(sizeof(struct IRValue));
        src->kind = IRValue_VAR;
        src->as.var = strdup(name);
        src->type = expr->type;

        dst = mkirvar();
        dst->type = expr->type;

        struct IRInstr instr;
        instr.kind = IRInstr_GETADDR;
        instr.as.getaddr.src = src;
        instr.as.getaddr.dst = clone_irval(dst);
        vec_insert(instrs, instr);

        free(name);

        return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = dst};
      } else {
        struct IRValue *ir_val = malloc(sizeof(struct IRValue));
        ir_val->kind = IRValue_CONST;
        ir_val->as.konst = expr->as.literal;
        ir_val->type = expr->type;

        return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = ir_val};
      }
    }
    case EXPR_VARIABLE: {
      struct IRValue *r = malloc(sizeof(struct IRValue));
      r->kind = IRValue_VAR;
      r->as.var = strdup(expr->as.var.name);
      r->type = expr->type;

      return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = r};
    }
    case EXPR_UNARY: {
      struct IRValue *src, *dst;

      /* Force lvalue-to-rvalue conversion for inner expr.  */
      src = irfy_expr_and_convert(instrs, expr->as.unary.expr);

      dst = mkirvar();
      dst->type = expr->type;

      enum IRInstrUnaryKind kind;
      if (expr->as.unary.op[0] == '!') {
        kind = IRInstrUnary_NOT;
      } else if (expr->as.unary.op[0] == '~') {
        kind = IRInstrUnary_BIT_NOT;
      } else {
        kind = IRInstrUnary_NEG;
      }

      struct IRInstr_Unary iu;
      iu.kind = kind;
      iu.src = src;
      iu.dst = clone_irval(dst);

      struct IRInstr i;
      i.kind = IRInstr_UNARY;
      i.as.unary = iu;
      vec_insert(instrs, i);

      return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = dst};
    }
    case EXPR_BINARY: {
      if (expr->as.binary.kind == EXPR_BIN_LOGICAL_AND) {
        struct IRValue *lhs, *rhs, *dst, *one, *zero, *dst_zero;
        char *lbl_false, *lbl_end;
        int tmp;

        dst = mkirvar();
        dst->type = (Type){.kind = BOOL_T};

        tmp = mktmp();
        lbl_false = mklbl("AndFalse", tmp);
        lbl_end = mklbl("AndEnd", tmp);

        lhs = irfy_expr_and_convert(instrs, expr->as.binary.lhs);
        vec_insert(instrs,
                   ((struct IRInstr){
                       .kind = IRInstr_JZ,
                       .as.jz = {.cond = *lhs, .target = strdup(lbl_false)}}));

        rhs = irfy_expr_and_convert(instrs, expr->as.binary.rhs);
        vec_insert(instrs,
                   ((struct IRInstr){
                       .kind = IRInstr_JZ,
                       .as.jz = {.cond = *rhs, .target = strdup(lbl_false)}}));

        one = malloc(sizeof(struct IRValue));
        one->kind = IRValue_CONST;
        one->type = (Type){.kind = BOOL_T};
        one->as.konst.kind = LITERAL_BOOL;
        one->as.konst.type = (Type){.kind = BOOL_T};
        one->as.konst.as.boolean = true;

        vec_insert(instrs,
                   ((struct IRInstr){
                       .kind = IRInstr_CPY,
                       .as.copy = {.src = one, .dst = clone_irval(dst)}}));
        vec_insert(instrs,
                   ((struct IRInstr){.kind = IRInstr_JMP,
                                     .as.jmp = {.target = strdup(lbl_end)}}));
        vec_insert(instrs,
                   ((struct IRInstr){.kind = IRInstr_LBL,
                                     .as.label = {.name = strdup(lbl_false)}}));

        zero = malloc(sizeof(struct IRValue));
        zero->kind = IRValue_CONST;
        zero->type = (Type){.kind = BOOL_T};
        zero->as.konst.kind = LITERAL_BOOL;
        zero->as.konst.type = (Type){.kind = BOOL_T};
        zero->as.konst.as.boolean = false;

        dst_zero = clone_irval(dst);

        vec_insert(instrs, ((struct IRInstr){
                               .kind = IRInstr_CPY,
                               .as.copy = {.src = zero, .dst = dst_zero}}));
        vec_insert(instrs,
                   ((struct IRInstr){.kind = IRInstr_LBL,
                                     .as.label = {.name = strdup(lbl_end)}}));

        free(lbl_false);
        free(lbl_end);

        return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = dst};
      }

      if (expr->as.binary.kind == EXPR_BIN_LOGICAL_OR) {
        struct IRValue *lhs, *rhs, *dst, *one, *zero, *dst_zero;
        int tmp;
        char *lbl_check_rhs, *lbl_true, *lbl_false, *lbl_end;

        dst = mkirvar();
        dst->type = (Type){.kind = BOOL_T};

        tmp = mktmp();
        lbl_check_rhs = mklbl("OrRhs", tmp);
        lbl_true = mklbl("OrTrue", tmp);
        lbl_false = mklbl("OrFalse", tmp);
        lbl_end = mklbl("OrEnd", tmp);

        lhs = irfy_expr_and_convert(instrs, expr->as.binary.lhs);
        vec_insert(
            instrs,
            ((struct IRInstr){
                .kind = IRInstr_JZ,
                .as.jz = {.cond = *lhs, .target = strdup(lbl_check_rhs)}}));
        vec_insert(instrs,
                   ((struct IRInstr){.kind = IRInstr_JMP,
                                     .as.jmp = {.target = strdup(lbl_true)}}));
        vec_insert(instrs, ((struct IRInstr){
                               .kind = IRInstr_LBL,
                               .as.label = {.name = strdup(lbl_check_rhs)}}));

        rhs = irfy_expr_and_convert(instrs, expr->as.binary.rhs);
        vec_insert(instrs,
                   ((struct IRInstr){
                       .kind = IRInstr_JZ,
                       .as.jz = {.cond = *rhs, .target = strdup(lbl_false)}}));

        vec_insert(instrs,
                   ((struct IRInstr){.kind = IRInstr_LBL,
                                     .as.label = {.name = strdup(lbl_true)}}));

        one = malloc(sizeof(struct IRValue));
        one->kind = IRValue_CONST;
        one->type = (Type){.kind = BOOL_T};
        one->as.konst.kind = LITERAL_BOOL;
        one->as.konst.type = (Type){.kind = BOOL_T};
        one->as.konst.as.boolean = true;

        vec_insert(instrs,
                   ((struct IRInstr){
                       .kind = IRInstr_CPY,
                       .as.copy = {.src = one, .dst = clone_irval(dst)}}));
        vec_insert(instrs,
                   ((struct IRInstr){.kind = IRInstr_JMP,
                                     .as.jmp = {.target = strdup(lbl_end)}}));
        vec_insert(instrs,
                   ((struct IRInstr){.kind = IRInstr_LBL,
                                     .as.label = {.name = strdup(lbl_false)}}));

        zero = malloc(sizeof(struct IRValue));
        zero->kind = IRValue_CONST;
        zero->type = (Type){.kind = BOOL_T};
        zero->as.konst.kind = LITERAL_BOOL;
        zero->as.konst.type = (Type){.kind = BOOL_T};
        zero->as.konst.as.boolean = false;

        dst_zero = clone_irval(dst);

        vec_insert(instrs, ((struct IRInstr){
                               .kind = IRInstr_CPY,
                               .as.copy = {.src = zero, .dst = dst_zero}}));
        vec_insert(instrs,
                   ((struct IRInstr){.kind = IRInstr_LBL,
                                     .as.label = {.name = strdup(lbl_end)}}));

        free(lbl_check_rhs);
        free(lbl_true);
        free(lbl_false);
        free(lbl_end);

        return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = dst};
      }

      struct IRValue *lhs, *rhs, *dst;

      lhs = irfy_expr_and_convert(instrs, expr->as.binary.lhs);
      rhs = irfy_expr_and_convert(instrs, expr->as.binary.rhs);

      dst = mkirvar();
      dst->type = expr->type;

      enum IRInstrBinaryKind kind;
      kind = expr_bin_to_ir_bin(expr->as.binary.kind, expr->type);

      struct IRInstr_Binary bininstr = {
          .lhs = lhs, .rhs = rhs, .dst = clone_irval(dst), .kind = kind};
      struct IRInstr instr;
      instr.kind = IRInstr_BIN;
      instr.as.binary = bininstr;
      vec_insert(instrs, instr);

      return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = dst};
    }
    case EXPR_ASSIGN: {
      struct ExpResult lhs_res;
      struct IRValue *rhs_val;

      /* Keep lhs as lvalue.  */
      lhs_res = irfy_expr(instrs, expr->as.assign.lhs);

      /* Force lvalue-to-rvalue conversion for rhs.  */
      rhs_val = irfy_expr_and_convert(instrs, expr->as.assign.rhs);

      /* If lhs is a plain operand, emit just a cpy.  */
      if (lhs_res.kind == EXPRESULT_PLAIN) {
        struct IRInstr instr = {0};
        instr.kind = IRInstr_CPY;
        instr.as.copy = (struct IRInstr_Copy){
            .src = rhs_val, .dst = clone_irval(lhs_res.as.plain)};
        vec_insert(instrs, instr);

        return (struct ExpResult){.kind = EXPRESULT_PLAIN,
                                  .as.plain = lhs_res.as.plain};
      } else if (lhs_res.kind == EXPRESULT_DEREF) {
        /* ...otherwise, emit a store.  */
        struct IRInstr instr = {0};
        instr.kind = IRInstr_STORE;
        instr.as.store = (struct IRInstr_Store){.val = clone_irval(rhs_val),
                                                .dst = lhs_res.as.ptr};
        vec_insert(instrs, instr);

        return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = rhs_val};
      } else if (lhs_res.kind == EXPRESULT_SUBOBJECT) {
        /* Reconstruct the base variable from the subobject name */
        struct IRValue *base_var = malloc(sizeof(struct IRValue));
        base_var->kind = IRValue_VAR;
        base_var->as.var = strdup(lhs_res.as.subobject.base);
        base_var->type = lhs_res.as.subobject.base_type;

        struct IRInstr_CopyToOffset copy_to = {
            .dst = base_var,
            .offset = lhs_res.as.subobject.offset,
            .src = clone_irval(rhs_val)};

        struct IRInstr instr = {0};
        instr.kind = IRInstr_CPY_TO_OFFSET;
        instr.as.cpy_to_offset = copy_to;
        vec_insert(instrs, instr);

        /* Assignments evaluate to their right-hand side value */
        return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = rhs_val};
      } else {
        assert(0 && "Unhandled left-hand side in assignment");
      }

      break;
    }
    case EXPR_COMPOUND_ASSIGN: {
      struct ExpResult lhs_res;
      struct IRValue *rhs_val;

      /* Keep lhs as lvalue. */
      lhs_res = irfy_expr(instrs, expr->as.compound_assign.lhs);

      /* Force lvalue-to-rvalue conversion for rhs. */
      rhs_val = irfy_expr_and_convert(instrs, expr->as.compound_assign.rhs);

      /* 1. Load the current value of LHS into a temporary */
      struct IRValue *lhs_val = NULL;
      if (lhs_res.kind == EXPRESULT_PLAIN) {
        lhs_val = lhs_res.as.plain;
      } else if (lhs_res.kind == EXPRESULT_DEREF) {
        lhs_val = mkirvar();
        lhs_val->type = expr->type;
        struct IRInstr load = {
            .kind = IRInstr_LOAD,
            .as.load = {.src = lhs_res.as.ptr, .dst = clone_irval(lhs_val)}};
        vec_insert(instrs, load);
      } else if (lhs_res.kind == EXPRESULT_SUBOBJECT) {
        lhs_val = mkirvar();
        lhs_val->type = expr->type;
        struct IRValue *base_var = malloc(sizeof(struct IRValue));
        base_var->kind = IRValue_VAR;
        base_var->as.var = strdup(lhs_res.as.subobject.base);
        base_var->type = lhs_res.as.subobject.base_type;

        struct IRInstr_CopyFromOffset copy_from = {
            .src = base_var,
            .offset = lhs_res.as.subobject.offset,
            .dst = clone_irval(lhs_val)};
        struct IRInstr instr = {.kind = IRInstr_CPY_FROM_OFFSET,
                                .as.cpy_from_offset = copy_from};
        vec_insert(instrs, instr);
      }

      /* 2. Perform the arithmetic operation */
      struct IRValue *bin_res = mkirvar();
      bin_res->type = expr->type;

      struct IRInstr bin_instr = {
          .kind = IRInstr_BIN,
          .as.binary = {.kind = expr_bin_to_ir_bin(
                            expr->as.compound_assign.kind, expr->type),
                        .lhs = clone_irval(lhs_val),
                        .rhs = rhs_val,
                        .dst = clone_irval(bin_res)}};
      vec_insert(instrs, bin_instr);

      /* 3. Store the result back into the LHS location */
      if (lhs_res.kind == EXPRESULT_PLAIN) {
        struct IRInstr cpy = {
            .kind = IRInstr_CPY,
            .as.copy = {.src = clone_irval(bin_res),
                        .dst = clone_irval(lhs_res.as.plain)}};
        vec_insert(instrs, cpy);
      } else if (lhs_res.kind == EXPRESULT_DEREF) {
        struct IRInstr store = {
            .kind = IRInstr_STORE,
            .as.store = {.val = clone_irval(bin_res), .dst = lhs_res.as.ptr}};
        vec_insert(instrs, store);
      } else if (lhs_res.kind == EXPRESULT_SUBOBJECT) {
        struct IRValue *base_var = malloc(sizeof(struct IRValue));
        base_var->kind = IRValue_VAR;
        base_var->as.var = strdup(lhs_res.as.subobject.base);
        base_var->type = lhs_res.as.subobject.base_type;

        struct IRInstr_CopyToOffset copy_to = {
            .dst = base_var,
            .offset = lhs_res.as.subobject.offset,
            .src = clone_irval(bin_res)};
        struct IRInstr instr = {.kind = IRInstr_CPY_TO_OFFSET,
                                .as.cpy_to_offset = copy_to};
        vec_insert(instrs, instr);
      }

      return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = bin_res};
    }
    case EXPR_CALL: {
      /* Force lvalue-to-rvalue conversion upon the arguments.  */
      VecIRValuePtr args = {0};
      for (int i = 0; i < expr->as.call.arguments.len; i++) {
        vec_insert(&args, irfy_expr_and_convert(
                              instrs, &expr->as.call.arguments.data[i]));
      }

      /* If the function returns a value, make sure we capture it in dst.  */
      struct IRValue *dst = NULL;
      if (expr->type.kind != VOID_T) {
        dst = mkirvar();
        dst->type = expr->type;
      }

      struct IRInstr_Call call_instr = {0};
      call_instr.target = *expr->as.call.target;
      call_instr.args = args;
      call_instr.dst = clone_irval(dst);

      struct IRInstr instr = {0};
      instr.kind = IRInstr_CALL;
      instr.as.call = call_instr;
      vec_insert(instrs, instr);

      /* Function calls evaluate to rvalues.  */
      if (dst) {
        return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = dst};
      } else {
        return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = NULL};
      }

      break;
    }
    case EXPR_SIZEOF: {
      struct ExpResult result;
      struct IRValue sz;

      sz.kind = IRValue_CONST;
      sz.as.konst = (struct Literal){
          .kind = LITERAL_NUM,
          .as.u64 = (unsigned long long) get_type_size(expr->type)};
      sz.type = (Type){.kind = U64_T};

      result.kind = EXPRESULT_PLAIN;
      result.as.plain = ALLOC(sz);

      return result;
    }
    case EXPR_ADDROF: {
      struct ExpResult result;

      result = irfy_expr(instrs, expr->as.addrof.expr);
      switch (result.kind) {
        case EXPRESULT_PLAIN: {
          /* If we take addrof of a plain operand, we will do
           * `dst = getaddr(src)`, and return cloned `dst`.  */
          struct IRValue *dst = mkirvar();
          dst->type = expr->type;

          struct IRInstr instr;
          instr.kind = IRInstr_GETADDR;
          instr.as.getaddr.src = result.as.plain;
          instr.as.getaddr.dst = clone_irval(dst);
          vec_insert(instrs, instr);

          return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = dst};
        }
        case EXPRESULT_DEREF: {
          /* If we take addrof of a deref (like `&*p`), they cancel out.  */
          return (struct ExpResult){.kind = EXPRESULT_PLAIN,
                                    .as.plain = result.as.ptr};
        }
        case EXPRESULT_SUBOBJECT: {
          struct IRValue *base_var = malloc(sizeof(struct IRValue));
          base_var->kind = IRValue_VAR;
          base_var->as.var = strdup(result.as.subobject.base);
          base_var->type = result.as.subobject.base_type;

          struct IRValue *base_ptr = mkirvar();
          base_ptr->type =
              (Type){.kind = PTR_T, .as.base = ALLOC(base_var->type)};

          struct IRInstr_GetAddress getaddr = {.src = base_var,
                                               .dst = clone_irval(base_ptr)};
          vec_insert(instrs, ((struct IRInstr){.kind = IRInstr_GETADDR,
                                               .as.getaddr = getaddr}));
          struct IRValue *field_ptr = mkirvar();
          field_ptr->type = expr->type;

          struct IRValue *index_val = malloc(sizeof(struct IRValue));
          index_val->kind = IRValue_CONST;
          index_val->type = (Type){.kind = I32_T};
          index_val->as.konst.kind = LITERAL_NUM;
          index_val->as.konst.type = (Type){.kind = I32_T};
          index_val->as.konst.as.i32 = result.as.subobject.offset;

          struct IRInstr_AddPtr add_ptr = {.ptr = base_ptr,
                                           .index = index_val,
                                           .scale = 1,
                                           .dst = clone_irval(field_ptr)};
          vec_insert(instrs, ((struct IRInstr){.kind = IRInstr_ADD_PTR,
                                               .as.add_ptr = add_ptr}));
          return (struct ExpResult){.kind = EXPRESULT_PLAIN,
                                    .as.plain = field_ptr};
        }
        default:
          assert(0);
      }

      break;
    }
    case EXPR_DEREF: {
      /* When we encounter a deref expression, like `*p`, we need to
       * evaluate the inner expr, which is `p`, to get the actual mem
       * addr.  But, we do NOT issue a LOAD instruction just yet.  Instead,
       * we defer this to the parent node because a Deref node might be
       * an lvalue (`*x = 5`) or an rvalue (`y = *x + 5`).  The parent node
       * will be the one to intercept this and decide whether a LOAD is needed
       * or just CPY.  */
      struct IRValue *ptr_val;

      ptr_val = irfy_expr_and_convert(instrs, expr->as.deref.expr);
      return (struct ExpResult){.kind = EXPRESULT_DEREF, .as.ptr = ptr_val};
    }
    case EXPR_CAST: {
      struct IRValue *src = irfy_expr_and_convert(instrs, expr->as.cast.expr);
      struct IRValue *dst = mkirvar();
      dst->type = expr->type;

      enum IRCastKind kind =
          get_cast_kind(expr->as.cast.expr->type, expr->type);

      if (kind == IRCast_None || kind == IRCast_Bitcast ||
          kind == IRCast_PtrToInt || kind == IRCast_IntToPtr) {
        struct IRInstr i = {0};
        i.kind = IRInstr_CPY;
        i.as.copy.src = src;
        i.as.copy.dst = clone_irval(dst);
        vec_insert(instrs, i);
      } else {
        struct IRInstr i = {0};
        i.kind = IRInstr_CAST;
        i.as.cast.kind = kind;
        i.as.cast.src = src;
        i.as.cast.dst = clone_irval(dst);
        vec_insert(instrs, i);
      }

      return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = dst};
    }
    case EXPR_MEMBER: {
      Type target_type = expr->as.member.target->type;
      char *struct_name = expr->as.member.is_arrow
                              ? target_type.as.base->as.struct_name
                              : target_type.as.struct_name;

      struct StructDef *def = struct_get(struct_table, struct_name);

      int offset = 0;
      for (int i = 0; i < def->fields.len; i++) {
        if (strcmp(def->fields.data[i].name, expr->as.member.field_name) == 0) {
          offset = def->fields.data[i].offset;
          break;
        }
      }

      if (expr->as.member.is_arrow) {
        struct IRValue *base_ptr =
            irfy_expr_and_convert(instrs, expr->as.member.target);

        if (offset == 0) {
          return (struct ExpResult){.kind = EXPRESULT_DEREF,
                                    .as.ptr = base_ptr};
        }
        struct IRValue *field_ptr = mkirvar();
        field_ptr->type = (Type){.kind = PTR_T, .as.base = ALLOC(expr->type)};

        struct IRValue *index_val = malloc(sizeof(struct IRValue));
        index_val->kind = IRValue_CONST;
        index_val->type = (Type){.kind = I32_T};
        index_val->as.konst.kind = LITERAL_NUM;
        index_val->as.konst.type = (Type){.kind = I32_T};
        index_val->as.konst.as.i32 = offset;

        struct IRInstr_AddPtr add_ptr = {.ptr = base_ptr,
                                         .index = index_val,
                                         .scale = 1,
                                         .dst = clone_irval(field_ptr)};
        vec_insert(instrs, ((struct IRInstr){.kind = IRInstr_ADD_PTR,
                                             .as.add_ptr = add_ptr}));

        return (struct ExpResult){.kind = EXPRESULT_DEREF, .as.ptr = field_ptr};
      } else {
        /* Direct access: foo.bar */
        struct ExpResult target_res = irfy_expr(instrs, expr->as.member.target);
        if (target_res.kind == EXPRESULT_PLAIN) {
          return (struct ExpResult){
              .kind = EXPRESULT_SUBOBJECT,
              .as.subobject = {
                  .base = strdup(target_res.as.plain->as.var),
                  .offset = offset,
                  .base_type = target_res.as.plain->type,
              }};
        } else if (target_res.kind == EXPRESULT_SUBOBJECT) {
          return (struct ExpResult){
              .kind = EXPRESULT_SUBOBJECT,
              .as.subobject = {
                  .base = strdup(target_res.as.subobject.base),
                  .offset = target_res.as.subobject.offset + offset,
                  .base_type = target_res.as.subobject.base_type,
              }};
        } else if (target_res.kind == EXPRESULT_DEREF) {
          if (offset == 0) {
            return target_res;
          }

          struct IRValue *field_ptr = mkirvar();
          field_ptr->type = (Type){.kind = PTR_T, .as.base = ALLOC(expr->type)};

          struct IRValue *index_val = malloc(sizeof(struct IRValue));
          index_val->kind = IRValue_CONST;
          index_val->type = (Type){.kind = I32_T};
          index_val->as.konst.kind = LITERAL_NUM;
          index_val->as.konst.type = (Type){.kind = I32_T};
          index_val->as.konst.as.i32 = offset;

          struct IRInstr_AddPtr add_ptr = {.ptr = target_res.as.ptr,
                                           .index = index_val,
                                           .scale = 1,
                                           .dst = clone_irval(field_ptr)};
          vec_insert(instrs, ((struct IRInstr){.kind = IRInstr_ADD_PTR,
                                               .as.add_ptr = add_ptr}));

          return (struct ExpResult){.kind = EXPRESULT_DEREF,
                                    .as.ptr = field_ptr};
        }
      }
      break;
    }
    case EXPR_STRUCT_INIT: {
      struct IRValue *tmp_struct = mkirvar();
      tmp_struct->type.kind = STRUCT_T;
      tmp_struct->type.as.struct_name = expr->as.struct_init.struct_name;

      for (int i = 0; i < expr->as.struct_init.values.len; i++) {
        struct IRValue *val = irfy_expr_and_convert(
            instrs, expr->as.struct_init.values.data[i].expr);

        int offset = expr->as.struct_init.values.data[i].resolved_offset;

        struct IRInstr_CopyToOffset copy_to = {
            .dst = clone_irval(tmp_struct), .offset = offset, .src = val};

        vec_insert(instrs, ((struct IRInstr){.kind = IRInstr_CPY_TO_OFFSET,
                                             .as.cpy_to_offset = copy_to}));
      }

      return (struct ExpResult){.kind = EXPRESULT_PLAIN,
                                .as.plain = tmp_struct};
    }
    default:
      assert(0);
  }
  assert(0);
}

static int extract_label_number(const char *label)
{
  if (!label) {
    return -1;
  }

  const char *dot = strrchr(label, '.');

  if (!dot) {
    return -1;
  }

  dot++;

  int n = -1;
  if (sscanf(dot, "%d", &n) == 1) {
    return n;
  }

  return -1;
}

static void irfy_stmt(VecIRInstr *instrs, struct Stmt *stmt)
{
  switch (stmt->kind) {
    case STMT_LOOP: {
      int tmp;
      struct IRInstr begin, jmp, end;

      tmp = extract_label_number(stmt->as.loop.label);

      begin.kind = IRInstr_LBL;
      begin.as.label.name = mklbl("Loop", tmp);

      vec_insert(instrs, begin);

      irfy_stmt(instrs, stmt->as.loop.body);

      jmp.kind = IRInstr_JMP;
      jmp.as.jmp.target = mklbl("Loop", tmp);

      vec_insert(instrs, jmp);

      end.kind = IRInstr_LBL;
      end.as.label.name = mklbl("End", tmp);

      vec_insert(instrs, end);

      break;
    }
    case STMT_LET: {
      struct IRValue *res, *dst;
      struct IRInstr cpy = {0};

      res = irfy_expr_and_convert(instrs, stmt->as.let.init);

      dst = malloc(sizeof(struct IRValue));
      dst->kind = IRValue_VAR;
      dst->as.var = strdup(stmt->as.let.name);
      dst->type = stmt->as.let.type;

      cpy.kind = IRInstr_CPY;
      cpy.as.copy = (struct IRInstr_Copy){.src = res, .dst = dst};

      vec_insert(instrs, cpy);
      break;
    }
    case STMT_RET: {
      struct IRInstr i = {0};

      i.kind = IRInstr_RET;
      i.as.ret.val = stmt->as.ret.val
                         ? irfy_expr_and_convert(instrs, stmt->as.ret.val)
                         : NULL;

      vec_insert(instrs, i);
      break;
    }
    case STMT_IF: {
      int tmp;
      struct IRValue *cond;

      tmp = mktmp();
      cond = irfy_expr_and_convert(instrs, &stmt->as.if_stmt.cond);

      if (!stmt->as.if_stmt.else_block) {
        struct IRInstr i1 = {0}, i2 = {0};
        struct IRInstr_JumpIfZero ijz = {0};
        struct IRInstr_Label ilbl = {0};

        ijz.cond = *cond;
        ijz.target = mklbl("End", tmp);

        i1.kind = IRInstr_JZ;
        i1.as.jz = ijz;

        vec_insert(instrs, i1);

        irfy_stmt(instrs, stmt->as.if_stmt.then_block);

        ilbl.name = mklbl("End", tmp);

        i2.kind = IRInstr_LBL;
        i2.as.label = ilbl;

        vec_insert(instrs, i2);
      } else {
        struct IRInstr jz_instr = {0};
        jz_instr.kind = IRInstr_JZ;
        jz_instr.as.jz.cond = *cond;
        jz_instr.as.jz.target = mklbl("Else", tmp);
        vec_insert(instrs, jz_instr);

        irfy_stmt(instrs, stmt->as.if_stmt.then_block);

        struct IRInstr jmp_end = {0};
        jmp_end.kind = IRInstr_JMP;
        jmp_end.as.jmp.target = mklbl("End", tmp);
        vec_insert(instrs, jmp_end);

        struct IRInstr label_else = {0};
        label_else.kind = IRInstr_LBL;
        label_else.as.label.name = mklbl("Else", tmp);
        vec_insert(instrs, label_else);

        irfy_stmt(instrs, stmt->as.if_stmt.else_block);

        struct IRInstr label_end = {0};
        label_end.kind = IRInstr_LBL;
        label_end.as.label.name = mklbl("End", tmp);
        vec_insert(instrs, label_end);
      }

      free(cond);
      break;
    }
    case STMT_DO_WHILE: {
      int tmp;
      struct IRValue *cond;
      struct IRInstr i1 = {0}, i2 = {0}, i3 = {0}, i4 = {0}, i5 = {0};

      tmp = extract_label_number(stmt->as.do_while_stmt.label);

      i1.kind = IRInstr_LBL;
      i1.as.label.name = mklbl("DoWhile", tmp);

      vec_insert(instrs, i1);

      irfy_stmt(instrs, stmt->as.do_while_stmt.body);

      i5.kind = IRInstr_LBL;
      i5.as.label.name = mklbl("Cond", tmp);

      vec_insert(instrs, i5);

      cond = irfy_expr_and_convert(instrs, &stmt->as.do_while_stmt.cond);

      i2.kind = IRInstr_JZ;
      i2.as.jz.cond = *cond;
      i2.as.jz.target = mklbl("End", tmp);

      free(cond);

      vec_insert(instrs, i2);

      i3.kind = IRInstr_JMP;
      i3.as.jmp.target = mklbl("DoWhile", tmp);

      vec_insert(instrs, i3);

      i4.kind = IRInstr_LBL;
      i4.as.label.name = mklbl("End", tmp);

      vec_insert(instrs, i4);
      break;
    }
    case STMT_WHILE: {
      int tmp;
      struct IRValue *cond;
      struct IRInstr i1 = {0}, i2 = {0}, i3 = {0}, i4 = {0};

      tmp = extract_label_number(stmt->as.while_stmt.label);

      i4.kind = IRInstr_LBL;
      i4.as.label.name = mklbl("While", tmp);

      vec_insert(instrs, i4);

      cond = irfy_expr_and_convert(instrs, &stmt->as.while_stmt.cond);

      i1.kind = IRInstr_JZ;
      i1.as.jz.cond = *cond;
      i1.as.jz.target = mklbl("End", tmp);

      free(cond);

      vec_insert(instrs, i1);

      irfy_stmt(instrs, stmt->as.while_stmt.body);

      i3.kind = IRInstr_JMP;
      i3.as.jmp.target = mklbl("While", tmp);

      vec_insert(instrs, i3);

      i2.kind = IRInstr_LBL;
      i2.as.label.name = mklbl("End", tmp);

      vec_insert(instrs, i2);
      break;
    }
    case STMT_BLOCK: {
      for (int i = 0; i < stmt->as.block.stmts.len; i++) {
        irfy_stmt(instrs, &stmt->as.block.stmts.data[i]);
      }
      break;
    }
    case STMT_BREAK: {
      char *label, *new_label;
      int n;

      label = stmt->as.break_stmt.label;
      n = extract_label_number(label);

      new_label = mklbl("End", n);

      struct IRInstr i;
      i.kind = IRInstr_JMP;

      struct IRInstr_Jump jmp;
      jmp.target = new_label;

      i.as.jmp = jmp;

      vec_insert(instrs, i);

      break;
    }
    case STMT_CONTINUE: {
      char *label;

      label = stmt->as.continue_stmt.label;

      struct IRInstr i;
      i.kind = IRInstr_JMP;
      i.as.jmp.target = strdup(label);

      vec_insert(instrs, i);
      break;
    }
    case STMT_GOTO: {
      char *label;

      label = stmt->as.goto_stmt.label;

      struct IRInstr i;
      i.kind = IRInstr_JMP;
      i.as.jmp.target = strdup(label);

      vec_insert(instrs, i);
      break;
    }
    case STMT_LABELED: {
      char *label;

      label = stmt->as.labeled.label;

      struct IRInstr i;
      i.kind = IRInstr_LBL;
      i.as.label.name = strdup(label);

      vec_insert(instrs, i);

      irfy_stmt(instrs, stmt->as.labeled.stmt);
      break;
    }
    case STMT_EXPR: {
      struct IRValue *v;

      v = irfy_expr_and_convert(instrs, &stmt->as.expr_stmt.expr);
      if (v) {
        free_ir_val(v);
      }

      break;
    }
    default:
      assert(0);
  }
}


static bool is_negative_literal(struct Expr *expr, struct Literal **literal,
                                bool *is_negative)
{
  if (expr->kind == EXPR_LITERAL) {
    *literal = &expr->as.literal;
    *is_negative = false;
    return true;
  }

  if (expr->kind == EXPR_UNARY && strcmp(expr->as.unary.op, "-") == 0 &&
      expr->as.unary.expr->kind == EXPR_LITERAL) {
    *literal = &expr->as.unary.expr->as.literal;
    *is_negative = true;
    return true;
  }

  return false;
}

static char *format_directive(const char *fmt, unsigned long long value)
{
  char buf[128];

  snprintf(buf, sizeof(buf), fmt, value);
  return strdup(buf);
}

static char *format_signed_directive(const char *fmt, long long value)
{
  char buf[128];

  snprintf(buf, sizeof(buf), fmt, value);
  return strdup(buf);
}

static char *global_string_directive(char *value)
{
  char *label, *directive;
  struct StaticConstant sc;
  char buf[128];

  label = mklbl("str", mktmp());
  sc.name = strdup(label);
  sc.value = strdup(value);
  vec_insert(&global_constants, sc);

  snprintf(buf, sizeof(buf), ".quad %s", label);
  directive = strdup(buf);
  free(label);

  return directive;
}

static char *global_initializer_directive(struct DeclVariable *variable)
{
  struct Literal *literal;
  bool is_negative;

  if (!variable->init ||
      !is_negative_literal(variable->init, &literal, &is_negative)) {
    return NULL;
  }

  if (literal->kind == LITERAL_STR) {
    if (variable->type.kind == STR_T && !is_negative) {
      return global_string_directive(literal->as.str);
    }
    return NULL;
  }

  if (literal->kind == LITERAL_BOOL) {
    if (is_negative) {
      return NULL;
    }
    return format_directive(".byte %llu",
                            literal->as.boolean ? 1ULL : 0ULL);
  }

  if (literal->kind != LITERAL_NUM) {
    return NULL;
  }

  switch (variable->type.kind) {
    case I8_T:
      return format_signed_directive(".byte %lld",
                                     is_negative ? -literal->as.i8
                                                 : literal->as.i8);
    case U8_T:
      if (is_negative) {
        return NULL;
      }
      return format_directive(".byte %llu", literal->as.u8);
    case I16_T:
      return format_signed_directive(".value %lld",
                                     is_negative ? -literal->as.i16
                                                 : literal->as.i16);
    case U16_T:
      if (is_negative) {
        return NULL;
      }
      return format_directive(".value %llu", literal->as.u16);
    case I32_T:
      return format_signed_directive(".long %lld",
                                     is_negative ? -literal->as.i32
                                                 : literal->as.i32);
    case U32_T:
      if (is_negative) {
        return NULL;
      }
      return format_directive(".long %llu", literal->as.u32);
    case I64_T:
      return format_signed_directive(".quad %lld",
                                     is_negative ? -literal->as.i64
                                                 : literal->as.i64);
    case U64_T:
      if (is_negative) {
        return NULL;
      }
      return format_directive(".quad %llu", literal->as.u64);
    case F32_T: {
      float value = literal->as.f32;
      unsigned int bits;
      if (is_negative) {
        value = -value;
      }
      memcpy(&bits, &value, sizeof(float));
      return format_directive(".long %llu", bits);
    }
    case F64_T: {
      double value = literal->as.f64;
      unsigned long long bits;
      if (is_negative) {
        value = -value;
      }
      memcpy(&bits, &value, sizeof(double));
      return format_directive(".quad %llu", bits);
    }
    case PTR_T:
      if (!is_negative && literal->as.u64 == 0) {
        return strdup(".quad 0");
      }
      return NULL;
    default:
      return NULL;
  }
}

static struct IRFunction *irfy_fn(struct DeclFn *fn)
{
  if (fn->is_extern) {
    return NULL;
  }

  struct IRFunction f;
  VecIRInstr instrs = {0};

  for (int i = 0; i < fn->body.len; i++) {
    irfy_stmt(&instrs, &fn->body.data[i]);
  }

  f.body = instrs;
  f.name = fn->name;
  f.params = fn->params;
  f.retval = fn->retval;

  return ALLOC(f);
}

struct IrfyResult irfy_ast(struct AST *ast)
{
  struct IRProgram prog;
  struct IrfyResult result;
  VecIRFunctionPtr funcs = {0};

  result.is_ok = true;
  result.msg = NULL;

  for (int i = 0; i < ast->decls.len; i++) {
    struct IRFunction *f;
    struct Decl *decl = &ast->decls.data[i];

    if (decl->kind == DECL_VARIABLE) {
      if (decl->as.variable.is_extern) {
        add_extern_variable(decl->as.variable.name);
      } else {
        char *directive = global_initializer_directive(&decl->as.variable);
        if (!directive) {
          result.is_ok = false;
          result.msg =
              "Global variable initializers must be constant literals";
          prog.funcs = funcs;
          result.prog = prog;
          return result;
        }
        add_global_variable(decl->as.variable.name, directive);
      }
      continue;
    }

    if (decl->kind != DECL_FN) {
      continue;
    }

    f = irfy_fn(&decl->as.fn);
    if (f) {
      vec_insert(&funcs, f);
    }
  }

  prog.funcs = funcs;

  result.prog = prog;

  return result;
}

#if defined (DEBUG_IR) || defined(DEBUG_IR_OPT)
void print_ir_val(struct IRValue *ir_val, int spaces)
{
  switch (ir_val->kind) {
    case IRValue_CONST: {
      printf("IRValue(\n");

      print_indent(spaces + 2);
      printf("type = CONST,\n");

      print_indent(spaces + 2);

      switch (ir_val->type.kind) {
        case I8_T: {
          printf("v: %d,\n", ir_val->as.konst.as.i8);
          break;
        }
        case U8_T: {
          printf("v: %d,\n", ir_val->as.konst.as.u8);
          break;
        }
        case I16_T: {
          printf("v: %d,\n", ir_val->as.konst.as.i16);
          break;
        }
        case U16_T: {
          printf("v: %d,\n", ir_val->as.konst.as.u16);
          break;
        }
        case I32_T: {
          printf("v: %d,\n", ir_val->as.konst.as.i32);
          break;
        }
        case U32_T: {
          printf("v: %d,\n", ir_val->as.konst.as.u32);
          break;
        }
        case I64_T: {
          printf("v: %lld,\n", ir_val->as.konst.as.i64);
          break;
        }
        case U64_T: {
          printf("v: %llu,\n", ir_val->as.konst.as.u64);
          break;
        }
        case F32_T: {
          printf("v: %f,\n", ir_val->as.konst.as.f32);
          break;
        }
        case F64_T: {
          printf("v: %f,\n", ir_val->as.konst.as.f64);
          break;
        }
        case BOOL_T: {
          printf("v: %s,\n", ir_val->as.konst.as.boolean ? "true" : "false");
          break;
        }
        case STR_T: {
          printf("v: \"%s\",\n", ir_val->as.konst.as.str);
          break;
        }
        case PTR_T: {
          printf("v: <ptr>,\n");
          break;
        }
        default:
          assert(0);
      }

      print_indent(spaces);
      printf(")");
      break;
    }
    case IRValue_VAR: {
      printf("IRValue(\n");

      print_indent(spaces + 2);
      printf("type = VAR,\n");

      print_indent(spaces + 2);
      printf("name = %s,\n", ir_val->as.var);

      print_indent(spaces);
      printf(")");
      break;
    }
    default:
      assert(0);
  }
}

static void print_ir_binary_op(enum IRInstrBinaryKind kind)
{
  switch (kind) {
    case IRInstrBinary_ADD:
      printf("ADD");
      break;
    case IRInstrBinary_SUB:
      printf("SUB");
      break;
    case IRInstrBinary_MUL:
      printf("MUL");
      break;
    case IRInstrBinary_DIV:
      printf("DIV");
      break;
    case IRInstrBinary_E:
      printf("EQUAL EQUAL");
      break;
    case IRInstrBinary_NE:
      printf("BANG EQUAL");
      break;
    case IRInstrBinary_L:
      printf("LESS");
      break;
    case IRInstrBinary_LE:
      printf("LESS EQUAL");
      break;
    case IRInstrBinary_G:
      printf("GREATER");
      break;
    case IRInstrBinary_GE:
      printf("GREATER EQUAL");
      break;
    case IRInstrBinary_BIT_AND:
      printf("BITWISE AND");
      break;
    case IRInstrBinary_BIT_XOR:
      printf("BITWISE XOR");
      break;
    case IRInstrBinary_BIT_OR:
      printf("BITWISE OR");
      break;
    case IRInstrBinary_SHL:
      printf("SHIFT LEFT");
      break;
    case IRInstrBinary_SHR:
      printf("SHIFT RIGHT LOGICAL");
      break;
    case IRInstrBinary_SAR:
      printf("SHIFT RIGHT ARITHMETIC");
      break;
    default:
      assert(0);
  }
}

static void print_ir_instr(struct IRInstr *instr, int spaces)
{
  print_indent(spaces);

  switch (instr->kind) {
    case IRInstr_LOAD: {
      printf("IRInstr_LOAD(\n");
      print_indent(spaces + 2);
      printf("src = ");
      print_ir_val(instr->as.load.src, spaces + 2);
      printf(",\n");
      print_indent(spaces + 2);
      printf("dst = ");
      print_ir_val(instr->as.load.dst, spaces + 2);
      printf("\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case IRInstr_STORE: {
      printf("IRInstr_STORE(\n");
      print_indent(spaces + 2);
      printf("val = ");
      print_ir_val(instr->as.store.val, spaces + 2);
      printf(",\n");
      print_indent(spaces + 2);
      printf("dst = ");
      print_ir_val(instr->as.store.dst, spaces + 2);
      printf("\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case IRInstr_GETADDR: {
      printf("IRInstr_GETADDR(\n");
      print_indent(spaces + 2);
      printf("src = ");
      print_ir_val(instr->as.getaddr.src, spaces + 2);
      printf(",\n");
      print_indent(spaces + 2);
      printf("dst = ");
      print_ir_val(instr->as.getaddr.dst, spaces + 2);
      printf("\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case IRInstr_CAST: {
      printf("IRInstr_CAST(\n");
      print_indent(spaces + 2);
      printf("src = ");
      print_ir_val(instr->as.cast.src, spaces + 2);
      printf(",\n");
      print_indent(spaces + 2);
      printf("dst = ");
      print_ir_val(instr->as.cast.dst, spaces + 2);
      printf("\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case IRInstr_UNARY: {
      printf("IRInstr_UNARY(\n");
      print_indent(spaces + 2);
      if (instr->as.unary.kind == IRInstrUnary_NOT) {
        printf("kind = NOT,\n");
      } else {
        printf("kind = NEG,\n");
      }
      print_indent(spaces + 2);
      printf("src = ");
      print_ir_val(instr->as.unary.src, spaces + 2);
      printf(",\n");
      print_indent(spaces + 2);
      printf("dst = ");
      print_ir_val(instr->as.unary.dst, spaces + 2);
      printf("\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case IRInstr_JMP: {
      printf("IRInstr_JMP(target = %s)", instr->as.jmp.target);
      break;
    }
    case IRInstr_JZ: {
      printf("IRInstr_JZ(\n");
      print_indent(spaces + 2);
      printf("target = %s,\n", instr->as.jz.target);
      print_indent(spaces + 2);
      printf("cond: ");
      print_ir_val(&instr->as.jz.cond, spaces + 2);
      printf(",\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case IRInstr_LBL: {
      printf("IRInstr_LBL(name = %s)", instr->as.label.name);
      break;
    }
    case IRInstr_CPY: {
      printf("IRInstr_CPY(\n");

      print_indent(spaces + 2);
      printf("src = ");
      print_ir_val(instr->as.copy.src, spaces + 2);
      printf(",\n");

      print_indent(spaces + 2);
      printf("dst = ");
      print_ir_val(instr->as.copy.dst, spaces + 2);
      printf("\n");

      print_indent(spaces);
      printf(")");
      break;
    }
    case IRInstr_BIN: {
      printf("IRInstr_BIN(\n");

      print_indent(spaces + 2);
      printf("type = ");
      print_ir_binary_op(instr->as.binary.kind);
      printf(",\n");

      print_indent(spaces + 2);
      printf("lhs = ");
      print_ir_val(instr->as.binary.lhs, spaces + 2);
      printf(",\n");

      print_indent(spaces + 2);
      printf("rhs = ");
      print_ir_val(instr->as.binary.rhs, spaces + 2);
      printf(",\n");

      print_indent(spaces + 2);
      printf("dst = ");
      print_ir_val(instr->as.binary.dst, spaces + 2);
      printf("\n");

      print_indent(spaces);
      printf(")");
      break;
    }
    case IRInstr_RET: {
      printf("IRInstr_RET(\n");

      print_indent(spaces + 2);
      printf("val = ");
      if (instr->as.ret.val) {
        print_ir_val(instr->as.ret.val, spaces + 2);
      }
      printf("\n");

      print_indent(spaces);
      printf(")");
      break;
    }
    case IRInstr_CALL: {
      printf("IRInstr_CALL(\n");

      print_indent(spaces + 2);
      printf("target = ");
      print_expr(&instr->as.call.target, spaces + 2);
      printf(",\n");

      print_indent(spaces + 2);
      printf("args: [\n");
      for (int k = 0; k < instr->as.call.args.len; k++) {
        print_indent(spaces + 4);
        print_ir_val(instr->as.call.args.data[k], spaces + 4);
        printf(",\n");
      }
      print_indent(spaces + 2);
      printf("],\n");

      print_indent(spaces + 2);
      printf("dst: ");
      if (instr->as.call.dst) {
        print_ir_val(instr->as.call.dst, spaces + 2);
      } else {
        printf("NULL");
      }
      printf("\n");

      print_indent(spaces);
      printf(")");
      break;
    }
    case IRInstr_CPY_FROM_OFFSET: {
      printf("IRInstr_CPY_FROM_OFFSET(\n");
      print_indent(spaces + 2);
      printf("src = ");
      print_ir_val(instr->as.cpy_from_offset.src, spaces + 2);
      printf(",\n");
      print_indent(spaces + 2);
      printf("offset = %d,\n", instr->as.cpy_from_offset.offset);
      print_indent(spaces + 2);
      printf("dst = ");
      print_ir_val(instr->as.cpy_from_offset.dst, spaces + 2);
      printf("\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case IRInstr_CPY_TO_OFFSET: {
      printf("IRInstr_CPY_TO_OFFSET(\n");
      print_indent(spaces + 2);
      printf("dst = ");
      print_ir_val(instr->as.cpy_to_offset.dst, spaces + 2);
      printf(",\n");
      print_indent(spaces + 2);
      printf("offset = %d,\n", instr->as.cpy_to_offset.offset);
      print_indent(spaces + 2);
      printf("src = ");
      print_ir_val(instr->as.cpy_to_offset.src, spaces + 2);
      printf("\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case IRInstr_ADD_PTR: {
      printf("IRInstr_ADD_PTR(\n");
      print_indent(spaces + 2);
      printf("ptr = ");
      print_ir_val(instr->as.add_ptr.ptr, spaces + 2);
      printf(",\n");
      print_indent(spaces + 2);
      printf("index = ");
      print_ir_val(instr->as.add_ptr.index, spaces + 2);
      printf(",\n");
      print_indent(spaces + 2);
      printf("scale = %d,\n", instr->as.add_ptr.scale);
      print_indent(spaces + 2);
      printf("dst = ");
      print_ir_val(instr->as.add_ptr.dst, spaces + 2);
      printf("\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    default:
      assert(0);
  }
}

static void print_ir_fn(struct IRFunction *func)
{
  printf("IRFunction(\n  name = %s,\n  retval = %d,\n  body = [\n", func->name,
         func->retval.kind);
  for (int i = 0; i < func->body.len; i++) {
    print_ir_instr(&func->body.data[i], 4);
    printf(",\n");
  }
  printf("  ]\n");
  printf(")\n");
}

void print_ir(struct IRProgram *prog)
{
  for (int i = 0; i < prog->funcs.len; i++) {
    print_ir_fn(prog->funcs.data[i]);
  }
}
#endif

