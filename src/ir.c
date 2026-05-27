#include "ir.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "typechecker.h"
#include "util.h"
#include "vector.h"

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
    case IRInstr_SPAWN: {
      for (int i = 0; i < instr->as.spawn.args.len; i++) {
        free_ir_val(instr->as.spawn.args.data[i]);
      }
      vec_free(&instr->as.spawn.args);

      if (instr->as.spawn.dst) {
        free_ir_val(instr->as.spawn.dst);
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

      /*
       * Pointer arithmetic must not be lowered as a plain integer ADD/SUB.
       * A source expression like `arr + x` means:
       *
       *     (char *)arr + x * sizeof(*arr)
       *
       * If we emit a normal IRInstr_BIN ADD here, codegen may generate invalid
       * mixed-width assembly such as `addq %ebx, %r8`, and even if that
       * assembled it would still be wrong because the index would not be scaled.
       */
      if ((expr->as.binary.kind == EXPR_BIN_ADD ||
           expr->as.binary.kind == EXPR_BIN_SUB) &&
          lhs->type.kind == PTR_T && is_integer_type(rhs->type.kind)) {
        int scale;

        assert(lhs->type.as.base &&
               "pointer arithmetic should have a sized pointee after typecheck");
        scale = get_type_size(*lhs->type.as.base);
        assert(scale > 0);

        if (expr->as.binary.kind == EXPR_BIN_SUB) {
          scale = -scale;
        }

        vec_insert(instrs, ((struct IRInstr){
                              .kind = IRInstr_ADD_PTR,
                              .as.add_ptr = {.ptr = lhs,
                                             .index = rhs,
                                             .scale = scale,
                                             .dst = clone_irval(dst)}}));

        return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = dst};
      }

      if (expr->as.binary.kind == EXPR_BIN_ADD &&
          is_integer_type(lhs->type.kind) && rhs->type.kind == PTR_T) {
        int scale;

        assert(rhs->type.as.base &&
               "pointer arithmetic should have a sized pointee after typecheck");
        scale = get_type_size(*rhs->type.as.base);
        assert(scale > 0);

        vec_insert(instrs, ((struct IRInstr){
                              .kind = IRInstr_ADD_PTR,
                              .as.add_ptr = {.ptr = rhs,
                                             .index = lhs,
                                             .scale = scale,
                                             .dst = clone_irval(dst)}}));

        return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = dst};
      }

      if (expr->as.binary.kind == EXPR_BIN_SUB && (lhs->type.kind == PTR_T && rhs->type.kind == PTR_T)) {
        int scale;

	scale = get_type_size(*lhs->type.as.base);

	struct IRValue *ptr1, *ptr2, *ptrdiff, scale_var, *ptrdiff_dst;

	ptr1 = irfy_expr_and_convert(instrs, expr->as.binary.lhs);
	ptr2 = irfy_expr_and_convert(instrs, expr->as.binary.rhs);

	ptrdiff = mkirvar();
	ptrdiff->type = (Type){.kind = I64_T};

	scale_var.kind = IRValue_CONST;
	scale_var.type = (Type){.kind = I64_T};
	scale_var.as.konst.as.i64 = (long long) scale;

	ptrdiff_dst = mkirvar();
	ptrdiff_dst->type = (Type){.kind = I64_T};

	struct IRInstr i1, i2;

	struct IRInstr_Binary sub;
	sub.kind = IRInstrBinary_SUB;
	sub.lhs = ptr1;
	sub.rhs = ptr2;
	sub.dst = clone_irval(ptrdiff);

	i1.kind = IRInstr_BIN;
	i1.as.binary = sub;

	vec_insert(instrs, i1);

	struct IRInstr_Binary div;
	div.kind = IRInstrBinary_DIV;
	div.lhs = ptrdiff;
	div.rhs = ALLOC(scale_var);
	div.dst = clone_irval(ptrdiff_dst);

	i2.kind = IRInstr_BIN;
	i2.as.binary = div;

	vec_insert(instrs, i2);

	return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = ptrdiff_dst};
      }

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
    case EXPR_AWAIT: {
      VecIRValuePtr args = {0};
      vec_insert(&args,
                 irfy_expr_and_convert(instrs, expr->as.await_expr.expr));

      struct IRValue *dst = mkirvar();
      dst->type = expr->type;

      struct Expr target = {0};
      target.kind = EXPR_VARIABLE;
      target.as.var.name = strdup("__x_task_await");
      target.as.var.type = (Type){.kind = UNKNOWN_T};

      struct IRInstr instr = {0};
      instr.kind = IRInstr_CALL;
      instr.as.call.target = target;
      instr.as.call.args = args;
      instr.as.call.dst = clone_irval(dst);
      vec_insert(instrs, instr);

      return (struct ExpResult){.kind = EXPRESULT_PLAIN, .as.plain = dst};
    }
        case EXPR_SIZEOF: {
      struct ExpResult result;
      struct IRValue sz;

      sz.kind = IRValue_CONST;
      sz.as.konst = (struct Literal){
          .kind = LITERAL_NUM,
          .as.u64 = (unsigned long long) get_type_size(
              expr->as.sizeof_expr.expr->type)};
      sz.type = (Type){.kind = U64_T};

      result.kind = EXPRESULT_PLAIN;
      result.as.plain = ALLOC(sz);

      return result;
    }
    case EXPR_SIZEOF_T: {
      struct ExpResult result;
      struct IRValue sz;

      sz.kind = IRValue_CONST;
      sz.as.konst = (struct Literal){
          .kind = LITERAL_NUM,
          .as.u64 = (unsigned long long) get_type_size(
              expr->as.sizeoft_expr.target_type)};
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
    case STMT_YIELD: {
      struct Expr target = {0};
      target.kind = EXPR_VARIABLE;
      target.as.var.name = strdup("__x_task_yield");
      target.as.var.type = (Type){.kind = UNKNOWN_T};

      struct IRInstr instr = {0};
      instr.kind = IRInstr_CALL;
      instr.as.call.target = target;
      instr.as.call.args = (VecIRValuePtr){0};
      instr.as.call.dst = NULL;
      vec_insert(instrs, instr);
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
    return format_directive(".byte %llu", literal->as.boolean ? 1ULL : 0ULL);
  }

  if (literal->kind != LITERAL_NUM) {
    return NULL;
  }

  switch (variable->type.kind) {
    case I8_T:
      return format_signed_directive(
          ".byte %lld", is_negative ? -literal->as.i8 : literal->as.i8);
    case U8_T:
      if (is_negative) {
        return NULL;
      }
      return format_directive(".byte %llu", literal->as.u8);
    case I16_T:
      return format_signed_directive(
          ".value %lld", is_negative ? -literal->as.i16 : literal->as.i16);
    case U16_T:
      if (is_negative) {
        return NULL;
      }
      return format_directive(".value %llu", literal->as.u16);
    case I32_T:
      return format_signed_directive(
          ".long %lld", is_negative ? -literal->as.i32 : literal->as.i32);
    case U32_T:
      if (is_negative) {
        return NULL;
      }
      return format_directive(".long %llu", literal->as.u32);
    case I64_T:
      return format_signed_directive(
          ".quad %lld", is_negative ? -literal->as.i64 : literal->as.i64);
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

/* Stackless async lowering
 *
 * Async functions are lowered into two ordinary functions:
 *   f(args...)              -> task      constructor/wrapper
 *   f__async_step(task)     -> i64       one poll of the state machine
 *
 * The step function never keeps async locals on its C stack across a suspend.
 * Instead every IR pseudo used by the original async body gets a slot in the
 * task frame.  Each poll loads operands out of the frame, executes until the
 * next suspension point, stores destinations back to the frame, records the
 * next program counter, and returns 0.  Returning 1 means the task completed.
 */

typedef Vector(char *) VecCharPtr;

typedef struct AsyncSlot {
  char *name;
  Type type;
  int slot;
} AsyncSlot;

typedef Vector(AsyncSlot) VecAsyncSlot;

/* "Compiler notebook" carried through the async lowering pass.
 *
 * It answers to questions while rewriting an async function:
 *   1)  Which async variable/temporary lives in which frame slot?
 *   2)  What is the synthetic IR variable name for the current task?
 *
 * When the compiler turns an async function into a state machine,
 * locals can no longer safely live in ordinary temporaries and registers,
 * because the function can suspend.  They must live in the task frame. So,
 * the compiler needs a mapping:
 *
 *   x        -> frame slot 0
 *   y        -> frame slot 1
 *   tmp.17   -> frame slot 2
 *   tmp.18   -> frame slot 3
 *
 * ...and this is stored in `ctx->slots`.
 *
 * So one entry might mean:
 *
 *   name = "y"
 *   type = i64
 *   slot = 1
 *
 * ... Then whenever lowered code wants to read `y`, it can emit:
 *
 *   y = __x_task_frame_get(task, 1)
 *
 * ... and whenever it wants to write `y`, it can emit:
 *
 *   __x_task_frame_set(task, 1, value)
 *
 * The generated step function has a hidden task parameter.
 *
 * Conceptually:
 *
 *   i64 f__async_step(task self) {
 *     ...
 *   }
 *
 * ...but in the IR, that task parameter needs a variable name, which the
 * lowering creates:
 *
 *   ctx.task_name = mkstr("var.__async_task.%d", mktmp());
 *
 * ...So inside helpers like this:
 *
 *   static struct IRValue *async_task_value(struct AsyncLowerCtx *ctx)
 *   {
 *     return ir_named_var(ctx->task_name, (Type){.kind = TASK_T});
 *   }
 *
 * ...the compiler can conveniently generate IR for “current task handle”.  */
struct AsyncLowerCtx {
  VecAsyncSlot slots;
  char *task_name;
};

static bool ir_target_is(struct Expr *target, const char *name)
{
  return target && target->kind == EXPR_VARIABLE && target->as.var.name &&
         strcmp(target->as.var.name, name) == 0;
}

static bool ir_value_is_frame_candidate(struct IRValue *v)
{
  return v && v->kind == IRValue_VAR && !is_data_variable(v->as.var);
}

static int async_find_slot(struct AsyncLowerCtx *ctx, const char *name)
{
  for (int i = 0; i < ctx->slots.len; i++) {
    if (strcmp(ctx->slots.data[i].name, name) == 0) {
      return i;
    }
  }
  return -1;
}

static int async_add_slot(struct AsyncLowerCtx *ctx, const char *name,
                          Type type)
{
  int existing = async_find_slot(ctx, name);
  if (existing >= 0) {
    return existing;
  }

  AsyncSlot slot = {
      .name = strdup(name), .type = clone_type(type), .slot = ctx->slots.len};
  vec_insert(&ctx->slots, slot);
  return slot.slot;
}

static struct IRValue *ir_const_u64(unsigned long long x)
{
  struct IRValue *v = malloc(sizeof(struct IRValue));
  v->kind = IRValue_CONST;
  v->type = (Type){.kind = U64_T};
  v->as.konst.kind = LITERAL_NUM;
  v->as.konst.type = (Type){.kind = U64_T};
  v->as.konst.as.u64 = x;
  return v;
}

static struct IRValue *ir_const_i64(long long x)
{
  struct IRValue *v = malloc(sizeof(struct IRValue));
  v->kind = IRValue_CONST;
  v->type = (Type){.kind = I64_T};
  v->as.konst.kind = LITERAL_NUM;
  v->as.konst.type = (Type){.kind = I64_T};
  v->as.konst.as.i64 = x;
  return v;
}

static struct IRValue *ir_named_var(const char *name, Type type)
{
  struct IRValue *v = malloc(sizeof(struct IRValue));
  v->kind = IRValue_VAR;
  v->as.var = strdup(name);
  v->type = clone_type(type);
  return v;
}

static struct Expr ir_func_target(const char *name)
{
  struct Expr target = {0};
  target.kind = EXPR_VARIABLE;
  target.as.var.name = strdup(name);
  target.as.var.type = (Type){.kind = UNKNOWN_T};
  return target;
}

static void ir_emit_call(VecIRInstr *out, const char *name, VecIRValuePtr args,
                         struct IRValue *dst)
{
  struct IRInstr instr = {0};
  instr.kind = IRInstr_CALL;
  instr.as.call.target = ir_func_target(name);
  instr.as.call.args = args;
  instr.as.call.dst = clone_irval(dst);
  vec_insert(out, instr);
}

static struct IRValue *async_task_value(struct AsyncLowerCtx *ctx)
{
  return ir_named_var(ctx->task_name, (Type){.kind = TASK_T});
}

static struct IRValue *async_frame_get(VecIRInstr *out,
                                       struct AsyncLowerCtx *ctx, int slot,
                                       Type type)
{
  VecIRValuePtr args = {0};
  vec_insert(&args, async_task_value(ctx));
  vec_insert(&args, ir_const_u64((unsigned long long) slot));

  struct IRValue *dst = mkirvar();
  dst->type = clone_type(type);
  ir_emit_call(out, "__x_task_frame_get", args, dst);
  return dst;
}

static void async_frame_set(VecIRInstr *out, struct AsyncLowerCtx *ctx,
                            int slot, struct IRValue *value)
{
  VecIRValuePtr args = {0};
  vec_insert(&args, async_task_value(ctx));
  vec_insert(&args, ir_const_u64((unsigned long long) slot));
  vec_insert(&args, clone_irval(value));
  ir_emit_call(out, "__x_task_frame_set", args, NULL);
}

/*
 * Mental model for:
 *  - async_materialize_value,
 *  - async_prepare_dst
 *  - async_store_dst_if_needed:
 *
 * These helpers implement this rule:
 *
 * When reading a frame-backed variable:
 *   frame_get into temp
 *
 * When writing a frame-backed variable:
 *   compute into temp
 *   frame_set from temp  */
static struct IRValue *async_materialize_value(VecIRInstr *out,
                                               struct AsyncLowerCtx *ctx,
                                               struct IRValue *value)
{
  /* In the state-machine transform, many original IR variables
   * no longer live as ordinary locals.  They live in the task frame.
   * So before an instruction can use one of those variables, the compiler
   * may need to emit a frame load.
   *
   * Because a frame-backed variable is not directly available as a
   * normal IR value anymore. It exists inside `task->frame[slot]`,
   * and to use it in an instruction, the compiler must “materialize”
   * it into a temporary value for the current poll.
   *
   * So:
   *
   * frame slot value -> ordinary IR temporary  */

  /* If this value is not something that should live in the frame, just clone
   * it.  */
  if (!ir_value_is_frame_candidate(value)) {
    return clone_irval(value);
  }

  /* Does this value live in the frame?  */
  int slot = async_find_slot(ctx, value->as.var);
  if (slot < 0) {
    /* No, it does not.  Clone it.  */
    return clone_irval(value);
  }

  /* Yes it does, go get it.  */
  return async_frame_get(out, ctx, slot, value->type);
}

static struct IRValue *async_prepare_dst(struct AsyncLowerCtx *ctx,
                                         struct IRValue *dst, bool *needs_store)
{
  /* Start by assuming that no frame store is needed.  */
  *needs_store = false;

  /* If dst is not a frame candidate, just use it normally.  */
  if (!ir_value_is_frame_candidate(dst)) {
    return clone_irval(dst);
  }

  /* Does dst live in the frame?  */
  int slot = async_find_slot(ctx, dst->as.var);
  if (slot < 0) {
    /* No, it does not, clone it and use it normally.  */
    return clone_irval(dst);
  }

  /* It lives in the frame.  Make a temp.  */
  struct IRValue *local = mkirvar();
  local->type = clone_type(dst->type);

  *needs_store = true;

  return local;
}

static void async_store_dst_if_needed(VecIRInstr *out,
                                      struct AsyncLowerCtx *ctx,
                                      struct IRValue *original_dst,
                                      struct IRValue *local_dst,
                                      bool needs_store)
{
  /* If no store is needed, do nothing.  */
  if (!needs_store) {
    return;
  }

  int slot = async_find_slot(ctx, original_dst->as.var);

  /* The assert is valid because this helper should only be called
   * with `needs_store = true`, which only happens when a slot was found.  */
  assert(slot >= 0);

  async_frame_set(out, ctx, slot, local_dst);
}

static void async_collect_value(struct AsyncLowerCtx *ctx, struct IRValue *v)
{
  if (ir_value_is_frame_candidate(v)) {
    async_add_slot(ctx, v->as.var, v->type);
  }
}

static void async_collect_instr_vars(struct AsyncLowerCtx *ctx,
                                     struct IRInstr *instr)
{
  switch (instr->kind) {
    case IRInstr_BIN:
      async_collect_value(ctx, instr->as.binary.lhs);
      async_collect_value(ctx, instr->as.binary.rhs);
      async_collect_value(ctx, instr->as.binary.dst);
      break;
    case IRInstr_UNARY:
      async_collect_value(ctx, instr->as.unary.src);
      async_collect_value(ctx, instr->as.unary.dst);
      break;
    case IRInstr_RET:
      async_collect_value(ctx, instr->as.ret.val);
      break;
    case IRInstr_CPY:
      async_collect_value(ctx, instr->as.copy.src);
      async_collect_value(ctx, instr->as.copy.dst);
      break;
    case IRInstr_CALL:
      for (int i = 0; i < instr->as.call.args.len; i++) {
        async_collect_value(ctx, instr->as.call.args.data[i]);
      }
      async_collect_value(ctx, instr->as.call.dst);
      break;
    case IRInstr_SPAWN:
      for (int i = 0; i < instr->as.spawn.args.len; i++) {
        async_collect_value(ctx, instr->as.spawn.args.data[i]);
      }
      async_collect_value(ctx, instr->as.spawn.dst);
      break;
    case IRInstr_JZ:
      async_collect_value(ctx, &instr->as.jz.cond);
      break;
    case IRInstr_GETADDR:
      async_collect_value(ctx, instr->as.getaddr.src);
      async_collect_value(ctx, instr->as.getaddr.dst);
      break;
    case IRInstr_LOAD:
      async_collect_value(ctx, instr->as.load.src);
      async_collect_value(ctx, instr->as.load.dst);
      break;
    case IRInstr_STORE:
      async_collect_value(ctx, instr->as.store.val);
      async_collect_value(ctx, instr->as.store.dst);
      break;
    case IRInstr_CAST:
      async_collect_value(ctx, instr->as.cast.src);
      async_collect_value(ctx, instr->as.cast.dst);
      break;
    case IRInstr_CPY_FROM_OFFSET:
      async_collect_value(ctx, instr->as.cpy_from_offset.src);
      async_collect_value(ctx, instr->as.cpy_from_offset.dst);
      break;
    case IRInstr_CPY_TO_OFFSET:
      async_collect_value(ctx, instr->as.cpy_to_offset.dst);
      async_collect_value(ctx, instr->as.cpy_to_offset.src);
      break;
    case IRInstr_ADD_PTR:
      async_collect_value(ctx, instr->as.add_ptr.ptr);
      async_collect_value(ctx, instr->as.add_ptr.index);
      async_collect_value(ctx, instr->as.add_ptr.dst);
      break;
    case IRInstr_JMP:
    case IRInstr_LBL:
      break;
    default:
      assert(0);
  }
}

static bool async_instr_is_io_wait(struct IRInstr *instr)
{
  return instr->kind == IRInstr_CALL &&
         (ir_target_is(&instr->as.call.target, "__x_io_wait_read") ||
          ir_target_is(&instr->as.call.target, "__x_io_wait_write") ||
          ir_target_is(&instr->as.call.target, "__x_sleep_ms"));
}

static const char *async_wait_runtime_helper(struct IRInstr *instr)
{
  assert(async_instr_is_io_wait(instr));
  if (ir_target_is(&instr->as.call.target, "__x_io_wait_read")) {
    return "__x_task_wait_read";
  }
  if (ir_target_is(&instr->as.call.target, "__x_io_wait_write")) {
    return "__x_task_wait_write";
  }
  if (ir_target_is(&instr->as.call.target, "__x_sleep_ms")) {
    return "__x_task_sleep_ms";
  }
  assert(0 && "unreachable async wait helper");
}

static bool async_instr_is_suspend(struct IRInstr *instr)
{
  return instr->kind == IRInstr_CALL &&
         (ir_target_is(&instr->as.call.target, "__x_task_yield") ||
          ir_target_is(&instr->as.call.target, "__x_task_await") ||
          async_instr_is_io_wait(instr));
}

static VecCharPtr async_collect_resume_labels(VecIRInstr *orig)
{
  VecCharPtr labels = {0};
  for (int i = 0; i < orig->len; i++) {
    if (async_instr_is_suspend(&orig->data[i])) {
      vec_insert(&labels, mklbl("AsyncResume", mktmp()));
    }
  }
  return labels;
}

static VecCharPtr async_collect_preempt_labels(VecIRInstr *orig)
{
  VecCharPtr labels = {0};
  for (int i = 0; i < orig->len; i++) {
    if (async_instr_is_suspend(&orig->data[i]) ||
        orig->data[i].kind == IRInstr_RET) {
      vec_insert(&labels, NULL);
    } else {
      vec_insert(&labels, mklbl("AsyncPreempt", mktmp()));
    }
  }
  return labels;
}

static VecCharPtr async_concat_pc_labels(VecCharPtr *resume_labels,
                                         VecCharPtr *preempt_labels)
{
  VecCharPtr labels = {0};
  for (int i = 0; i < resume_labels->len; i++) {
    vec_insert(&labels, resume_labels->data[i]);
  }
  for (int i = 0; i < preempt_labels->len; i++) {
    if (preempt_labels->data[i]) {
      vec_insert(&labels, preempt_labels->data[i]);
    }
  }
  return labels;
}

static void async_emit_ret_status(VecIRInstr *out, long long status)
{
  struct IRInstr ret = {0};
  ret.kind = IRInstr_RET;
  ret.as.ret.val = ir_const_i64(status);
  vec_insert(out, ret);
}

static void async_emit_set_pc(VecIRInstr *out, struct AsyncLowerCtx *ctx,
                              int pc)
{
  VecIRValuePtr args = {0};
  vec_insert(&args, async_task_value(ctx));
  vec_insert(&args, ir_const_u64((unsigned long long) pc));
  ir_emit_call(out, "__x_task_set_pc", args, NULL);
}

static void async_emit_preempt_point(VecIRInstr *out, struct AsyncLowerCtx *ctx,
                                     int pc, char *resume_label)
{
  char *continue_label = mklbl("AsyncTickContinue", mktmp());

  struct IRInstr resume = {0};
  resume.kind = IRInstr_LBL;
  resume.as.label.name = strdup(resume_label);
  vec_insert(out, resume);

  VecIRValuePtr tick_args = {0};
  vec_insert(&tick_args, async_task_value(ctx));

  struct IRValue *should_preempt = mkirvar();
  should_preempt->type = (Type){.kind = BOOL_T};
  ir_emit_call(out, "__x_task_tick", tick_args, should_preempt);

  struct IRInstr jz = {0};
  jz.kind = IRInstr_JZ;
  jz.as.jz.cond = *should_preempt;
  free(should_preempt);
  jz.as.jz.target = strdup(continue_label);
  vec_insert(out, jz);

  async_emit_set_pc(out, ctx, pc);
  async_emit_ret_status(out, 0);

  struct IRInstr cont = {0};
  cont.kind = IRInstr_LBL;
  cont.as.label.name = continue_label;
  vec_insert(out, cont);
}

static void async_emit_dispatch(VecIRInstr *out, struct AsyncLowerCtx *ctx,
                                VecCharPtr *resume_labels, char *start_label)
{
  /* u64 pc = __x_task_get_pc(self); */
  VecIRValuePtr pc_args = {0};

  vec_insert(&pc_args, async_task_value(ctx));

  struct IRValue *pc = mkirvar();
  pc->type = (Type){.kind = U64_T};

  ir_emit_call(out, "__x_task_get_pc", pc_args, pc);

  for (int i = 0; i <= resume_labels->len; i++) {
    char *target_label = i == 0 ? start_label : resume_labels->data[i - 1];
    char *next_label = mklbl("AsyncDispatchNext", mktmp());

    /* bool eq = pc == i; */
    struct IRValue *eq = mkirvar();
    eq->type = (Type){.kind = BOOL_T};

    struct IRInstr cmp = {0};
    cmp.kind = IRInstr_BIN;
    cmp.as.binary.kind = IRInstrBinary_E;
    cmp.as.binary.lhs = clone_irval(pc);
    cmp.as.binary.rhs = ir_const_u64((unsigned long long) i);
    cmp.as.binary.dst = clone_irval(eq);

    vec_insert(out, cmp);

    /* if (!eq) goto next_label */
    struct IRInstr jz = {0};
    jz.kind = IRInstr_JZ;
    jz.as.jz.cond = *eq;

    free(eq);

    jz.as.jz.target = strdup(next_label);

    vec_insert(out, jz);

    /* goto target_label */
    struct IRInstr jmp = {0};
    jmp.kind = IRInstr_JMP;
    jmp.as.jmp.target = strdup(target_label);
    vec_insert(out, jmp);

    struct IRInstr lbl = {0};
    lbl.kind = IRInstr_LBL;
    lbl.as.label.name = next_label;
    vec_insert(out, lbl);
  }

  /*   __x_async_bad_pc(task);
      return 1;  */
  VecIRValuePtr bad_args = {0};
  vec_insert(&bad_args, async_task_value(ctx));
  ir_emit_call(out, "__x_async_bad_pc", bad_args, NULL);
  async_emit_ret_status(out, 1);

  /* This is where the transformed original function body begins.
   *
   * So after `async_emit_dispatch`, the output body looks like:
   *
   *   pc = __x_task_get_pc(task);
   *
   *   if pc == 0 goto AsyncStart
   *   if pc == 1 goto AsyncResume0
   *   if pc == 2 goto AsyncResume1
   *
   *   ...
   *
   *   __x_async_bad_pc(task)
   *   ret 1
   *
   *   AsyncStart:
   *     ...
   *
   * Then `async_transform_body` continues appending the transformed
   * original instructions after that `AsyncStart:` label. */
  struct IRInstr start = {0};
  start.kind = IRInstr_LBL;
  start.as.label.name = strdup(start_label);
  vec_insert(out, start);
}

/* async_transform_io_wait
 *   rewrites async runtime wait calls into non-blocking state-machine waits.
 *
 * Source inside an async function may contain calls such as:
 *
 *   __x_io_wait_read(fd);
 *   __x_io_wait_write(fd);
 *   __x_sleep_ms(ms);
 *
 * In synchronous code those are ordinary blocking runtime calls.  Inside an
 * async step function, though, they are suspension markers.  The transform
 * registers the current task with the runtime wait queue, saves the resume pc,
 * returns pending, then resumes after the original call once the event loop
 * wakes the task.  */
static void async_transform_io_wait(VecIRInstr *out, struct AsyncLowerCtx *ctx,
                                    struct IRInstr *instr, int pc,
                                    char *resume_label)
{
  assert(instr->as.call.args.len == 1);

  struct IRValue *wait_arg =
      async_materialize_value(out, ctx, instr->as.call.args.data[0]);

  VecIRValuePtr wait_args = {0};
  vec_insert(&wait_args, async_task_value(ctx));
  vec_insert(&wait_args, wait_arg);
  ir_emit_call(out, async_wait_runtime_helper(instr), wait_args, NULL);

  async_emit_set_pc(out, ctx, pc);
  async_emit_ret_status(out, 0);

  struct IRInstr resume = {0};
  resume.kind = IRInstr_LBL;
  resume.as.label.name = strdup(resume_label);
  vec_insert(out, resume);

  if (instr->as.call.dst) {
    bool store_dst = false;
    struct IRValue *dst =
        async_prepare_dst(ctx, instr->as.call.dst, &store_dst);
    struct IRInstr zero = {0};
    zero.kind = IRInstr_CPY;
    zero.as.copy.src = ir_const_i64(0);
    zero.as.copy.dst = clone_irval(dst);
    vec_insert(out, zero);
    async_store_dst_if_needed(out, ctx, instr->as.call.dst, dst, store_dst);
  }
}

/* async_transform_await
 *   rewrites `await t` inside an async function into
 *   non-blocking state-machine logic.
 *
 * Inside a normal synchronous function, the `__x_task_await(t)` marker
 * can block and run the scheduler until t finishes.
 *
 * But inside an async step function, we do not want to block.
 * The step function should run a little bit, then return `0`
 * if it cannot continue yet.
 *
 * So `await` inside async becomes:
 *
 *   ResumeLabel:
 *     child = load child task;
 *     if (!__x_task_is_done(child)) {
 *         __x_task_set_pc(self, pc);
 *         return 0;
 *     }
 *
 *     result = __x_task_take_result(child);
 *     store result into await destination;
 *     goto AwaitEnd;
 *
 *   AwaitEnd:
 *     continue...  */
static void async_transform_await(VecIRInstr *out, struct AsyncLowerCtx *ctx,
                                  struct IRInstr *instr, int pc,
                                  char *resume_label)
{
  assert(instr->as.call.args.len == 1);

  char *suspend_label, *end_label;

  suspend_label = mklbl("AsyncAwaitSuspend", mktmp());
  end_label = mklbl("AsyncAwaitEnd", mktmp());

  /* Emit `AsyncResume:` label.  */
  struct IRInstr resume = {0};
  resume.kind = IRInstr_LBL;
  resume.as.label.name = strdup(resume_label);
  vec_insert(out, resume);

  /* Gets the task handle we are awaiting.
   * If the awaited task handle lives in the async frame,
   * this emits something like:
   *
   *   tmp_task = __x_task_frame_get(self, SLOT_child_task)
   *
   * If it is already a usable local value or constant, it
   * just clones it.  */
  struct IRValue *task_to_wait =
      async_materialize_value(out, ctx, instr->as.call.args.data[0]);

  /* done = __x_task_is_done(task_to_wait) */
  VecIRValuePtr done_args = {0};
  vec_insert(&done_args, clone_irval(task_to_wait));
  struct IRValue *done = mkirvar();
  done->type = (Type){.kind = BOOL_T};
  ir_emit_call(out, "__x_task_is_done", done_args, done);

  /* if (!done) goto suspend_label;  */
  struct IRInstr jz = {0};
  jz.kind = IRInstr_JZ;
  jz.as.jz.cond = *done;

  free(done);
  jz.as.jz.target = strdup(suspend_label);

  vec_insert(out, jz);

  /* If done, take the result.  */
  VecIRValuePtr result_args = {0};
  vec_insert(&result_args, clone_irval(task_to_wait));

  /* If the original await had a destination,
   * like: `let v: i64 = await t;`,
   * the transform needs somewhere to put the result.
   *
   * Again, async_prepare_dst decides:
   *   - Can I write directly to the destination?
   *   - Or should I write to a temporary and later
   *     store into the async frame?  */
  struct IRValue *await_dst = NULL;
  bool store_dst = false;
  if (instr->as.call.dst) {
    await_dst = async_prepare_dst(ctx, instr->as.call.dst, &store_dst);
  }

  /* await_dst = __x_task_take_result(task_to_wait); */
  ir_emit_call(out, "__x_task_take_result", result_args, await_dst);

  if (instr->as.call.dst) {
    async_store_dst_if_needed(out, ctx, instr->as.call.dst, await_dst,
                              store_dst);
  }

  /* If the task was done and we took the result, we should skip
   * the suspend block. */
  struct IRInstr jmp_end = {0};
  jmp_end.kind = IRInstr_JMP;
  jmp_end.as.jmp.target = strdup(end_label);
  vec_insert(out, jmp_end);

  /* Emit suspend block.  */
  struct IRInstr suspend = {0};
  suspend.kind = IRInstr_LBL;
  suspend.as.label.name = suspend_label;
  vec_insert(out, suspend);

  /* The child task is not ready.
   * Save its pc so it resumes at this await check.
   * Return pending to the scheduler.  */
  async_emit_set_pc(out, ctx, pc);
  async_emit_ret_status(out, 0);

  /* Emit the end label.  */
  struct IRInstr end = {0};
  end.kind = IRInstr_LBL;
  end.as.label.name = end_label;
  vec_insert(out, end);
}

/* async_transform_regular_instr
     rewrites ordinary IR instructions so their inputs/outputs
     go through the async task frame. */
static void async_transform_regular_instr(VecIRInstr *out,
                                          struct AsyncLowerCtx *ctx,
                                          struct IRInstr *instr)
{
  switch (instr->kind) {
    case IRInstr_BIN: {
      bool store = false;
      struct IRValue *dst =
          async_prepare_dst(ctx, instr->as.binary.dst, &store);
      struct IRInstr ni = {0};
      ni.kind = IRInstr_BIN;
      ni.as.binary.kind = instr->as.binary.kind;
      ni.as.binary.lhs =
          async_materialize_value(out, ctx, instr->as.binary.lhs);
      ni.as.binary.rhs =
          async_materialize_value(out, ctx, instr->as.binary.rhs);
      ni.as.binary.dst = clone_irval(dst);
      vec_insert(out, ni);
      async_store_dst_if_needed(out, ctx, instr->as.binary.dst, dst, store);
      break;
    }
    case IRInstr_UNARY: {
      bool store = false;
      struct IRValue *dst = async_prepare_dst(ctx, instr->as.unary.dst, &store);
      struct IRInstr ni = {0};
      ni.kind = IRInstr_UNARY;
      ni.as.unary.kind = instr->as.unary.kind;
      ni.as.unary.src = async_materialize_value(out, ctx, instr->as.unary.src);
      ni.as.unary.dst = clone_irval(dst);
      vec_insert(out, ni);
      async_store_dst_if_needed(out, ctx, instr->as.unary.dst, dst, store);
      break;
    }
    case IRInstr_CPY: {
      bool store = false;
      struct IRValue *dst = async_prepare_dst(ctx, instr->as.copy.dst, &store);
      struct IRInstr ni = {0};
      ni.kind = IRInstr_CPY;
      ni.as.copy.src = async_materialize_value(out, ctx, instr->as.copy.src);
      ni.as.copy.dst = clone_irval(dst);
      vec_insert(out, ni);
      async_store_dst_if_needed(out, ctx, instr->as.copy.dst, dst, store);
      break;
    }
    case IRInstr_CALL: {
      VecIRValuePtr args = {0};
      for (int i = 0; i < instr->as.call.args.len; i++) {
        vec_insert(&args, async_materialize_value(out, ctx,
                                                  instr->as.call.args.data[i]));
      }
      bool store = false;
      struct IRValue *dst = NULL;
      if (instr->as.call.dst) {
        dst = async_prepare_dst(ctx, instr->as.call.dst, &store);
      }
      struct IRInstr ni = {0};
      ni.kind = IRInstr_CALL;
      ni.as.call.target = instr->as.call.target;
      ni.as.call.args = args;
      ni.as.call.dst = clone_irval(dst);
      vec_insert(out, ni);
      if (instr->as.call.dst) {
        async_store_dst_if_needed(out, ctx, instr->as.call.dst, dst, store);
      }
      break;
    }
    case IRInstr_JMP: {
      struct IRInstr ni = {0};
      ni.kind = IRInstr_JMP;
      ni.as.jmp.target = strdup(instr->as.jmp.target);
      vec_insert(out, ni);
      break;
    }
    case IRInstr_JZ: {
      struct IRValue *cond =
          async_materialize_value(out, ctx, &instr->as.jz.cond);
      struct IRInstr ni = {0};
      ni.kind = IRInstr_JZ;
      ni.as.jz.cond = *cond;
      free(cond);
      ni.as.jz.target = strdup(instr->as.jz.target);
      vec_insert(out, ni);
      break;
    }
    case IRInstr_LBL: {
      struct IRInstr ni = {0};
      ni.kind = IRInstr_LBL;
      ni.as.label.name = strdup(instr->as.label.name);
      vec_insert(out, ni);
      break;
    }
    case IRInstr_LOAD: {
      bool store = false;
      struct IRValue *dst = async_prepare_dst(ctx, instr->as.load.dst, &store);
      struct IRInstr ni = {0};
      ni.kind = IRInstr_LOAD;
      ni.as.load.src = async_materialize_value(out, ctx, instr->as.load.src);
      ni.as.load.dst = clone_irval(dst);
      vec_insert(out, ni);
      async_store_dst_if_needed(out, ctx, instr->as.load.dst, dst, store);
      break;
    }
    case IRInstr_STORE: {
      struct IRInstr ni = {0};
      ni.kind = IRInstr_STORE;
      ni.as.store.val = async_materialize_value(out, ctx, instr->as.store.val);
      ni.as.store.dst = async_materialize_value(out, ctx, instr->as.store.dst);
      vec_insert(out, ni);
      break;
    }
    case IRInstr_CAST: {
      bool store = false;
      struct IRValue *dst = async_prepare_dst(ctx, instr->as.cast.dst, &store);
      struct IRInstr ni = {0};
      ni.kind = IRInstr_CAST;
      ni.as.cast.kind = instr->as.cast.kind;
      ni.as.cast.src = async_materialize_value(out, ctx, instr->as.cast.src);
      ni.as.cast.dst = clone_irval(dst);
      vec_insert(out, ni);
      async_store_dst_if_needed(out, ctx, instr->as.cast.dst, dst, store);
      break;
    }
    case IRInstr_ADD_PTR: {
      bool store = false;
      struct IRValue *dst =
          async_prepare_dst(ctx, instr->as.add_ptr.dst, &store);
      struct IRInstr ni = {0};
      ni.kind = IRInstr_ADD_PTR;
      ni.as.add_ptr.ptr =
          async_materialize_value(out, ctx, instr->as.add_ptr.ptr);
      ni.as.add_ptr.index =
          async_materialize_value(out, ctx, instr->as.add_ptr.index);
      ni.as.add_ptr.scale = instr->as.add_ptr.scale;
      ni.as.add_ptr.dst = clone_irval(dst);
      vec_insert(out, ni);
      async_store_dst_if_needed(out, ctx, instr->as.add_ptr.dst, dst, store);
      break;
    }
    case IRInstr_GETADDR: {
      bool store = false;
      struct IRValue *dst =
          async_prepare_dst(ctx, instr->as.getaddr.dst, &store);
      struct IRInstr ni = {0};
      ni.kind = IRInstr_GETADDR;
      ni.as.getaddr.src = clone_irval(instr->as.getaddr.src);
      ni.as.getaddr.dst = clone_irval(dst);
      vec_insert(out, ni);
      async_store_dst_if_needed(out, ctx, instr->as.getaddr.dst, dst, store);
      break;
    }
    case IRInstr_CPY_FROM_OFFSET:
    case IRInstr_CPY_TO_OFFSET:
    case IRInstr_SPAWN:
      /* These are not expected in the small first async-state-machine ABI. */
      assert(0 && "unsupported IR instruction in async state machine");
      break;
    case IRInstr_RET:
      assert(0 && "RET is handled by async_transform_body");
      break;
    default:
      assert(0);
  }
}

static VecIRInstr async_transform_body(VecIRInstr *orig,
                                       struct AsyncLowerCtx *ctx,
                                       VecCharPtr *resume_labels,
                                       VecCharPtr *preempt_labels)
{
  VecIRInstr out = {0};
  char *start_label;
  VecCharPtr pc_labels = async_concat_pc_labels(resume_labels, preempt_labels);
  int preempt_pc_base = resume_labels->len + 1;

  /* This emits the top-of-function state dispatch.
   * Conceptually it appends:
   *
   *   pc = __x_task_get_pc(task);
   *   if (pc == 0) goto AsyncStart;
   *   if (pc == 1) goto AsyncResume0;
   *   if (pc == 2) goto AsyncResume1;
   *   ...
   *   __x_async_bad_pc(task);
   *   return 1;
   *
   *   AsyncStart:
   *     ... */
  start_label = mklbl("AsyncStart", mktmp());

  async_emit_dispatch(&out, ctx, &pc_labels, start_label);

  free(start_label);

  int suspend_index = 0;
  int preempt_index = 0;
  for (int i = 0; i < orig->len; i++) {
    struct IRInstr *instr = &orig->data[i];

    /* Track which kind of suspension point we are handling.  */
    if (instr->kind == IRInstr_CALL &&
        ir_target_is(&instr->as.call.target, "__x_task_yield")) {
      /* If it is a yield...
       *
       * Earlier in IR lowering, a source-level:
       *
       *   yield;
       *
       * ...was represented as a fake call:
       *
       *   call __x_task_yield()
       *
       * Inside an async function, this call is not meant to survive.
       * It is just a marker saying that this is a suspension point.
       *
       * The transform replaces it with state-machine code.  */

      /* Get the next pc and resume label.  */
      int pc = suspend_index + 1;
      char *resume_label = resume_labels->data[suspend_index++];

      /* Emit `task.pc = pc`.  */
      async_emit_set_pc(&out, ctx, pc);

      /* Emit ret value.  0 for pending, 1 for completed.  */
      async_emit_ret_status(&out, 0);

      /* This emits:
       *
       * AsyncResume0:
       *
       * ...right after the `return 0`.
       *
       * That looks strange at first, because code after the return
       * is unreachable in a normal call. But, the next poll starts
       * at the top of the step function, reads `task.pc`, and jumps
       * directly to this label. */
      struct IRInstr resume = {0};
      resume.kind = IRInstr_LBL;
      resume.as.label.name = strdup(resume_label);

      vec_insert(&out, resume);

      continue;
    }

    if (instr->kind == IRInstr_CALL &&
        ir_target_is(&instr->as.call.target, "__x_task_await")) {
      /* If it's an await...
       *
       * Earlier, source:
       *
       *   let v = await t;
       *
       * ...became marker IR:
       *
       *   v = call __x_task_await(t)
       *
       * In a synchronous function, `__x_task_await` can be a blocking runtime
       * call. But inside an async step function, blocking would be wrong. The
       * task must suspend if the awaited task is not ready.
       *
       * So this transform replaces the blocking call with non-blocking poll
       * logic. */

      /* Get the next pc and resume label.  */
      int pc = suspend_index + 1;
      char *resume_label = resume_labels->data[suspend_index++];

      /* This emits something conceptually like:
       *
       *   AsyncResumeN:
       *     child = load awaited task;
       *
       *     if (__x_task_is_done(child)) {
       *         result = __x_task_take_result(child);
       *         store result into destination;
       *         goto AsyncAwaitEnd;
       *     }
       *
       *     __x_task_set_pc(current_task, pc);
       *     return 0;
       *
       *   AsyncAwaitEnd:
       *
       * The key difference from `yield` is that `yield` always
       * suspends immediately, and `await` only suspends if the
       * awaited task is not done.*/
      async_transform_await(&out, ctx, instr, pc, resume_label);

      continue;
    }

    if (async_instr_is_io_wait(instr)) {
      int pc = suspend_index + 1;
      char *resume_label = resume_labels->data[suspend_index++];
      async_transform_io_wait(&out, ctx, instr, pc, resume_label);
      continue;
    }

    if (instr->kind == IRInstr_RET) {
      /* If it is a ret...
       *
       * If the original ret has a value, this loads/prepares it.
       * For example, if the return value is a variable stored
       * in the async frame:
       *
       *  ret y
       *
       * ...then `async_materialize_value` may emit:
       *
       *  tmp_y = __x_task_frame_get(task, SLOT_y);
       *
       * ...and return `tmp_y`.
       *
       *  If there is no return value, it uses `0`. */
      struct IRValue *ret_val =
          instr->as.ret.val
              ? async_materialize_value(&out, ctx, instr->as.ret.val)
              : ir_const_u64(0);
      VecIRValuePtr args = {0};
      vec_insert(&args, async_task_value(ctx));
      vec_insert(&args, ret_val);
      ir_emit_call(&out, "__x_task_set_result", args, NULL);

      /* Task is completed.  */
      async_emit_ret_status(&out, 1);

      continue;
    }

    char *preempt_label = preempt_labels->data[i];
    int preempt_pc = preempt_pc_base + preempt_index++;
    assert(preempt_label);

    if (instr->kind == IRInstr_LBL) {
      async_transform_regular_instr(&out, ctx, instr);
      async_emit_preempt_point(&out, ctx, preempt_pc, preempt_label);
      continue;
    }

    async_emit_preempt_point(&out, ctx, preempt_pc, preempt_label);

    /* If the instruction is not `yield`, not `await`, and not `ret`,
     * it gets transformed normally. This means reads and writes are
     * rewritten through the task frame.
     *
     * For example, original IR:
     *
     *   tmp = x + 1
     *
     * ...becomes roughly:
     *
     *   tmp_x = __x_task_frame_get(task, SLOT_x)
     *   tmp_local = tmp_x + 1
     *   __x_task_frame_set(task, SLOT_tmp, tmp_local)
     *
     * Another example:
     *
     * Original IR:
     *
     *   call printf(fmt, x)
     *
     * ...becomes roughly:
     *
     *   tmp_x = __x_task_frame_get(task, SLOT_x)
     *   call printf(fmt, tmp_x)
     *
     * This is what makes local variables and temporaries
     * survive across suspension points like `yield` or `await`. */
    async_transform_regular_instr(&out, ctx, instr);
  }

  /* Falling off the end of an async function is completion with result 0. */
  VecIRValuePtr args = {0};
  vec_insert(&args, async_task_value(ctx));
  vec_insert(&args, ir_const_u64(0));
  ir_emit_call(&out, "__x_task_set_result", args, NULL);
  async_emit_ret_status(&out, 1);

  return out;
}

static struct IRFunction *irfy_async_wrapper(struct DeclFn *fn,
                                             struct AsyncLowerCtx *ctx,
                                             char *step_name)
{
  VecIRInstr body = {0};
  VecIRValuePtr spawn_args = {0};
  vec_insert(&spawn_args, ir_const_u64((unsigned long long) ctx->slots.len));

  struct IRValue *task = mkirvar();
  task->type = (Type){.kind = TASK_T};

  struct Expr target = {0};
  target.kind = EXPR_VARIABLE;
  target.as.var.name = strdup(step_name);
  target.as.var.type = (Type){.kind = UNKNOWN_T};

  struct IRInstr spawn = {0};
  spawn.kind = IRInstr_SPAWN;
  spawn.as.spawn.target = target;
  spawn.as.spawn.args = spawn_args;
  spawn.as.spawn.dst = clone_irval(task);
  vec_insert(&body, spawn);

  for (int i = 0; i < fn->params.len; i++) {
    struct Parameter *param = &fn->params.data[i];
    int slot = async_find_slot(ctx, param->name);
    assert(slot >= 0);

    VecIRValuePtr set_args = {0};
    vec_insert(&set_args, clone_irval(task));
    vec_insert(&set_args, ir_const_u64((unsigned long long) slot));
    vec_insert(&set_args, ir_named_var(param->name, param->type));
    ir_emit_call(&body, "__x_task_frame_set", set_args, NULL);
  }

  struct IRInstr ret = {0};
  ret.kind = IRInstr_RET;
  ret.as.ret.val = clone_irval(task);
  vec_insert(&body, ret);

  struct IRFunction f = {0};
  f.name = fn->name;
  f.params = fn->params;
  f.retval = (Type){.kind = TASK_T};
  f.body = body;
  return ALLOC(f);
}

static struct IRFunction *irfy_async_step(struct AsyncLowerCtx *ctx,
                                          VecIRInstr body, char *step_name)
{
  struct Parameter task_param = {0};
  task_param.name = ctx->task_name;
  task_param.type = (Type){.kind = TASK_T};
  task_param.is_mut = false;

  VecParam params = {0};
  vec_insert(&params, task_param);

  struct IRFunction f = {0};
  f.name = step_name;
  f.params = params;
  f.retval = (Type){.kind = I64_T};
  f.body = body;
  return ALLOC(f);
}

static void irfy_async_fn(struct DeclFn *fn, VecIRFunctionPtr *funcs)
{
  VecIRInstr orig = {0};
  for (int i = 0; i < fn->body.len; i++) {
    irfy_stmt(&orig, &fn->body.data[i]);
  }

  struct AsyncLowerCtx ctx = {0};
  ctx.task_name = mkstr("var.__async_task.%d", mktmp());

  /* Add fn params to the async frame-layout context.
   *
   * async fn count(name: str, start: i64) -> i64 {
   *   ...
   * }
   *
   * ...it creates entries roughly like:
   *
   *   slot 0 = name   str
   *   slot 1 = start  i64
   *
   * Those entries go into `ctx.slots`.  */
  for (int i = 0; i < fn->params.len; i++) {
    async_add_slot(&ctx, fn->params.data[i].name, fn->params.data[i].type);
  }

  /* Add locals and temporaries to frame slots.
   *
   * For example, a source like:
   *
   *   async fn f(x: i64) -> i64 {
   *     let y: i64 = x + 1;
   *     yield;
   *     ret y;
   *   }
   *
   * ...might first become normal-ish IR like:
   *
   *   tmp.1 = x + 1
   *   y = tmp.1
   *   call __x_task_yield()
   *   ret y
   *
   * Then this loop finds `x`, `tmp.1`, and `y`, and adds them to
   * `ctx.slots`, producing a frame layout like:
   *
   *   slot 0 = x
   *   slot 1 = tmp.1
   *   slot 2 = y
   *
   * Later, when transforming the body, reads/writes of those values
   * become frame operations:
   *
   *   tmp_x = __x_task_frame_get(task, slot_x)
   *   tmp_1 = tmp_x + 1;
   *   __x_task_frame_set(task, slot_tmp_1, tmp_1)
   *   __x_task_frame_set(task, slot_y, tmp_1)
   *
   *   ...
   *
   *   tmp_y = __x_task_frame_get(task, slot_y)
   *   __x_task_set_result(task, tmp_y)  */
  for (int i = 0; i < orig.len; i++) {
    async_collect_instr_vars(&ctx, &orig.data[i]);
  }

  /* This line creates the list of resume labels for the async state machine:
   * At this point, `orig` is the ordinary IR for the async function body.=
   * It still contains marker calls like:
   *
   * call __x_task_yield()
   * dst = call __x_task_await(task)
   *
   * It scans that IR and creates one label per suspension point (`yield` or
   * `await`, because both can pause the async function and return control to
   * the scheduler).
   *
   * So for a source like:
   *
   *   async fn f() -> i64 {
   *     print(1);
   *     yield;
   *     print(2);
   *     yield;
   *     print(3);
   *     ret 0
   *   }
   *
   * ...it will create something like:
   *
   *   resume_labels[0] = ".LAsyncResume.10"
   *   resume_labels[1] = ".LAsyncResume.11"  */
  VecCharPtr resume_labels = async_collect_resume_labels(&orig);
  VecCharPtr preempt_labels = async_collect_preempt_labels(&orig);

  /* The async lowering starts building the generated step function.
   *
   * For an async function like:
   *
   *   async fn count(name: str, start: i64) -> i64 {
   *     ...
   *   }
   *
   * ...the first line creates the generated step-function name:
   * `count__async_step`.
   *
   * After lowering, the compiler will emit two IR functions:
   *
   *   count                 // public wrapper/constructor
   *   count__async_step     // hidden state-machine body
   *
   * The wrapper `count(...)` allocates a task and stores parameters
   * into the task frame. The step function `count__async_step(task)`
   * is what the scheduler repeatedly polls.
   *
   * The original ordinary IR body is taken along with the async
   * lowering context and resume labels, and rewritten it into
   * the actual state-machine body.
   *
   * So if orig looked conceptually like:
   *
   *   tmp = start + 1
   *   call __x_task_yield()
   *   ret tmp
   *
   * ..the body will be transformed into:
   *
   *   pc = __x_task_get_pc(task)
   *   if pc == 0 goto AsyncStart
   *   if pc == 1 goto AsyncResume0
   *   call __x_async_bad_pc(task)
   *   ret 1
   *
   *   AsyncStart:
   *     start_local = __x_task_frame_get(task, SLOT_start)
   *     tmp_local = start_local + 1
   *     __x_task_frame_set(task, SLOT_tmp, tmp_local)
   *
   *     __x_task_set_pc(task, 1)
   *     ret 0
   *
   *   AsyncResume0:
   *     tmp_local = __x_task_frame_get(task, SLOT_tmp)
   *     __x_task_set_result(task, tmp_local)
   *     ret 1 */
  char *step_name = mkstr("%s__async_step", fn->name);
  VecIRInstr step_body =
      async_transform_body(&orig, &ctx, &resume_labels, &preempt_labels);

  /* This builds the public function with the original name:
   *
   *   count(name: str, start: i64) -> task
   *
   * Its job is not to run the async body, but to create
   * and initialize a task object. Conceptually it emits IR for:
   *
   *   task count(char *name, int64_t start) {
   *     task t = __x_task_new(count__async_step, frame_slot_count);
   *
   *     __x_task_frame_set(t, SLOT_name, name);
   *     __x_task_frame_set(t, SLOT_start, start);
   *
   *     return t;
   *   } */
  struct IRFunction *wrapper = irfy_async_wrapper(fn, &ctx, step_name);
  struct IRFunction *step = irfy_async_step(&ctx, step_body, step_name);
  vec_insert(funcs, wrapper);
  vec_insert(funcs, step);
}

static void irfy_fn(struct DeclFn *fn, VecIRFunctionPtr *funcs)
{
  if (fn->is_extern) {
    return;
  }

  struct IRFunction f = {0};
  VecIRInstr instrs = {0};

  for (int i = 0; i < fn->body.len; i++) {
    irfy_stmt(&instrs, &fn->body.data[i]);
  }

  f.body = instrs;
  f.name = fn->name;
  f.params = fn->params;
  f.retval = fn->retval;

  vec_insert(funcs, ALLOC(f));
}

struct IrfyResult irfy_ast(struct AST *ast)
{
  struct IRProgram prog;
  struct IrfyResult result;
  VecIRFunctionPtr funcs = {0};

  result.is_ok = true;
  result.msg = NULL;

  for (int i = 0; i < ast->decls.len; i++) {
    struct Decl *decl = &ast->decls.data[i];

    if (decl->kind == DECL_VARIABLE) {
      if (decl->as.variable.is_extern) {
        add_extern_variable(decl->as.variable.name);
      } else {
        char *directive = global_initializer_directive(&decl->as.variable);
        if (!directive) {
          result.is_ok = false;
          result.msg = "Global variable initializers must be constant literals";
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

    if (decl->as.fn.is_async) {
      irfy_async_fn(&decl->as.fn, &funcs);
    } else {
      irfy_fn(&decl->as.fn, &funcs);
    }
  }

  prog.funcs = funcs;

  result.prog = prog;

  return result;
}

#if defined(DEBUG_IR) || defined(DEBUG_IR_OPT)
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
    case IRInstr_SPAWN: {
      printf("IRInstr_SPAWN(\n");

      print_indent(spaces + 2);
      printf("target = ");
      print_expr(&instr->as.spawn.target, spaces + 2);
      printf(",\n");

      print_indent(spaces + 2);
      printf("args: [\n");
      for (int k = 0; k < instr->as.spawn.args.len; k++) {
        print_indent(spaces + 4);
        print_ir_val(instr->as.spawn.args.data[k], spaces + 4);
        printf(",\n");
      }
      print_indent(spaces + 2);
      printf("],\n");

      print_indent(spaces + 2);
      printf("dst: ");
      print_ir_val(instr->as.spawn.dst, spaces + 2);
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
