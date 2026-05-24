#include "fixup.h"

#include <stdlib.h>
#include <stdint.h>

static bool fits_i32(long long x)
{
  return x >= INT32_MIN && x <= INT32_MAX;
}

static bool is_shift_asm_binary(enum AsmInstrBinaryKind kind)
{
  return kind == AsmInstrBinary_SHL || kind == AsmInstrBinary_SHR ||
         kind == AsmInstrBinary_SAR;
}

struct FixupResult fixup(struct AsmProgram *prog)
{
  struct FixupResult r;

  r.is_ok = true;
  r.msg = NULL;

  for (int i = 0; i < prog->funcs.len; i++) {
    VecAsmInstr instrs = {0};
    for (int j = 0; j < prog->funcs.data[i].body.len; j++) {
      struct AsmInstr *asminstr = &prog->funcs.data[i].body.data[j];
      switch (asminstr->kind) {
        case AsmInstr_CMP: {
          struct AsmType cmp_type = asminstr->as.cmp.asm_type;

          bool is_float =
              cmp_type.kind == AsmType_FLOAT || cmp_type.kind == AsmType_DOUBLE;

          bool is_both_stack = asminstr->as.cmp.lhs.kind == AsmOperand_STACK &&
                               asminstr->as.cmp.rhs.kind == AsmOperand_STACK;

          bool is_dst_imm = asminstr->as.cmp.rhs.kind == AsmOperand_IMM;

          bool is_dst_float_stack =
              is_float && asminstr->as.cmp.rhs.kind == AsmOperand_STACK;

          bool is_large_imm64 = cmp_type.kind == AsmType_QUADWORD &&
                                asminstr->as.cmp.lhs.kind == AsmOperand_IMM &&
                                !fits_i32(asminstr->as.cmp.lhs.as.imm);

          if (is_both_stack || is_dst_imm || is_dst_float_stack ||
              is_large_imm64) {
            enum AsmRegister scratch_reg = is_float ? XMM8 : R10;

            struct AsmOperand scratch_op = {
                .kind = AsmOperand_REG,
                .as.reg = scratch_reg,
                .asm_type = cmp_type,
            };

            struct AsmInstr i1 = {0};
            i1.kind = AsmInstr_MOV;
            i1.asm_type = cmp_type;

            if (is_large_imm64) {
              i1.as.mov.src = asminstr->as.cmp.lhs;
              i1.as.mov.dst = scratch_op;

              struct AsmInstr i2 = {0};
              i2.kind = AsmInstr_CMP;
              i2.as.cmp.asm_type = cmp_type;
              i2.as.cmp.lhs = scratch_op;
              i2.as.cmp.rhs = asminstr->as.cmp.rhs;
              i2.asm_type = cmp_type;

              vec_insert(&instrs, i1);
              vec_insert(&instrs, i2);
              break;
            }

            i1.as.mov.src = asminstr->as.cmp.rhs;
            i1.as.mov.dst = scratch_op;

            struct AsmInstr i2 = {0};
            i2.kind = AsmInstr_CMP;
            i2.as.cmp.asm_type = cmp_type;
            i2.as.cmp.lhs = asminstr->as.cmp.lhs;
            i2.as.cmp.rhs = scratch_op;
            i2.asm_type = cmp_type;

            vec_insert(&instrs, i1);
            vec_insert(&instrs, i2);
          } else {
            vec_insert(&instrs, *asminstr);
          }

          break;
        }
        case AsmInstr_CVT: {
          bool src_is_imm = asminstr->as.cvt.src.kind == AsmOperand_IMM;

          if (src_is_imm && (asminstr->as.cvt.kind == AsmCast_ZeroExtend ||
                             asminstr->as.cvt.kind == AsmCast_SignExtend)) {
            struct AsmInstr mov = {0};
            mov.kind = AsmInstr_MOV;
            mov.asm_type = asminstr->as.cvt.dst.asm_type;
            mov.as.mov.src = asminstr->as.cvt.src;
            mov.as.mov.dst = asminstr->as.cvt.dst;

            mov.as.mov.src.asm_type = asminstr->as.cvt.dst.asm_type;

            vec_insert(&instrs, mov);
            break;
          }

          if (src_is_imm) {
            struct AsmOperand scratch = {
                .kind = AsmOperand_REG,
                .as.reg = R10,
                .asm_type = asminstr->as.cvt.src.asm_type,
            };

            struct AsmInstr mov = {0};
            mov.kind = AsmInstr_MOV;
            mov.asm_type = scratch.asm_type;
            mov.as.mov.src = asminstr->as.cvt.src;
            mov.as.mov.dst = scratch;

            struct AsmInstr cvt = *asminstr;
            cvt.as.cvt.src = scratch;

            vec_insert(&instrs, mov);
            vec_insert(&instrs, cvt);
            break;
          }

          vec_insert(&instrs, *asminstr);
          break;
        }
        case AsmInstr_LEA: {
          if (asminstr->as.lea.dst.kind == AsmOperand_STACK) {
            struct AsmOperand scratch_op = {
                .kind = AsmOperand_REG,
                .as.reg = R10,
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};
            struct AsmInstr i1 = {0}, i2 = {0};

            i1.kind = AsmInstr_LEA;
            i1.as.lea.src = asminstr->as.lea.src;
            i1.as.lea.dst = scratch_op;
            i1.asm_type = (struct AsmType){.kind = AsmType_QUADWORD};

            i2.kind = AsmInstr_MOV;
            i2.as.mov.src = scratch_op;
            i2.as.mov.dst = asminstr->as.lea.dst;
            i2.asm_type = (struct AsmType){.kind = AsmType_QUADWORD};

            vec_insert(&instrs, i1);
            vec_insert(&instrs, i2);
          } else {
            vec_insert(&instrs, *asminstr);
          }
          break;
        }
        case AsmInstr_MOV: {
          if ((asminstr->as.mov.src.kind == AsmOperand_STACK &&
               asminstr->as.mov.dst.kind == AsmOperand_STACK) ||
              ((asminstr->asm_type.kind == AsmType_FLOAT ||
                asminstr->asm_type.kind == AsmType_DOUBLE) &&
               asminstr->as.mov.dst.kind == AsmOperand_STACK) ||
              (asminstr->as.mov.src.kind == AsmOperand_MEMORY &&
               asminstr->as.mov.dst.kind == AsmOperand_STACK)) {
            enum AsmRegister scratch_reg;
            struct AsmOperand scratch_op;
            struct AsmInstrMov mov1, mov2;
            struct AsmInstr i1 = {0}, i2 = {0};

            scratch_reg = (asminstr->asm_type.kind == AsmType_FLOAT ||
                           asminstr->asm_type.kind == AsmType_DOUBLE)
                              ? XMM8
                              : R10;
            scratch_op.kind = AsmOperand_REG;
            scratch_op.as.reg = scratch_reg;
            scratch_op.asm_type = asminstr->asm_type;

            i1.kind = AsmInstr_MOV;
            mov1.src = asminstr->as.mov.src;
            mov1.dst = scratch_op;
            i1.as.mov = mov1;
            i1.asm_type = asminstr->asm_type;

            i2.kind = AsmInstr_MOV;
            mov2.src = scratch_op;
            mov2.dst = asminstr->as.mov.dst;
            i2.as.mov = mov2;
            i2.asm_type = asminstr->asm_type;

            vec_insert(&instrs, i1);
            vec_insert(&instrs, i2);
          } else {
            vec_insert(&instrs, *asminstr);
          }

          break;
        }
        case AsmInstr_BIN: {
          bool is_float = (asminstr->asm_type.kind == AsmType_FLOAT ||
                           asminstr->asm_type.kind == AsmType_DOUBLE);

          if (is_shift_asm_binary(asminstr->as.binary.kind) &&
              asminstr->as.binary.lhs.kind != AsmOperand_IMM) {
            struct AsmOperand count_src = asminstr->as.binary.lhs;
            count_src.asm_type = (struct AsmType){.kind = AsmType_BYTE};

            struct AsmOperand cl = {
                .kind = AsmOperand_REG,
                .as.reg = CX,
                .asm_type = (struct AsmType){.kind = AsmType_BYTE}};

            struct AsmInstr i1 = {0}, i2 = {0};
            i1.kind = AsmInstr_MOV;
            i1.as.mov.src = count_src;
            i1.as.mov.dst = cl;
            i1.asm_type = (struct AsmType){.kind = AsmType_BYTE};

            i2.kind = AsmInstr_BIN;
            i2.as.binary.kind = asminstr->as.binary.kind;
            i2.as.binary.lhs = cl;
            i2.as.binary.rhs = asminstr->as.binary.rhs;
            i2.asm_type = asminstr->asm_type;

            vec_insert(&instrs, i1);
            vec_insert(&instrs, i2);
            break;
          }

          /* imul and float ops cannot use mem as dst */
          if ((asminstr->as.binary.kind == AsmInstrBinary_MUL || is_float) &&
              asminstr->as.binary.rhs.kind == AsmOperand_STACK) {
            enum AsmRegister scratch_reg = is_float ? XMM8 : R10;
            struct AsmOperand scratch_op = {.kind = AsmOperand_REG,
                                            .as.reg = scratch_reg,
                                            .asm_type = asminstr->asm_type};
            struct AsmInstrMov mov1, mov2;
            struct AsmInstrBinary bin;
            struct AsmInstr i1 = {0}, i2 = {0}, i3 = {0};

            i1.kind = AsmInstr_MOV;
            mov1.src = asminstr->as.binary.rhs;
            mov1.dst = scratch_op;
            i1.as.mov = mov1;
            i1.asm_type = asminstr->asm_type;

            i2.kind = AsmInstr_BIN;
            bin.kind = asminstr->as.binary.kind;
            bin.lhs = asminstr->as.binary.lhs;
            bin.rhs = scratch_op;
            i2.as.binary = bin;
            i2.asm_type = asminstr->asm_type;

            i3.kind = AsmInstr_MOV;
            mov2.src = scratch_op;
            mov2.dst = asminstr->as.binary.rhs;
            i3.as.mov = mov2;
            i3.asm_type = asminstr->asm_type;

            vec_insert(&instrs, i1);
            vec_insert(&instrs, i2);
            vec_insert(&instrs, i3);

            break;
          }

          /* integer binary ops cannot use mem as both operands */
          if (asminstr->as.binary.lhs.kind == AsmOperand_STACK &&
              asminstr->as.binary.rhs.kind == AsmOperand_STACK) {
            enum AsmRegister scratch_reg = R10;
            struct AsmOperand scratch_op;
            struct AsmInstrMov mov;
            struct AsmInstrBinary bin;
            struct AsmInstr i1 = {0}, i2 = {0};

            scratch_op.kind = AsmOperand_REG;
            scratch_op.as.reg = scratch_reg;
            scratch_op.asm_type = asminstr->asm_type;

            i1.kind = AsmInstr_MOV;
            mov.src = asminstr->as.binary.lhs;
            mov.dst = scratch_op;
            i1.as.mov = mov;
            i1.asm_type = asminstr->asm_type;

            i2.kind = AsmInstr_BIN;
            bin.kind = asminstr->as.binary.kind;
            bin.lhs = scratch_op;
            bin.rhs = asminstr->as.binary.rhs;
            i2.as.binary = bin;
            i2.asm_type = asminstr->asm_type;

            vec_insert(&instrs, i1);
            vec_insert(&instrs, i2);

            break;
          } else {
            vec_insert(&instrs, *asminstr);
            break;
          }
          break;
        }
        default:
          vec_insert(&instrs, *asminstr);
          break;
      }
    }

    vec_free(&prog->funcs.data[i].body);
    prog->funcs.data[i].body = instrs;
  }

  r.prog = prog;

  return r;
}
