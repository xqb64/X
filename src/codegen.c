#include "codegen.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ir.h"
#include "typechecker.h"
#include "util.h"

void print_asm_type(struct AsmType type)
{
  switch (type.kind) {
    case AsmType_BYTE:
      printf("AsmType_BYTE");
      break;
    case AsmType_WORD:
      printf("AsmType_WORD");
      break;
    case AsmType_LONGWORD:
      printf("AsmType_LONGWORD");
      break;
    case AsmType_QUADWORD:
      printf("AsmType_QUADWORD");
      break;
    default:
      assert(0);
  }
}

struct AsmType type_to_asm_type(Type type)
{
  switch (type.kind) {
    case U8_T:
    case I8_T:
    case BOOL_T:
      return (struct AsmType){.kind = AsmType_BYTE};
    case U16_T:
    case I16_T:
      return (struct AsmType){.kind = AsmType_WORD};
    case U32_T:
    case I32_T:
      return (struct AsmType){.kind = AsmType_LONGWORD};
    case U64_T:
    case I64_T:
    case PTR_T:
      return (struct AsmType){.kind = AsmType_QUADWORD};
    case F32_T:
      return (struct AsmType){.kind = AsmType_FLOAT};
    case F64_T:
      return (struct AsmType){.kind = AsmType_DOUBLE};
    case STRUCT_T: {
      int size, alignment;
      get_type_size_and_align(&type, &size, &alignment);
      return (struct AsmType){
          .kind = AsmType_BYTE_ARRAY,
          .as.bytearray = {.size = size, .alignment = alignment}};
    }
    default:
      return (struct AsmType){.kind = AsmType_QUADWORD};
  }
}

int asm_type_stack_size(struct AsmType t)
{
  switch (t.kind) {
    case AsmType_BYTE:
      return 1;
    case AsmType_WORD:
      return 2;
    case AsmType_LONGWORD:
    case AsmType_FLOAT:
      return 4;
    case AsmType_QUADWORD:
    case AsmType_DOUBLE:
      return 8;
    case AsmType_BYTE_ARRAY:
      return ((t.as.bytearray.size + 7) / 8) * 8;
    default:
      assert(0);
  }

  assert(0);
}

int asm_type_stack_align(struct AsmType t)
{
  switch (t.kind) {
    case AsmType_BYTE:
      return 1;
    case AsmType_WORD:
      return 2;
    case AsmType_LONGWORD:
    case AsmType_FLOAT:
      return 4;
    case AsmType_QUADWORD:
    case AsmType_DOUBLE:
      return 8;
    case AsmType_BYTE_ARRAY:
      return t.as.bytearray.alignment;
    default:
      assert(0);
  }

  assert(0);
}

int align_up_int(int n, int align)
{
  assert(align > 0);
  return ((n + align - 1) / align) * align;
}

const char *reg_to_str_64(enum AsmRegister reg)
{
  switch (reg) {
    case AX:
      return "%rax";
    case DI:
      return "%rdi";
    case SI:
      return "%rsi";
    case DX:
      return "%rdx";
    case CX:
      return "%rcx";
    case R8:
      return "%r8";
    case R9:
      return "%r9";
    case R10:
      return "%r10";
    case BP:
      return "%rbp";
    case SP:
      return "%rsp";
    default:
      return "";
  }
}

void print_asm_operand(struct AsmOperand *op)
{
  switch (op->kind) {
    case AsmOperand_INDEXED: {
      printf("AsmOperand_INDEXED((%s, %s, %d)",
             reg_to_str_64(op->as.indexed.base),
             reg_to_str_64(op->as.indexed.index), op->as.indexed.scale);
      break;
    }
    case AsmOperand_MEMORY: {
      printf("AsmOperand_MEMORY(offset = %d, base = ", op->as.mem.offset);
      switch (op->as.mem.base) {
        case AX:
          printf("%%rax");
          break;
        case BX:
          printf("%%rbx");
          break;
        case DI:
          printf("%%rdi");
          break;
        case SI:
          printf("%%rsi");
          break;
        case DX:
          printf("%%rdx");
          break;
        case CX:
          printf("%%rcx");
          break;
        case R8:
          printf("%%r8");
          break;
        case R9:
          printf("%%r9");
          break;
        case R10:
          printf("%%r10");
          break;
        case R11:
          printf("%%r11");
          break;
        case R12:
          printf("%%r12");
          break;
        case R13:
          printf("%%r13");
          break;
        case R14:
          printf("%%r14");
          break;
        case R15:
          printf("%%r15");
          break;
        case BP:
          printf("%%rbp");
          break;
        case SP:
          printf("%%rsp");
          break;
        default:
          assert(0);
      }
      printf(")");
      break;
    }
    case AsmOperand_IMM: {
      printf("AsmOperand_IMM(%lld)", op->as.imm);
      break;
    }
    case AsmOperand_PSEUDO: {
      printf("AsmOperand_PSEUDO(%s)", op->as.pseudo);
      break;
    }
    case AsmOperand_REG: {
      printf("AsmOperand_REG(");

      switch (op->as.reg) {
        case XMM0: {
          printf("%%xmm0");
          break;
        }
        case XMM1: {
          printf("%%xmm1");
          break;
        }
        case XMM2: {
          printf("%%xmm2");
          break;
        }
        case XMM3: {
          printf("%%xmm3");
          break;
        }
        case XMM4: {
          printf("%%xmm4");
          break;
        }
        case XMM5: {
          printf("%%xmm5");
          break;
        }
        case XMM6: {
          printf("%%xmm6");
          break;
        }
        case XMM7: {
          printf("%%xmm7");
          break;
        }
        case XMM8: {
          printf("%%xmm8");
          break;
        }
        case XMM9: {
          printf("%%xmm9");
          break;
        }
        case XMM10: {
          printf("%%xmm10");
          break;
        }
        case XMM11: {
          printf("%%xmm11");
          break;
        }
        case XMM12: {
          printf("%%xmm12");
          break;
        }
        case XMM13: {
          printf("%%xmm13");
          break;
        }
        case XMM14: {
          printf("%%xmm14");
          break;
        }
        case XMM15: {
          printf("%%xmm15");
          break;
        }
        case AX: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%al");
              break;
            }
            case AsmType_WORD: {
              printf("%%ax");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%eax");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%rax");
              break;
            }
            default:
              assert(0);
          }

          break;
        }
        case BX: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%bl");
              break;
            }
            case AsmType_WORD: {
              printf("%%bx");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%ebx");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%rbx");
              break;
            }
            default:
              assert(0);
          }

          break;
        }
        case DI: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%dil");
              break;
            }
            case AsmType_WORD: {
              printf("%%di");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%edi");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%rdi");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case SI: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%sil");
              break;
            }
            case AsmType_WORD: {
              printf("%%si");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%esi");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%rsi");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case DX: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%dl");
              break;
            }
            case AsmType_WORD: {
              printf("%%dx");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%edx");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%rdx");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case CX: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%cl");
              break;
            }
            case AsmType_WORD: {
              printf("%%cx");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%ecx");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%rcx");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case R8: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%r8b");
              break;
            }
            case AsmType_WORD: {
              printf("%%r8w");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%r8d");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%r8");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case R9: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%r9b");
              break;
            }
            case AsmType_WORD: {
              printf("%%r9w");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%r9d");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%r9");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case R10: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%r10b");
              break;
            }
            case AsmType_WORD: {
              printf("%%r10w");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%r10d");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%r10");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case R11: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%r11b");
              break;
            }
            case AsmType_WORD: {
              printf("%%r11w");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%r11d");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%r11");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case R12: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%r12b");
              break;
            }
            case AsmType_WORD: {
              printf("%%r12w");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%r12d");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%r12");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case R13: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%r13b");
              break;
            }
            case AsmType_WORD: {
              printf("%%r13w");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%r13d");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%r13");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case R14: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%r14b");
              break;
            }
            case AsmType_WORD: {
              printf("%%r14w");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%r14d");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%r14");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case R15: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%r15b");
              break;
            }
            case AsmType_WORD: {
              printf("%%r15w");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%r15d");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%r15");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case BP: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%bpl");
              break;
            }
            case AsmType_WORD: {
              printf("%%bp");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%ebp");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%rbp");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        case SP: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              printf("%%spl");
              break;
            }
            case AsmType_WORD: {
              printf("%%sp");
              break;
            }
            case AsmType_LONGWORD: {
              printf("%%esp");
              break;
            }
            case AsmType_QUADWORD: {
              printf("%%rsp");
              break;
            }
            default:
              assert(0);
          }
          break;
        }
        default:
          assert(0);
      }

      printf(")");

      break;
    }
    case AsmOperand_STACK: {
      printf("AsmOperand_STACK(offset = %d)", op->as.stack_offset);
      break;
    }
    case AsmOperand_DATA: {
      printf("AsmOperand_DATA(name = %s)", op->as.data);
      break;
    }
    default:
      assert(0);
  }
}

void print_condition_code(enum ConditionCode cc)
{
  switch (cc) {
    case CC_E:
      printf("E");
      break;
    case CC_NE:
      printf("NE");
      break;
    case CC_L:
      printf("L");
      break;
    case CC_LE:
      printf("LE");
      break;
    case CC_G:
      printf("G");
      break;
    case CC_GE:
      printf("GE");
      break;
    case CC_A:
      printf("A");
      break;
    case CC_AE:
      printf("AE");
      break;
    case CC_B:
      printf("B");
      break;
    case CC_BE:
      printf("BE");
      break;
    default:
      assert(0);
  }
}

void print_asm_binary_op(enum AsmInstrBinaryKind kind)
{
  switch (kind) {
    case AsmInstrBinary_ADD:
      printf("ADD");
      break;
    case AsmInstrBinary_SUB:
      printf("SUB");
      break;
    case AsmInstrBinary_MUL:
      printf("MUL");
      break;
    case AsmInstrBinary_DIV:
      printf("DIV");
      break;
    case AsmInstrBinary_LESS:
      printf("LESS");
      break;
    case AsmInstrBinary_LESS_EQUAL:
      printf("LESS EQUAL");
      break;
    case AsmInstrBinary_GREATER:
      printf("GREATER");
      break;
    case AsmInstrBinary_GREATER_EQUAL:
      printf("GREATER EQUAL");
      break;
    case AsmInstrBinary_EQUAL_EQUAL:
      printf("EQUAL EQUAL");
      break;
    case AsmInstrBinary_BANG_EQUAL:
      printf("BANG EQUAL");
      break;
    case AsmInstrBinary_BIT_AND:
      printf("BITWISE AND");
      break;
    case AsmInstrBinary_BIT_XOR:
      printf("BITWISE XOR");
      break;
    case AsmInstrBinary_BIT_OR:
      printf("BITWISE OR");
      break;
    case AsmInstrBinary_SHL:
      printf("SHIFT LEFT");
      break;
    case AsmInstrBinary_SHR:
      printf("SHIFT RIGHT LOGICAL");
      break;
    case AsmInstrBinary_SAR:
      printf("SHIFT RIGHT ARITHMETIC");
      break;
    default:
      assert(0 && "Unhandled AsmInstrBinaryKind");
  }
}

void print_asm_instr(struct AsmInstr *instr, int spaces)
{
  print_indent(spaces);

  switch (instr->kind) {
    case AsmInstr_PUSH: {
      printf("AsmInstr_PUSH(\n");
      print_indent(spaces + 2);
      printf("op = ");
      print_asm_operand(&instr->as.push.op);
      printf(",\n");
      print_indent(spaces);
      printf("),\n");
      break;
    }
    case AsmInstr_POP: {
      printf("AsmInstr_POP(\n");
      print_indent(spaces + 2);
      printf("op = ");
      print_asm_operand(&instr->as.pop.op);
      printf(",\n");
      print_indent(spaces);
      printf("),\n");
      break;
    }
    case AsmInstr_MOV: {
      printf("AsmInstr_MOV(\n");
      print_indent(spaces + 2);
      printf("src = ");
      print_asm_operand(&instr->as.mov.src);
      printf(",\n");
      print_indent(spaces + 2);
      printf("dst = ");
      print_asm_operand(&instr->as.mov.dst);
      printf(",\n");
      print_indent(spaces);
      printf("),\n");
      break;
    }
    case AsmInstr_BIN: {
      printf("AsmInstr_BIN(\n");
      print_indent(spaces + 2);
      printf("kind = ");
      print_asm_binary_op(instr->as.binary.kind);
      printf(",\n");
      print_indent(spaces + 2);
      printf("lhs = ");
      print_asm_operand(&instr->as.binary.lhs);
      printf(",\n");
      print_indent(spaces + 2);
      printf("rhs = ");
      print_asm_operand(&instr->as.binary.rhs);
      printf(",\n");
      print_indent(spaces);
      printf("),\n");
      break;
    }
    case AsmInstr_RET: {
      printf("AsmInstr_RET,\n");
      break;
    }
    case AsmInstr_CALL: {
      printf("AsmInstr_CALL(target = %s),\n", instr->as.call.target);
      break;
    }
    case AsmInstr_JMP: {
      printf("AsmInstr_JMP(target = %s),\n", instr->as.jmp.target);
      break;
    }
    case AsmInstr_LBL: {
      printf("AsmInstr_LBL(name = %s),\n", instr->as.lbl.name);
      break;
    }
    case AsmInstr_CMP: {
      printf("AsmInstr_CMP(\n");
      print_indent(spaces + 2);
      printf("asm_type = ");
      print_asm_type(instr->as.cmp.asm_type);
      printf(",\n");
      print_indent(spaces + 2);
      printf("lhs = ");
      print_asm_operand(&instr->as.cmp.lhs);
      printf(",\n");
      print_indent(spaces + 2);
      printf("rhs = ");
      print_asm_operand(&instr->as.cmp.rhs);
      printf(",\n");
      print_indent(spaces);
      printf(",)\n");
      break;
    }
    case AsmInstr_JmpCC: {
      printf("AsmInstr_JmpCC(\n");
      print_indent(spaces + 2);
      printf("cc = ");
      print_condition_code(instr->as.jmpcc.cc);
      printf(",\n");
      print_indent(spaces + 2);
      printf("target = %s,\n", instr->as.jmpcc.target);
      print_indent(spaces);
      printf("),\n");
      break;
    }
    case AsmInstr_SetCC: {
      printf("AsmInstr_SetCC(\n");
      print_indent(spaces + 2);
      printf("cc = ");
      print_condition_code(instr->as.setcc.cc);
      printf(",\n");
      print_indent(spaces + 2);
      printf("op = ");
      print_asm_operand(&instr->as.setcc.op);
      printf(",\n");
      print_indent(spaces);
      printf("),\n");
      break;
    }

    case AsmInstr_LEA: {
      printf("AsmInstr_LEA(\n");
      print_indent(spaces + 2);
      printf("src = ");
      print_asm_operand(&instr->as.lea.src);
      printf(",\n");
      print_indent(spaces + 2);
      printf("dst = ");
      print_asm_operand(&instr->as.lea.dst);
      printf(",\n");
      print_indent(spaces);
      printf("),\n");
      break;
    }
    case AsmInstr_UNARY: {
      printf("AsmInstr_UNARY(\n");
      print_indent(spaces + 2);
      printf("op = ");
      print_asm_operand(&instr->as.unary.op);
      printf(",\n");
      print_indent(spaces);
      printf("),\n");
      break;
    }
    case AsmInstr_CVT: {
      printf("AsmInstr_CVT(\n");
      print_indent(spaces + 2);
      printf("src = ");
      print_asm_operand(&instr->as.cvt.src);
      printf(",\n");
      print_indent(spaces + 2);
      printf("dst = ");
      print_asm_operand(&instr->as.cvt.dst);
      printf(",\n");
      print_indent(spaces);
      printf("),\n");
      break;
    }
    case AsmInstr_REP_MOVSB: {
      printf("AsmInstr_REP_MOVSB,\n");
      break;
    }
    default:
      assert(0);
  }
}

void free_asm_instr(struct AsmInstr *instr)
{
  if (instr->kind == AsmInstr_LEA) {
    if (instr->as.lea.src.kind == AsmOperand_DATA) {
      free(instr->as.lea.src.as.data);
    }
    if (instr->as.lea.dst.kind == AsmOperand_DATA) {
      free(instr->as.lea.dst.as.data);
    }
  }
}

void print_asm_fn(struct AsmFunction *fn)
{
  printf("AsmFunction(\n");
  print_indent(2);
  printf("name = %s,\n", fn->name);
  print_indent(2);
  printf("body: [\n");

  for (int i = 0; i < fn->body.len; i++) {
    print_asm_instr(&fn->body.data[i], 4);
  }

  print_indent(2);
  printf("]\n)\n");
}

void free_asm_fn(struct AsmFunction *fn)
{
  for (int i = 0; i < fn->body.len; i++) {
    free_asm_instr(&fn->body.data[i]);
  }
  vec_free(&fn->body);
}

void print_asm(struct AsmProgram *prog)
{
  for (int i = 0; i < prog->funcs.len; i++) {
    print_asm_fn(&prog->funcs.data[i]);
  }
}

void free_asm(struct AsmProgram *prog)
{
  for (int i = 0; i < prog->funcs.len; i++) {
    free_asm_fn(&prog->funcs.data[i]);
  }
  vec_free(&prog->funcs);
}

struct AsmOperand codegen_irvalue(struct IRValue *val)
{
  switch (val->kind) {
    case IRValue_CONST: {
      struct AsmOperand operand;
      operand.kind = AsmOperand_IMM;
      switch (val->type.kind) {
        case I8_T:
          operand.as.imm = val->as.konst.as.i8;
          break;
        case U8_T:
          operand.as.imm = val->as.konst.as.u8;
          break;
        case I16_T:
          operand.as.imm = val->as.konst.as.i16;
          break;
        case U16_T:
          operand.as.imm = val->as.konst.as.u16;
          break;
        case I32_T:
          operand.as.imm = val->as.konst.as.i32;
          break;
        case U32_T:
          operand.as.imm = val->as.konst.as.u32;
          break;
        case I64_T:
          operand.as.imm = val->as.konst.as.i64;
          break;
        case U64_T:
          operand.as.imm = val->as.konst.as.u64;
          break;
        case F32_T:
        case F64_T: {
          char *lbl = mklbl("LC", mktmp());
          struct StaticConstant sc;
          sc.name = strdup(lbl);

          char buf[64];
          if (val->type.kind == F32_T) {
            unsigned int bits;
            memcpy(&bits, &val->as.konst.as.f32, sizeof(float));
            snprintf(buf, sizeof(buf), ".long %u", bits);
          } else {
            unsigned long long bits;
            memcpy(&bits, &val->as.konst.as.f64, sizeof(double));
            snprintf(buf, sizeof(buf), ".quad %llu", bits);
          }

          sc.value = strdup(buf);
          vec_insert(&global_constants, sc);

          operand.kind = AsmOperand_DATA;
          operand.as.data = strdup(lbl);
          break;
        }
        case BOOL_T:
          operand.as.imm = val->as.konst.as.boolean ? 1 : 0;
          break;
        default:
          assert(0);
      }
      operand.asm_type = type_to_asm_type(val->type);
      return operand;
    }
    case IRValue_VAR: {
      struct AsmOperand operand;
      operand.kind = AsmOperand_PSEUDO;
      operand.as.pseudo = val->as.var;
      operand.asm_type = type_to_asm_type(val->type);
      return operand;
    }
    default:
      assert(0);
  }
}

bool is_comparison(enum IRInstrBinaryKind kind)
{
  return kind == IRInstrBinary_E || kind == IRInstrBinary_NE ||
         kind == IRInstrBinary_L || kind == IRInstrBinary_LE ||
         kind == IRInstrBinary_G || kind == IRInstrBinary_GE;
}

struct ABIClassification classify_type(Type *type)
{
  struct ABIClassification result = {
      .is_memory = false, .eightbytes = {ABI_NO_CLASS, ABI_NO_CLASS}};

  int size, align;
  get_type_size_and_align(type, &size, &align);

  /* System V ABI: If size is > 16 (2 eightbytes), it goes to memory. */
  if (size > 16) {
    result.is_memory = true;
    return result;
  }

  /* Base case: primitives and pointers */
  if (type->kind != STRUCT_T) {
    if (type->kind == F32_T || type->kind == F64_T) {
      result.eightbytes[0] = ABI_SSE;
    } else {
      result.eightbytes[0] = ABI_INTEGER;
    }
    return result;
  }

  /* Recursive case: Structs */
  struct StructDef *def = struct_get(struct_table, type->as.struct_name);
  assert(def && "Tried to classify unknown struct");

  for (int i = 0; i < def->fields.len; i++) {
    struct StructField *field = &def->fields.data[i];
    int offset = field->offset;

    struct ABIClassification field_class = classify_type(&field->type);
    if (field_class.is_memory) {
      result.is_memory = true;
      return result;
    }

    int field_size, field_align;
    get_type_size_and_align(&field->type, &field_size, &field_align);

    int start_eightbyte = offset / 8;
    int end_eightbyte = (offset + field_size - 1) / 8;

    for (int eb = start_eightbyte; eb <= end_eightbyte; eb++) {
      int field_eb = eb - start_eightbyte;
      enum ABIClass c = field_class.eightbytes[field_eb];

      if (c == ABI_INTEGER) {
        result.eightbytes[eb] = ABI_INTEGER;
      } else if (c == ABI_SSE && result.eightbytes[eb] == ABI_NO_CLASS) {
        result.eightbytes[eb] = ABI_SSE;
      }
    }
  }

  return result;
}

bool is_sret(Type *retval)
{
  if (retval->kind != STRUCT_T) {
    return false;
  }

  int size, align;
  get_type_size_and_align(retval, &size, &align);
  return size > 16;
}

bool is_shift_asm_binary(enum AsmInstrBinaryKind kind)
{
  return kind == AsmInstrBinary_SHL || kind == AsmInstrBinary_SHR ||
         kind == AsmInstrBinary_SAR;
}

void codegen_instr(struct IRInstr *ir_instr, VecAsmInstr *instrs,
                   Type *fn_retval)
{
  switch (ir_instr->kind) {
    case IRInstr_BIN: {
      if (is_comparison(ir_instr->as.binary.kind)) {
        Type common = ir_instr->as.binary.dst->type;
        bool is_signed = !is_unsigned(common.kind);

        enum ConditionCode cc;

        if (is_signed) {
          switch (ir_instr->as.binary.kind) {
            case IRInstrBinary_E:
              cc = CC_E;
              break;
            case IRInstrBinary_NE:
              cc = CC_NE;
              break;
            case IRInstrBinary_L:
              cc = CC_L;
              break;
            case IRInstrBinary_G:
              cc = CC_G;
              break;
            case IRInstrBinary_LE:
              cc = CC_LE;
              break;
            case IRInstrBinary_GE:
              cc = CC_GE;
              break;
            default:
              assert(0 && "Unreachable or unhandled signed condition");
          }
        } else {
          switch (ir_instr->as.binary.kind) {
            case IRInstrBinary_E:
              cc = CC_E;
              break;
            case IRInstrBinary_NE:
              cc = CC_NE;
              break;
            case IRInstrBinary_L:
              cc = CC_B;
              break;
            case IRInstrBinary_G:
              cc = CC_A;
              break;
            case IRInstrBinary_LE:
              cc = CC_BE;
              break;
            case IRInstrBinary_GE:
              cc = CC_AE;
              break;
            default:
              assert(0 && "Unreachable or unhandled unsigned condition");
          }
        }

        struct AsmInstr cmp_instr = {0};
        cmp_instr.kind = AsmInstr_CMP;

        struct AsmOperand lhs;
        lhs = codegen_irvalue(ir_instr->as.binary.lhs);

        cmp_instr.as.cmp.asm_type = lhs.asm_type;
        cmp_instr.as.cmp.lhs = codegen_irvalue(ir_instr->as.binary.rhs);
        cmp_instr.as.cmp.rhs = lhs;

        struct AsmInstr mov_instr = {0};
        mov_instr.kind = AsmInstr_MOV;
        mov_instr.asm_type = type_to_asm_type(ir_instr->as.binary.dst->type);
        mov_instr.as.mov.src =
            (struct AsmOperand){.kind = AsmOperand_IMM, .as.imm = 0};
        mov_instr.as.mov.dst = codegen_irvalue(ir_instr->as.binary.dst);

        struct AsmInstr setcc = {0};
        setcc.kind = AsmInstr_SetCC;
        setcc.as.setcc.cc = cc;
        setcc.as.setcc.op = codegen_irvalue(ir_instr->as.binary.dst);

        vec_insert(instrs, cmp_instr);
        vec_insert(instrs, mov_instr);
        vec_insert(instrs, setcc);

        break;
      }

      enum AsmInstrBinaryKind kind;
      struct AsmOperand lhs, rhs, dst;

      switch (ir_instr->as.binary.kind) {
        case IRInstrBinary_ADD:
          kind = AsmInstrBinary_ADD;
          break;
        case IRInstrBinary_SUB:
          kind = AsmInstrBinary_SUB;
          break;
        case IRInstrBinary_MUL:
          kind = AsmInstrBinary_MUL;
          break;
        case IRInstrBinary_DIV:
          kind = AsmInstrBinary_DIV;
          break;
        case IRInstrBinary_BIT_AND:
          kind = AsmInstrBinary_BIT_AND;
          break;
        case IRInstrBinary_BIT_XOR:
          kind = AsmInstrBinary_BIT_XOR;
          break;
        case IRInstrBinary_BIT_OR:
          kind = AsmInstrBinary_BIT_OR;
          break;
        case IRInstrBinary_SHL:
          kind = AsmInstrBinary_SHL;
          break;
        case IRInstrBinary_SHR:
          kind = AsmInstrBinary_SHR;
          break;
        case IRInstrBinary_SAR:
          kind = AsmInstrBinary_SAR;
          break;
        default:
          assert(0);
      }

      lhs = codegen_irvalue(ir_instr->as.binary.lhs);
      rhs = codegen_irvalue(ir_instr->as.binary.rhs);
      dst = codegen_irvalue(ir_instr->as.binary.dst);

      struct AsmInstr i1 = {0}, i2 = {0};

      i1.kind = AsmInstr_MOV;
      i1.as.mov.src = lhs;
      i1.as.mov.dst = dst;
      i1.asm_type = dst.asm_type;

      i2.kind = AsmInstr_BIN;
      i2.as.binary.kind = kind;
      i2.as.binary.lhs = rhs;
      i2.as.binary.rhs = dst;
      i2.asm_type = dst.asm_type;

      vec_insert(instrs, i1);
      vec_insert(instrs, i2);

      break;
    }
    case IRInstr_UNARY: {
      struct AsmOperand src, dst;

      src = codegen_irvalue(ir_instr->as.unary.src);
      dst = codegen_irvalue(ir_instr->as.unary.dst);

      if (ir_instr->as.unary.kind == IRInstrUnary_NOT) {
        struct AsmInstr i1 = {0}, i2 = {0}, i3 = {0};

        i1.kind = AsmInstr_MOV;
        i1.as.mov.src = src;
        i1.as.mov.dst = dst;
        i1.asm_type = dst.asm_type;

        i2.kind = AsmInstr_CMP;
        i2.as.cmp.lhs =
            (struct AsmOperand){.kind = AsmOperand_IMM, .as.imm = 0};
        i2.as.cmp.rhs = dst;
        i2.asm_type = dst.asm_type;

        i3.kind = AsmInstr_SetCC;
        i3.as.setcc.cc = CC_E;
        i3.as.setcc.op = dst;

        vec_insert(instrs, i1);
        vec_insert(instrs, i2);
        vec_insert(instrs, i3);
      } else {
        struct AsmInstr i1 = {0}, i2 = {0};

        i1.kind = AsmInstr_MOV;
        i1.as.mov.src = src;
        i1.as.mov.dst = dst;
        i1.asm_type = dst.asm_type;

        i2.kind = AsmInstr_UNARY;
        i2.as.unary.kind = (ir_instr->as.unary.kind == IRInstrUnary_BIT_NOT)
                               ? AsmInstrUnary_BIT_NOT
                               : AsmInstrUnary_NEG;
        i2.as.unary.op = dst;
        i2.asm_type = dst.asm_type;

        vec_insert(instrs, i1);
        vec_insert(instrs, i2);
      }

      break;
    }
    case IRInstr_RET: {
      struct AsmInstrPop pop;
      struct AsmInstrMov mov;
      struct AsmInstrRet ret;
      struct AsmInstr e1 = {0}, e2 = {0}, i2 = {0};

      ret.__dummy = 0;

      if (ir_instr->as.ret.val) {
        struct AsmOperand retval = codegen_irvalue(ir_instr->as.ret.val);

        if (fn_retval && fn_retval->kind == STRUCT_T) {
          struct ABIClassification cls = classify_type(fn_retval);
          int size, align;
          get_type_size_and_align(fn_retval, &size, &align);

          if (is_sret(fn_retval)) {
            struct AsmOperand sret_slot = {
                .kind = AsmOperand_PSEUDO,
                .as.pseudo = "$__sret_ptr",
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

            struct AsmOperand rsi = {
                .kind = AsmOperand_REG,
                .as.reg = SI,
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

            struct AsmOperand rdi = {
                .kind = AsmOperand_REG,
                .as.reg = DI,
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

            struct AsmOperand rcx = {
                .kind = AsmOperand_REG,
                .as.reg = CX,
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

            struct AsmOperand rax = {
                .kind = AsmOperand_REG,
                .as.reg = AX,
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

            vec_insert(instrs, ((struct AsmInstr){
                                   .kind = AsmInstr_LEA,
                                   .as.lea = {.src = retval, .dst = rsi}}));

            vec_insert(
                instrs,
                ((struct AsmInstr){
                    .kind = AsmInstr_MOV,
                    .asm_type = (struct AsmType){.kind = AsmType_QUADWORD},
                    .as.mov = {.src = sret_slot, .dst = rdi}}));

            vec_insert(
                instrs,
                ((struct AsmInstr){
                    .kind = AsmInstr_MOV,
                    .asm_type = (struct AsmType){.kind = AsmType_QUADWORD},
                    .as.mov = {.src = {.kind = AsmOperand_IMM, .as.imm = size},
                               .dst = rcx}}));

            vec_insert(instrs, ((struct AsmInstr){.kind = AsmInstr_REP_MOVSB}));

            vec_insert(
                instrs,
                ((struct AsmInstr){
                    .kind = AsmInstr_MOV,
                    .asm_type = (struct AsmType){.kind = AsmType_QUADWORD},
                    .as.mov = {.src = sret_slot, .dst = rax}}));
          } else {
            enum AsmRegister int_ret_regs[] = {AX, DX};
            enum AsmRegister xmm_ret_regs[] = {XMM0, XMM1};

            int int_ret_idx = 0;
            int xmm_ret_idx = 0;
            int num_eb = (size + 7) / 8;

            struct AsmOperand r10 = {
                .kind = AsmOperand_REG,
                .as.reg = R10,
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

            vec_insert(instrs, ((struct AsmInstr){
                                   .kind = AsmInstr_LEA,
                                   .as.lea = {.src = retval, .dst = r10}}));

            for (int eb = 0; eb < num_eb; eb++) {
              bool is_sse = cls.eightbytes[eb] == ABI_SSE;

              struct AsmType eb_type =
                  is_sse ? (struct AsmType){.kind = AsmType_DOUBLE}
                         : (struct AsmType){.kind = AsmType_QUADWORD};

              enum AsmRegister reg = is_sse ? xmm_ret_regs[xmm_ret_idx++]
                                            : int_ret_regs[int_ret_idx++];

              struct AsmOperand mem_src = {
                  .kind = AsmOperand_MEMORY,
                  .as.mem = {.base = R10, .offset = eb * 8},
                  .asm_type = eb_type};

              struct AsmOperand dst_reg = {
                  .kind = AsmOperand_REG, .as.reg = reg, .asm_type = eb_type};

              vec_insert(instrs,
                         ((struct AsmInstr){
                             .kind = AsmInstr_MOV,
                             .asm_type = eb_type,
                             .as.mov = {.src = mem_src, .dst = dst_reg}}));
            }
          }
        } else {
          bool is_ret_float = retval.asm_type.kind == AsmType_FLOAT ||
                              retval.asm_type.kind == AsmType_DOUBLE;

          struct AsmOperand dst_reg = {.kind = AsmOperand_REG,
                                       .as.reg = is_ret_float ? XMM0 : AX,
                                       .asm_type = retval.asm_type};

          vec_insert(instrs, ((struct AsmInstr){
                                 .kind = AsmInstr_MOV,
                                 .asm_type = retval.asm_type,
                                 .as.mov = {.src = retval, .dst = dst_reg}}));
        }
      }

      mov.src = (struct AsmOperand){
          .kind = AsmOperand_REG,
          .as.reg = BP,
          .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

      mov.dst = (struct AsmOperand){
          .kind = AsmOperand_REG,
          .as.reg = SP,
          .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

      pop.op = (struct AsmOperand){
          .kind = AsmOperand_REG,
          .as.reg = BP,
          .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

      e1.kind = AsmInstr_MOV;
      e1.as.mov = mov;
      e1.asm_type = (struct AsmType){.kind = AsmType_QUADWORD};

      e2.kind = AsmInstr_POP;
      e2.as.pop = pop;

      i2.kind = AsmInstr_RET;
      i2.as.ret = ret;

      vec_insert(instrs, e1);
      vec_insert(instrs, e2);
      vec_insert(instrs, i2);

      break;
    }
    case IRInstr_CPY: {
      struct AsmOperand src = codegen_irvalue(ir_instr->as.copy.src);
      struct AsmOperand dst = codegen_irvalue(ir_instr->as.copy.dst);

      int size, align;
      get_type_size_and_align(&ir_instr->as.copy.src->type, &size, &align);

      if (ir_instr->as.copy.src->type.kind != STRUCT_T && size <= 8) {
        vec_insert(instrs,
                   ((struct AsmInstr){.kind = AsmInstr_MOV,
                                      .asm_type = src.asm_type,
                                      .as.mov = {.src = src, .dst = dst}}));
      } else {
        struct AsmOperand rsi = {
            .kind = AsmOperand_REG,
            .as.reg = SI,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        struct AsmOperand rdi = {
            .kind = AsmOperand_REG,
            .as.reg = DI,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        struct AsmOperand rcx = {
            .kind = AsmOperand_REG,
            .as.reg = CX,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        vec_insert(instrs,
                   ((struct AsmInstr){.kind = AsmInstr_LEA,
                                      .as.lea = {.src = src, .dst = rsi}}));

        vec_insert(instrs,
                   ((struct AsmInstr){.kind = AsmInstr_LEA,
                                      .as.lea = {.src = dst, .dst = rdi}}));

        vec_insert(
            instrs,
            ((struct AsmInstr){
                .kind = AsmInstr_MOV,
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD},
                .as.mov = {.src = {.kind = AsmOperand_IMM, .as.imm = size},
                           .dst = rcx}}));

        vec_insert(instrs, ((struct AsmInstr){.kind = AsmInstr_REP_MOVSB}));
      }

      break;
    }
    case IRInstr_CALL: {
      /* According to the SystemV ABI, the first six int arguments and pointers
       * are passed via the int regs, and the first eight floating point
       * arguments are passed via the SSE registers. */
      enum AsmRegister int_arg_regs[] = {DI, SI, DX, CX, R8, R9};
      enum AsmRegister xmm_arg_regs[] = {XMM0, XMM1, XMM2, XMM3,
                                         XMM4, XMM5, XMM6, XMM7};

      int num_args = ir_instr->as.call.args.len;

      /* If the function we are calling returns a large struct by value,
       * we must pass a hidden pointer to allocated memory as the first
       * argument. */
      bool call_has_sret =
          ir_instr->as.call.dst && is_sret(&ir_instr->as.call.dst->type);

      /* If there is an sret, it uses up %rdi, so normal integer arguments
       * start at index 1 instead of 0. */
      int int_reg_idx = call_has_sret ? 1 : 0;
      int xmm_reg_idx = 0;
      int num_stack_bytes = 0;

      struct ArgLocation {
        bool is_memory;
        int num_eightbytes;
        enum AsmRegister regs[2];
        struct AsmType asm_types[2];
      } *arg_locs = malloc(sizeof(struct ArgLocation) * num_args);

      /* First pass: Loop through every argument to classify it.
       * We need to figure out which arguments go into registers, which fall
       * to the stack, and how much total stack space we need to allocate. */
      for (int i = 0; i < num_args; i++) {
        struct IRValue *arg_val = ir_instr->as.call.args.data[i];
        struct ABIClassification cls = classify_type(&arg_val->type);

        int type_size, type_align;
        get_type_size_and_align(&arg_val->type, &type_size, &type_align);

        int num_eb = (type_size + 7) / 8;
        bool falls_to_memory = cls.is_memory;
        int needed_int = 0;
        int needed_xmm = 0;

        if (!falls_to_memory) {
          for (int eb = 0; eb < num_eb; eb++) {
            if (cls.eightbytes[eb] == ABI_INTEGER) {
              needed_int++;
            }

            if (cls.eightbytes[eb] == ABI_SSE) {
              needed_xmm++;
            }
          }

          if (int_reg_idx + needed_int > 6 || xmm_reg_idx + needed_xmm > 8) {
            falls_to_memory = true;
          }
        }

        arg_locs[i].is_memory = falls_to_memory;
        arg_locs[i].num_eightbytes = num_eb;

        if (falls_to_memory) {
          num_stack_bytes += align_up_int(type_size, 8);
        } else {
          for (int eb = 0; eb < num_eb; eb++) {
            if (cls.eightbytes[eb] == ABI_INTEGER) {
              arg_locs[i].regs[eb] = int_arg_regs[int_reg_idx++];
              arg_locs[i].asm_types[eb] =
                  (struct AsmType){.kind = AsmType_QUADWORD};
            } else if (cls.eightbytes[eb] == ABI_SSE) {
              arg_locs[i].regs[eb] = xmm_arg_regs[xmm_reg_idx++];
              arg_locs[i].asm_types[eb] =
                  (struct AsmType){.kind = AsmType_DOUBLE};
            }
          }
        }
      }

      /* The SystemV ABI requires the stack pointer (%rsp) to be 16-byte aligned
       * right before the `call` instruction is executed.
       * We calculate the padding needed to satisfy this requirement. */
      int stack_padding =
          num_stack_bytes % 16 != 0 ? 16 - (num_stack_bytes % 16) : 0;
      int total_stack_adjustment = num_stack_bytes + stack_padding;

      /* Allocate space on the stack for memory arguments and padding. */
      if (total_stack_adjustment != 0) {
        struct AsmInstr padding_instr = {0};

        padding_instr.kind = AsmInstr_BIN;
        padding_instr.asm_type = (struct AsmType){.kind = AsmType_QUADWORD};
        padding_instr.as.binary.kind = AsmInstrBinary_SUB;
        padding_instr.as.binary.lhs = (struct AsmOperand){
            .kind = AsmOperand_IMM, .as.imm = total_stack_adjustment};
        padding_instr.as.binary.rhs = (struct AsmOperand){
            .kind = AsmOperand_REG,
            .as.reg = SP,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        vec_insert(instrs, padding_instr);
      }

      int current_stack_offset = 0;

      /* Second pass: Push the stack-bound arguments into the space we just
       * allocated. We do this before loading register arguments to avoid
       * accidentally clobbering the argument registers if evaluating these
       * expressions is complex. */
      for (int i = 0; i < num_args; i++) {
        if (!arg_locs[i].is_memory) {
          continue;
        }

        struct AsmOperand src_op =
            codegen_irvalue(ir_instr->as.call.args.data[i]);

        int size, align;
        get_type_size_and_align(&ir_instr->as.call.args.data[i]->type, &size,
                                &align);

        struct AsmOperand dst_op = {
            .kind = AsmOperand_MEMORY,
            .as.mem = {.base = SP, .offset = current_stack_offset},
            .asm_type = src_op.asm_type};

        /* If it is a primitive or small struct (<= 8 bytes), we can just
         * use a mov via a scratch register. */
        if (ir_instr->as.call.args.data[i]->type.kind != STRUCT_T &&
            size <= 8) {
          bool use_sse_scratch = src_op.asm_type.kind == AsmType_FLOAT ||
                                 src_op.asm_type.kind == AsmType_DOUBLE;

          struct AsmOperand scratch_reg = {
              .kind = AsmOperand_REG,
              .as.reg = use_sse_scratch ? XMM8 : R10,
              .asm_type = src_op.asm_type};

          vec_insert(instrs, ((struct AsmInstr){
                                 .kind = AsmInstr_MOV,
                                 .as.mov = {.src = src_op, .dst = scratch_reg},
                                 .asm_type = src_op.asm_type}));

          vec_insert(instrs, ((struct AsmInstr){
                                 .kind = AsmInstr_MOV,
                                 .as.mov = {.src = scratch_reg, .dst = dst_op},
                                 .asm_type = dst_op.asm_type}));
        } else {
          /* If it is a large struct, we need rep movsb to copy it to the stack.
           */
          struct AsmOperand rsi = {
              .kind = AsmOperand_REG,
              .as.reg = SI,
              .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

          struct AsmOperand rdi = {
              .kind = AsmOperand_REG,
              .as.reg = DI,
              .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

          struct AsmOperand rcx = {
              .kind = AsmOperand_REG,
              .as.reg = CX,
              .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

          vec_insert(instrs, ((struct AsmInstr){
                                 .kind = AsmInstr_LEA,
                                 .as.lea = {.src = src_op, .dst = rsi}}));

          vec_insert(instrs, ((struct AsmInstr){
                                 .kind = AsmInstr_LEA,
                                 .as.lea = {.src = dst_op, .dst = rdi}}));

          vec_insert(
              instrs,
              ((struct AsmInstr){
                  .kind = AsmInstr_MOV,
                  .asm_type = (struct AsmType){.kind = AsmType_QUADWORD},
                  .as.mov = {.src = {.kind = AsmOperand_IMM, .as.imm = size},
                             .dst = rcx}}));

          vec_insert(instrs, ((struct AsmInstr){.kind = AsmInstr_REP_MOVSB}));
        }

        current_stack_offset += align_up_int(size, 8);
      }

      /* If the function has an sret, load the address of our local memory
       * destination into %rdi (the hidden first argument). */
      if (call_has_sret) {
        struct AsmOperand dst_op = codegen_irvalue(ir_instr->as.call.dst);

        struct AsmOperand rdi = {
            .kind = AsmOperand_REG,
            .as.reg = DI,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        vec_insert(instrs,
                   ((struct AsmInstr){.kind = AsmInstr_LEA,
                                      .as.lea = {.src = dst_op, .dst = rdi}}));
      }

      /* Third pass: Load the arguments that fit into hardware registers. */
      for (int i = 0; i < num_args; i++) {
        if (arg_locs[i].is_memory) {
          continue;
        }

        struct AsmOperand src_op =
            codegen_irvalue(ir_instr->as.call.args.data[i]);

        if (arg_locs[i].num_eightbytes == 1) {
          if (ir_instr->as.call.args.data[i]->type.kind == STRUCT_T) {
            /* Small struct passed in a single register: move it from memory. */
            struct AsmOperand r10 = {
                .kind = AsmOperand_REG,
                .as.reg = R10,
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

            struct AsmOperand mem0 = {.kind = AsmOperand_MEMORY,
                                      .as.mem = {.base = R10, .offset = 0},
                                      .asm_type = arg_locs[i].asm_types[0]};

            struct AsmOperand dst_reg = {.kind = AsmOperand_REG,
                                         .as.reg = arg_locs[i].regs[0],
                                         .asm_type = arg_locs[i].asm_types[0]};

            vec_insert(instrs, ((struct AsmInstr){
                                   .kind = AsmInstr_LEA,
                                   .as.lea = {.src = src_op, .dst = r10}}));

            vec_insert(instrs, ((struct AsmInstr){
                                   .kind = AsmInstr_MOV,
                                   .asm_type = mem0.asm_type,
                                   .as.mov = {.src = mem0, .dst = dst_reg}}));
          } else {
            /* Normal primitive: move directly into the target register. */
            struct AsmOperand dst_reg = {.kind = AsmOperand_REG,
                                         .as.reg = arg_locs[i].regs[0],
                                         .asm_type = src_op.asm_type};

            vec_insert(instrs, ((struct AsmInstr){
                                   .kind = AsmInstr_MOV,
                                   .as.mov = {.src = src_op, .dst = dst_reg},
                                   .asm_type = src_op.asm_type}));
          }
        } else {
          /* Medium struct (9-16 bytes) passed split across two registers:
           * Load the first 8 bytes into the first reg, and the rest into the
           * second. */
          struct AsmOperand scratch_ptr = {
              .kind = AsmOperand_REG,
              .as.reg = R10,
              .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

          struct AsmOperand mem0 = {.kind = AsmOperand_MEMORY,
                                    .as.mem = {.base = R10, .offset = 0},
                                    .asm_type = arg_locs[i].asm_types[0]};

          struct AsmOperand dst_reg0 = {.kind = AsmOperand_REG,
                                        .as.reg = arg_locs[i].regs[0],
                                        .asm_type = arg_locs[i].asm_types[0]};

          struct AsmOperand mem1 = {.kind = AsmOperand_MEMORY,
                                    .as.mem = {.base = R10, .offset = 8},
                                    .asm_type = arg_locs[i].asm_types[1]};

          struct AsmOperand dst_reg1 = {.kind = AsmOperand_REG,
                                        .as.reg = arg_locs[i].regs[1],
                                        .asm_type = arg_locs[i].asm_types[1]};

          vec_insert(instrs,
                     ((struct AsmInstr){
                         .kind = AsmInstr_LEA,
                         .as.lea = {.src = src_op, .dst = scratch_ptr}}));

          vec_insert(instrs, ((struct AsmInstr){
                                 .kind = AsmInstr_MOV,
                                 .as.mov = {.src = mem0, .dst = dst_reg0},
                                 .asm_type = mem0.asm_type}));

          vec_insert(instrs, ((struct AsmInstr){
                                 .kind = AsmInstr_MOV,
                                 .as.mov = {.src = mem1, .dst = dst_reg1},
                                 .asm_type = mem1.asm_type}));
        }
      }

      /* For functions with variable arguments (varargs), the System V ABI
       * requires that the %al register contains the number of vector (SSE)
       * registers used. */
      struct AsmInstr eax_instr = {0};

      eax_instr.kind = AsmInstr_MOV;
      eax_instr.asm_type = (struct AsmType){.kind = AsmType_LONGWORD};
      eax_instr.as.mov.src =
          (struct AsmOperand){.kind = AsmOperand_IMM, .as.imm = xmm_reg_idx};
      eax_instr.as.mov.dst = (struct AsmOperand){
          .kind = AsmOperand_REG,
          .as.reg = AX,
          .asm_type = (struct AsmType){.kind = AsmType_LONGWORD}};

      vec_insert(instrs, eax_instr);

      /* Emit the actual CALL instruction. */
      struct AsmInstr call_instr = {0};
      call_instr.kind = AsmInstr_CALL;
      call_instr.as.call.target = ir_instr->as.call.target.as.var.name;

      vec_insert(instrs, call_instr);

      /* After the function returns, clean up the stack space we allocated
       * for the stack arguments and alignment padding. */
      if (total_stack_adjustment != 0) {
        struct AsmInstr cleanup_instr = {0};

        cleanup_instr.kind = AsmInstr_BIN;
        cleanup_instr.asm_type = (struct AsmType){.kind = AsmType_QUADWORD};
        cleanup_instr.as.binary.kind = AsmInstrBinary_ADD;
        cleanup_instr.as.binary.lhs = (struct AsmOperand){
            .kind = AsmOperand_IMM, .as.imm = total_stack_adjustment};
        cleanup_instr.as.binary.rhs = (struct AsmOperand){
            .kind = AsmOperand_REG,
            .as.reg = SP,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        vec_insert(instrs, cleanup_instr);
      }

      /* Handle the return value (if there is one, and it wasn't an sret). */
      if (ir_instr->as.call.dst && !call_has_sret) {
        struct AsmOperand dst_op = codegen_irvalue(ir_instr->as.call.dst);

        if (ir_instr->as.call.dst->type.kind == STRUCT_T) {
          /* If a struct is returned in registers, the ABI says it will be split
           * across %rax and %rdx (or %xmm0 and %xmm1). We must piece it back
           * together into our local memory destination. */
          struct ABIClassification cls =
              classify_type(&ir_instr->as.call.dst->type);

          int size, align;
          get_type_size_and_align(&ir_instr->as.call.dst->type, &size, &align);

          int num_eb = (size + 7) / 8;

          enum AsmRegister int_ret_regs[] = {AX, DX};
          enum AsmRegister xmm_ret_regs[] = {XMM0, XMM1};

          int int_ret_idx = 0;
          int xmm_ret_idx = 0;

          struct AsmOperand r10 = {
              .kind = AsmOperand_REG,
              .as.reg = R10,
              .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

          vec_insert(instrs, ((struct AsmInstr){
                                 .kind = AsmInstr_LEA,
                                 .as.lea = {.src = dst_op, .dst = r10}}));

          for (int eb = 0; eb < num_eb; eb++) {
            bool is_sse = cls.eightbytes[eb] == ABI_SSE;

            struct AsmType eb_type =
                is_sse ? (struct AsmType){.kind = AsmType_DOUBLE}
                       : (struct AsmType){.kind = AsmType_QUADWORD};

            enum AsmRegister reg = is_sse ? xmm_ret_regs[xmm_ret_idx++]
                                          : int_ret_regs[int_ret_idx++];

            struct AsmOperand src_reg = {
                .kind = AsmOperand_REG, .as.reg = reg, .asm_type = eb_type};

            struct AsmOperand mem_dst = {
                .kind = AsmOperand_MEMORY,
                .as.mem = {.base = R10, .offset = eb * 8},
                .asm_type = eb_type};

            vec_insert(instrs, ((struct AsmInstr){.kind = AsmInstr_MOV,
                                                  .asm_type = eb_type,
                                                  .as.mov = {.src = src_reg,
                                                             .dst = mem_dst}}));
          }
        } else {
          /* Standard primitive return: grab it from %rax (int) or %xmm0
           * (float). */
          struct AsmInstr mov_instr = {0};

          bool is_dst_float = dst_op.asm_type.kind == AsmType_FLOAT ||
                              dst_op.asm_type.kind == AsmType_DOUBLE;

          mov_instr.kind = AsmInstr_MOV;
          mov_instr.as.mov.src =
              (struct AsmOperand){.kind = AsmOperand_REG,
                                  .as.reg = is_dst_float ? XMM0 : AX,
                                  .asm_type = dst_op.asm_type};
          mov_instr.as.mov.dst = dst_op;
          mov_instr.asm_type = dst_op.asm_type;

          vec_insert(instrs, mov_instr);
        }
      }

      break;
    }
    case IRInstr_JMP: {
      struct AsmInstr i = {0};
      struct AsmInstrJmp jmp;

      jmp.target = ir_instr->as.jmp.target;

      i.kind = AsmInstr_JMP;
      i.as.jmp = jmp;

      vec_insert(instrs, i);
      break;
    }
    case IRInstr_JZ: {
      struct AsmInstr i1 = {0}, i2 = {0};
      struct AsmInstrCmp cmp;
      struct AsmOperand cond;

      cond = codegen_irvalue(&ir_instr->as.jz.cond);

      i1.kind = AsmInstr_CMP;
      cmp.asm_type = cond.asm_type;
      cmp.lhs = (struct AsmOperand){.kind = AsmOperand_IMM, .as.imm = 0};
      cmp.rhs = cond;
      i1.as.cmp = cmp;

      i2.kind = AsmInstr_JmpCC;
      i2.as.jmpcc.cc = CC_E;
      i2.as.jmpcc.target = ir_instr->as.jz.target;

      vec_insert(instrs, i1);
      vec_insert(instrs, i2);

      break;
    }
    case IRInstr_LBL: {
      struct AsmInstr i = {0};
      struct AsmInstrLabel lbl;

      lbl.name = ir_instr->as.label.name;

      i.kind = AsmInstr_LBL;
      i.as.lbl = lbl;

      vec_insert(instrs, i);
      break;
    }
    case IRInstr_GETADDR: {
      struct AsmInstr i = {0};

      i.kind = AsmInstr_LEA;
      i.asm_type = (struct AsmType){.kind = AsmType_QUADWORD};
      i.as.lea.src = codegen_irvalue(ir_instr->as.getaddr.src);
      i.as.lea.dst = codegen_irvalue(ir_instr->as.getaddr.dst);

      vec_insert(instrs, i);
      break;
    }
    case IRInstr_LOAD: {
      struct AsmOperand src_ptr = codegen_irvalue(ir_instr->as.load.src);
      struct AsmOperand dst_val = codegen_irvalue(ir_instr->as.load.dst);

      int size, align;
      get_type_size_and_align(&ir_instr->as.load.dst->type, &size, &align);

      if (ir_instr->as.load.dst->type.kind != STRUCT_T && size <= 8) {
        struct AsmOperand scratch_ptr = {
            .kind = AsmOperand_REG,
            .as.reg = R10,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        vec_insert(
            instrs,
            ((struct AsmInstr){
                .kind = AsmInstr_MOV,
                .as.mov = {.src = src_ptr, .dst = scratch_ptr},
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}}));

        struct AsmOperand mem_op = {.kind = AsmOperand_MEMORY,
                                    .as.mem = {.base = R10, .offset = 0},
                                    .asm_type = dst_val.asm_type};

        vec_insert(instrs,
                   ((struct AsmInstr){.kind = AsmInstr_MOV,
                                      .as.mov = {.src = mem_op, .dst = dst_val},
                                      .asm_type = dst_val.asm_type}));
      } else {
        struct AsmOperand rsi = {
            .kind = AsmOperand_REG,
            .as.reg = SI,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        struct AsmOperand rdi = {
            .kind = AsmOperand_REG,
            .as.reg = DI,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        struct AsmOperand rcx = {
            .kind = AsmOperand_REG,
            .as.reg = CX,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        vec_insert(instrs,
                   ((struct AsmInstr){.kind = AsmInstr_MOV,
                                      .as.mov = {.src = src_ptr, .dst = rsi},
                                      .asm_type = (struct AsmType){
                                          .kind = AsmType_QUADWORD}}));

        vec_insert(instrs,
                   ((struct AsmInstr){.kind = AsmInstr_LEA,
                                      .as.lea = {.src = dst_val, .dst = rdi}}));

        vec_insert(
            instrs,
            ((struct AsmInstr){
                .kind = AsmInstr_MOV,
                .as.mov = {.src = {.kind = AsmOperand_IMM, .as.imm = size},
                           .dst = rcx},
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}}));

        vec_insert(instrs, ((struct AsmInstr){.kind = AsmInstr_REP_MOVSB}));
      }

      break;
    }
    case IRInstr_STORE: {
      struct AsmOperand src_val = codegen_irvalue(ir_instr->as.store.val);
      struct AsmOperand dst_ptr = codegen_irvalue(ir_instr->as.store.dst);

      int size, align;
      get_type_size_and_align(&ir_instr->as.store.val->type, &size, &align);

      if (ir_instr->as.store.val->type.kind != STRUCT_T && size <= 8) {
        struct AsmOperand scratch_val = {
            .kind = AsmOperand_REG, .as.reg = R9, .asm_type = src_val.asm_type};

        vec_insert(instrs, ((struct AsmInstr){
                               .kind = AsmInstr_MOV,
                               .as.mov = {.src = src_val, .dst = scratch_val},
                               .asm_type = src_val.asm_type}));

        struct AsmOperand scratch_ptr = {
            .kind = AsmOperand_REG,
            .as.reg = R10,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        vec_insert(
            instrs,
            ((struct AsmInstr){
                .kind = AsmInstr_MOV,
                .as.mov = {.src = dst_ptr, .dst = scratch_ptr},
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}}));

        struct AsmOperand mem_op = {.kind = AsmOperand_MEMORY,
                                    .as.mem = {.base = R10, .offset = 0},
                                    .asm_type = src_val.asm_type};

        vec_insert(instrs, ((struct AsmInstr){
                               .kind = AsmInstr_MOV,
                               .as.mov = {.src = scratch_val, .dst = mem_op},
                               .asm_type = src_val.asm_type}));
      } else {
        struct AsmOperand rsi = {
            .kind = AsmOperand_REG,
            .as.reg = SI,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        struct AsmOperand rdi = {
            .kind = AsmOperand_REG,
            .as.reg = DI,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        struct AsmOperand rcx = {
            .kind = AsmOperand_REG,
            .as.reg = CX,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        vec_insert(instrs,
                   ((struct AsmInstr){.kind = AsmInstr_LEA,
                                      .as.lea = {.src = src_val, .dst = rsi}}));

        vec_insert(instrs,
                   ((struct AsmInstr){.kind = AsmInstr_MOV,
                                      .as.mov = {.src = dst_ptr, .dst = rdi},
                                      .asm_type = (struct AsmType){
                                          .kind = AsmType_QUADWORD}}));

        vec_insert(
            instrs,
            ((struct AsmInstr){
                .kind = AsmInstr_MOV,
                .as.mov = {.src = {.kind = AsmOperand_IMM, .as.imm = size},
                           .dst = rcx},
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}}));

        vec_insert(instrs, ((struct AsmInstr){.kind = AsmInstr_REP_MOVSB}));
      }

      break;
    }
    case IRInstr_CAST: {
      struct AsmOperand src = codegen_irvalue(ir_instr->as.cast.src);
      struct AsmOperand dst = codegen_irvalue(ir_instr->as.cast.dst);

      if (src.kind == AsmOperand_IMM) {
        struct AsmInstr i = {0};
        i.kind = AsmInstr_MOV;
        i.as.mov.src = src;
        i.as.mov.dst = dst;
        i.asm_type = dst.asm_type;
        vec_insert(instrs, i);
      } else if (ir_instr->as.cast.kind == IRCast_Truncate) {
        struct AsmOperand narrowed_src = src;
        narrowed_src.asm_type = dst.asm_type;
        struct AsmInstr i = {0};
        i.kind = AsmInstr_MOV;
        i.as.mov.src = narrowed_src;
        i.as.mov.dst = dst;
        i.asm_type = dst.asm_type;
        vec_insert(instrs, i);
      } else {
        enum IRCastKind ir_k = ir_instr->as.cast.kind;

        if (ir_k == IRCast_FloatToUInt) {
          ir_k = IRCast_FloatToInt;
        }
        if (ir_k == IRCast_DoubleToUInt) {
          ir_k = IRCast_DoubleToInt;
        }

        if (ir_k == IRCast_UIntToFloat || ir_k == IRCast_UIntToDouble) {
          int src_sz = asm_type_stack_size(src.asm_type);
          if (src_sz < 8) {
            struct AsmOperand r10 = {
                .kind = AsmOperand_REG,
                .as.reg = R10,
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};
            struct AsmInstr zext = {0};
            zext.kind = AsmInstr_CVT;
            zext.as.cvt.kind = AsmCast_ZeroExtend;
            zext.as.cvt.src = src;
            zext.as.cvt.dst = r10;
            vec_insert(instrs, zext);
            src = r10;
          }
          ir_k = (ir_k == IRCast_UIntToFloat) ? IRCast_IntToFloat
                                              : IRCast_IntToDouble;
        }

        enum AsmCastKind asm_k;
        switch (ir_k) {
          case IRCast_SignExtend:
            asm_k = AsmCast_SignExtend;
            break;
          case IRCast_ZeroExtend:
            asm_k = AsmCast_ZeroExtend;
            break;
          case IRCast_FloatPromote:
            asm_k = AsmCast_FloatPromote;
            break;
          case IRCast_FloatDemote:
            asm_k = AsmCast_FloatDemote;
            break;
          case IRCast_IntToFloat:
            asm_k = AsmCast_IntToFloat;
            break;
          case IRCast_IntToDouble:
            asm_k = AsmCast_IntToDouble;
            break;
          case IRCast_FloatToInt:
            asm_k = AsmCast_FloatToInt;
            break;
          case IRCast_DoubleToInt:
            asm_k = AsmCast_DoubleToInt;
            break;
          default:
            assert(0 && "Unhandled mapped cast variant");
        }

        struct AsmInstr i = {0};
        i.kind = AsmInstr_CVT;
        i.as.cvt.kind = asm_k;
        i.as.cvt.src = src;
        i.as.cvt.dst = dst;
        i.asm_type = dst.asm_type;
        vec_insert(instrs, i);
      }
      break;
    }
    case IRInstr_ADD_PTR: {
      struct AsmInstr mov1 = {0};
      mov1.kind = AsmInstr_MOV;
      mov1.asm_type = (struct AsmType){.kind = AsmType_QUADWORD};
      mov1.as.mov.src = codegen_irvalue(ir_instr->as.add_ptr.ptr);
      mov1.as.mov.dst = (struct AsmOperand){
          .kind = AsmOperand_REG,
          .as.reg = AX,
          .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};
      vec_insert(instrs, mov1);

      struct AsmInstr mov2 = {0};
      mov2.kind = AsmInstr_MOV;
      mov2.asm_type = (struct AsmType){.kind = AsmType_QUADWORD};
      mov2.as.mov.src = codegen_irvalue(ir_instr->as.add_ptr.index);
      mov2.as.mov.dst = (struct AsmOperand){
          .kind = AsmOperand_REG,
          .as.reg = DX,
          .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};
      vec_insert(instrs, mov2);

      int scale = ir_instr->as.add_ptr.scale;
      if (scale == 1 || scale == 2 || scale == 4 || scale == 8) {
        struct AsmInstr lea_instr = {0};
        lea_instr.kind = AsmInstr_LEA;
        lea_instr.asm_type = (struct AsmType){.kind = AsmType_QUADWORD};
        lea_instr.as.lea.src = (struct AsmOperand){
            .kind = AsmOperand_INDEXED,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD},
            .as.indexed = {.base = AX, .index = DX, .scale = scale}};
        lea_instr.as.lea.dst = codegen_irvalue(ir_instr->as.add_ptr.dst);
        vec_insert(instrs, lea_instr);
      } else {
        struct AsmInstr imul_instr = {0};
        imul_instr.kind = AsmInstr_BIN;
        imul_instr.asm_type = (struct AsmType){.kind = AsmType_QUADWORD};
        imul_instr.as.binary.kind = AsmInstrBinary_MUL;
        imul_instr.as.binary.lhs = (struct AsmOperand){
            .kind = AsmOperand_IMM,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD},
            .as.imm = scale};
        imul_instr.as.binary.rhs = (struct AsmOperand){
            .kind = AsmOperand_REG,
            .as.reg = DX,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};
        vec_insert(instrs, imul_instr);

        struct AsmInstr lea_fallback = {0};
        lea_fallback.kind = AsmInstr_LEA;
        lea_fallback.asm_type = (struct AsmType){.kind = AsmType_QUADWORD};
        lea_fallback.as.lea.src = (struct AsmOperand){
            .kind = AsmOperand_INDEXED,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD},
            .as.indexed = {.base = AX, .index = DX, .scale = 1}};
        lea_fallback.as.lea.dst = codegen_irvalue(ir_instr->as.add_ptr.dst);
        vec_insert(instrs, lea_fallback);
      }
      break;
    }
    case IRInstr_CPY_FROM_OFFSET: {
      struct AsmOperand src = codegen_irvalue(ir_instr->as.cpy_from_offset.src);
      struct AsmOperand dst = codegen_irvalue(ir_instr->as.cpy_from_offset.dst);

      int size, align;
      get_type_size_and_align(&ir_instr->as.cpy_from_offset.dst->type, &size,
                              &align);

      if (ir_instr->as.cpy_from_offset.dst->type.kind != STRUCT_T &&
          size <= 8) {
        struct AsmOperand scratch_reg = {
            .kind = AsmOperand_REG,
            .as.reg = R10,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        struct AsmInstr i1 = {0};
        i1.kind = AsmInstr_LEA;
        i1.as.lea.src = src;
        i1.as.lea.dst = scratch_reg;

        struct AsmOperand mem_op = {
            .kind = AsmOperand_MEMORY,
            .as.mem = {.base = R10,
                       .offset = ir_instr->as.cpy_from_offset.offset},
            .asm_type = dst.asm_type};

        struct AsmInstr i2 = {0};
        i2.kind = AsmInstr_MOV;
        i2.as.mov.src = mem_op;
        i2.as.mov.dst = dst;
        i2.asm_type = dst.asm_type;

        vec_insert(instrs, i1);
        vec_insert(instrs, i2);
      } else {
        struct AsmOperand r10 = {
            .kind = AsmOperand_REG,
            .as.reg = R10,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        struct AsmOperand rsi = {
            .kind = AsmOperand_REG,
            .as.reg = SI,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        struct AsmOperand rdi = {
            .kind = AsmOperand_REG,
            .as.reg = DI,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        struct AsmOperand rcx = {
            .kind = AsmOperand_REG,
            .as.reg = CX,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        vec_insert(instrs,
                   ((struct AsmInstr){.kind = AsmInstr_LEA,
                                      .as.lea = {.src = src, .dst = r10}}));

        struct AsmOperand mem_offset = {
            .kind = AsmOperand_MEMORY,
            .as.mem = {.base = R10,
                       .offset = ir_instr->as.cpy_from_offset.offset}};

        vec_insert(instrs, ((struct AsmInstr){
                               .kind = AsmInstr_LEA,
                               .as.lea = {.src = mem_offset, .dst = rsi}}));

        vec_insert(instrs,
                   ((struct AsmInstr){.kind = AsmInstr_LEA,
                                      .as.lea = {.src = dst, .dst = rdi}}));

        vec_insert(
            instrs,
            ((struct AsmInstr){
                .kind = AsmInstr_MOV,
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD},
                .as.mov = {.src = {.kind = AsmOperand_IMM, .as.imm = size},
                           .dst = rcx}}));

        vec_insert(instrs, ((struct AsmInstr){.kind = AsmInstr_REP_MOVSB}));
      }

      break;
    }
    case IRInstr_CPY_TO_OFFSET: {
      struct AsmOperand dst = codegen_irvalue(ir_instr->as.cpy_to_offset.dst);
      struct AsmOperand src = codegen_irvalue(ir_instr->as.cpy_to_offset.src);

      int size, align;
      get_type_size_and_align(&ir_instr->as.cpy_to_offset.src->type, &size,
                              &align);

      if (ir_instr->as.cpy_to_offset.src->type.kind != STRUCT_T && size <= 8) {
        struct AsmOperand scratch_reg = {
            .kind = AsmOperand_REG,
            .as.reg = R10,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        vec_insert(instrs, ((struct AsmInstr){
                               .kind = AsmInstr_LEA,
                               .as.lea = {.src = dst, .dst = scratch_reg}}));

        struct AsmOperand mem_op = {
            .kind = AsmOperand_MEMORY,
            .as.mem = {.base = R10,
                       .offset = ir_instr->as.cpy_to_offset.offset},
            .asm_type = src.asm_type};

        vec_insert(instrs,
                   ((struct AsmInstr){.kind = AsmInstr_MOV,
                                      .asm_type = src.asm_type,
                                      .as.mov = {.src = src, .dst = mem_op}}));
      } else {
        struct AsmOperand r10 = {
            .kind = AsmOperand_REG,
            .as.reg = R10,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        struct AsmOperand rsi = {
            .kind = AsmOperand_REG,
            .as.reg = SI,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        struct AsmOperand rdi = {
            .kind = AsmOperand_REG,
            .as.reg = DI,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        struct AsmOperand rcx = {
            .kind = AsmOperand_REG,
            .as.reg = CX,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        vec_insert(instrs,
                   ((struct AsmInstr){.kind = AsmInstr_LEA,
                                      .as.lea = {.src = src, .dst = rsi}}));

        vec_insert(instrs,
                   ((struct AsmInstr){.kind = AsmInstr_LEA,
                                      .as.lea = {.src = dst, .dst = r10}}));

        struct AsmOperand mem_offset = {
            .kind = AsmOperand_MEMORY,
            .as.mem = {.base = R10,
                       .offset = ir_instr->as.cpy_to_offset.offset}};

        vec_insert(instrs, ((struct AsmInstr){
                               .kind = AsmInstr_LEA,
                               .as.lea = {.src = mem_offset, .dst = rdi}}));

        vec_insert(
            instrs,
            ((struct AsmInstr){
                .kind = AsmInstr_MOV,
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD},
                .as.mov = {.src = {.kind = AsmOperand_IMM, .as.imm = size},
                           .dst = rcx}}));

        vec_insert(instrs, ((struct AsmInstr){.kind = AsmInstr_REP_MOVSB}));
      }

      break;
    }
    default:
      break;
  }
}

struct AsmFunction codegen_fn(struct IRFunction *ir_func)
{
  struct AsmFunction func = {0};
  struct AsmInstr p1, p2, p3;
  struct AsmInstrPush push;
  struct AsmInstrMov mov;
  struct AsmInstrBinary sub;

  func.name = ir_func->name;

  /*  In the prologue, we save the caller's BP.  */
  push.op = (struct AsmOperand){
      .kind = AsmOperand_REG,
      .as.reg = BP,
      .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

  p1.kind = AsmInstr_PUSH;
  p1.as.push = push;

  /*  ...and place our SP into BP.  */
  mov.src = (struct AsmOperand){
      .kind = AsmOperand_REG,
      .as.reg = SP,
      .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};
  mov.dst = (struct AsmOperand){
      .kind = AsmOperand_REG,
      .as.reg = BP,
      .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

  p2.kind = AsmInstr_MOV;
  p2.as.mov = mov;
  p2.asm_type = (struct AsmType){.kind = AsmType_QUADWORD};

  sub.kind = AsmInstrBinary_SUB;
  sub.lhs = (struct AsmOperand){.kind = AsmOperand_IMM, .as.imm = 0};
  sub.rhs = (struct AsmOperand){
      .kind = AsmOperand_REG,
      .as.reg = SP,
      .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

  p3.kind = AsmInstr_BIN;
  p3.as.binary = sub;
  p3.asm_type = (struct AsmType){.kind = AsmType_QUADWORD};

  vec_insert(&func.body, p1);
  vec_insert(&func.body, p2);
  vec_insert(&func.body, p3);

  /* When a function needs to return a large struct by value,
   * the struct wouldn't fit in the two regiters (%rax and %rdi),
   * which means it needs to be returned via the stack.
   *
   * This means that the caller will allocate space on its stack,
   * and pass the pointer to that space to the callee in %rdi.
   *
   * As the callee, we want to hold onto that address */
  bool fn_has_sret;

  fn_has_sret = is_sret(&ir_func->retval);
  if (fn_has_sret) {
    struct AsmOperand sret_slot = {
        .kind = AsmOperand_PSEUDO,
        .as.pseudo = "$__sret_ptr",
        .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

    struct AsmOperand rdi = {
        .kind = AsmOperand_REG,
        .as.reg = DI,
        .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

    vec_insert(&func.body,
               ((struct AsmInstr){
                   .kind = AsmInstr_MOV,
                   .asm_type = (struct AsmType){.kind = AsmType_QUADWORD},
                   .as.mov = {.src = rdi, .dst = sret_slot}}));
  }

  /* According to the SystemV ABI, the first six int arguments and pointers
   * are passed via the int regs, and the first eight floating point arguments
   * are passed via the SSE registers.  */
  enum AsmRegister int_arg_regs[] = {DI, SI, DX, CX, R8, R9};
  enum AsmRegister xmm_arg_regs[] = {XMM0, XMM1, XMM2, XMM3,
                                     XMM4, XMM5, XMM6, XMM7};

  int num_params = ir_func->params.len;

  /* If the fn has sret, this means that %rdi will have already been used up,
   * so we start at index 1.  */
  int int_reg_idx = fn_has_sret ? 1 : 0;
  int xmm_reg_idx = 0;

  /* In the SystemV ABI, during the call instruction, the CPU
   * will push the return address on the stack, which means that
   * this will decrement the stack pointer by 8.  Then an instruction
   * like `pushq %rbp`, will decrement the stack pointer by another 8.
   * Then we have `movq %rsp, %rbp`, which means that the return address
   * is at 8(%rbp), and first stack-passed argument will have been at 16(%rbp).
   * The locals are starting off at -8(%rbp).  */
  int stack_offset = 16;

  /* Loop through every parameter and classify it.
   * - How many 8-byte chunks ("eightbytes") the parameter takes?
   * - How many Integer vs. SSE registers this specific parameter needs?
   * - If passing this parameter would exceed the available registers (6 int,
   *   8 SSE), it forces falls_to_memory = true, meaning this argument must
   *   be fetched from the stack. */
  for (int i = 0; i < num_params; i++) {
    struct Parameter *param = &ir_func->params.data[i];
    struct ABIClassification cls = classify_type(&param->type);

    int size, align;
    get_type_size_and_align(&param->type, &size, &align);

    /* The math for the ceiling division below works out as follows:
     *
     * If size is 1 to 8 bytes: (size + 7) is 8 to 15. Integer division by 8
     * results in 1 eightbyte.
     *
     * If size is 9 to 16 bytes: (size + 7) is 16 to 23. Integer division by 8
     * results in 2 eightbytes.
     *
     * If size is 17 to 24 bytes: (size + 7) is 24 to 31. Integer division by 8
     * results in 3 eightbytes. */
    int num_eb = (size + 7) / 8;

    bool falls_to_memory = cls.is_memory;
    int needed_int = 0;
    int needed_xmm = 0;

    if (!falls_to_memory) {
      for (int eb = 0; eb < num_eb; eb++) {
        if (cls.eightbytes[eb] == ABI_INTEGER) {
          needed_int++;
        }

        if (cls.eightbytes[eb] == ABI_SSE) {
          needed_xmm++;
        }
      }

      if (int_reg_idx + needed_int > 6 || xmm_reg_idx + needed_xmm > 8) {
        falls_to_memory = true;
      }
    }

    struct AsmType param_asm_type = type_to_asm_type(param->type);

    struct AsmOperand dst;
    dst.kind = AsmOperand_PSEUDO;
    dst.as.pseudo = param->name;
    dst.asm_type = param_asm_type;

    if (falls_to_memory) {
      struct AsmOperand src_stack = {.kind = AsmOperand_STACK,
                                     .as.stack_offset = stack_offset,
                                     .asm_type = param_asm_type};

      /* If it is a struct, but smaller or equal to 8 bytes, then we can just
       * use mov. */
      if (param->type.kind != STRUCT_T && size <= 8) {
        bool use_sse_scratch = param_asm_type.kind == AsmType_FLOAT ||
                               param_asm_type.kind == AsmType_DOUBLE;

        struct AsmOperand scratch_reg = {
            .kind = AsmOperand_REG,
            .as.reg = use_sse_scratch ? XMM8 : R10,
            .asm_type = param_asm_type,
        };

        vec_insert(
            &func.body,
            ((struct AsmInstr){.kind = AsmInstr_MOV,
                               .as.mov = {.src = src_stack, .dst = scratch_reg},
                               .asm_type = param_asm_type}));

        vec_insert(&func.body, ((struct AsmInstr){
                                   .kind = AsmInstr_MOV,
                                   .as.mov = {.src = scratch_reg, .dst = dst},
                                   .asm_type = param_asm_type}));
      } else {
        /* If it is a struct, but larger than 8 bytes, we need rep movsb. */
        struct AsmOperand rsi = {
            .kind = AsmOperand_REG,
            .as.reg = SI,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        struct AsmOperand rdi = {
            .kind = AsmOperand_REG,
            .as.reg = DI,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        struct AsmOperand rcx = {
            .kind = AsmOperand_REG,
            .as.reg = CX,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        vec_insert(&func.body, ((struct AsmInstr){
                                   .kind = AsmInstr_LEA,
                                   .as.lea = {.src = src_stack, .dst = rsi}}));

        vec_insert(&func.body,
                   ((struct AsmInstr){.kind = AsmInstr_LEA,
                                      .as.lea = {.src = dst, .dst = rdi}}));

        vec_insert(
            &func.body,
            ((struct AsmInstr){
                .kind = AsmInstr_MOV,
                .asm_type = (struct AsmType){.kind = AsmType_QUADWORD},
                .as.mov = {.src = {.kind = AsmOperand_IMM, .as.imm = size},
                           .dst = rcx}}));

        vec_insert(&func.body, ((struct AsmInstr){.kind = AsmInstr_REP_MOVSB}));
      }

      stack_offset += align_up_int(size, 8);
    } else {
      /* If it does NOT fall to memory */
      if (num_eb <= 1) {
        /* ...and there is at most a single eightbyte, */
        if (param->type.kind == STRUCT_T) {
          /* ...but it's still a struct. */
          bool is_sse = cls.eightbytes[0] == ABI_SSE;

          enum AsmRegister reg = is_sse ? xmm_arg_regs[xmm_reg_idx++]
                                        : int_arg_regs[int_reg_idx++];

          struct AsmType eb_type =
              is_sse ? (struct AsmType){.kind = AsmType_DOUBLE}
                     : (struct AsmType){.kind = AsmType_QUADWORD};

          struct AsmOperand r10 = {
              .kind = AsmOperand_REG,
              .as.reg = R10,
              .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

          struct AsmOperand src_reg = {
              .kind = AsmOperand_REG, .as.reg = reg, .asm_type = eb_type};

          struct AsmOperand mem_dst = {.kind = AsmOperand_MEMORY,
                                       .as.mem = {.base = R10, .offset = 0},
                                       .asm_type = eb_type};

          vec_insert(&func.body,
                     ((struct AsmInstr){.kind = AsmInstr_LEA,
                                        .as.lea = {.src = dst, .dst = r10}}));

          vec_insert(
              &func.body,
              ((struct AsmInstr){.kind = AsmInstr_MOV,
                                 .asm_type = eb_type,
                                 .as.mov = {.src = src_reg, .dst = mem_dst}}));
        } else {
          /* not a struct */
          enum AsmRegister reg = cls.eightbytes[0] == ABI_SSE
                                     ? xmm_arg_regs[xmm_reg_idx++]
                                     : int_arg_regs[int_reg_idx++];

          struct AsmOperand src_reg = {.kind = AsmOperand_REG,
                                       .as.reg = reg,
                                       .asm_type = param_asm_type};

          vec_insert(&func.body,
                     ((struct AsmInstr){.kind = AsmInstr_MOV,
                                        .as.mov = {.src = src_reg, .dst = dst},
                                        .asm_type = param_asm_type}));
        }
      } else {
        /* there is more than 1 eightbyte */
        struct AsmOperand r10 = {
            .kind = AsmOperand_REG,
            .as.reg = R10,
            .asm_type = (struct AsmType){.kind = AsmType_QUADWORD}};

        vec_insert(&func.body,
                   ((struct AsmInstr){.kind = AsmInstr_LEA,
                                      .as.lea = {.src = dst, .dst = r10}}));

        for (int eb = 0; eb < num_eb; eb++) {
          enum AsmRegister reg = cls.eightbytes[eb] == ABI_SSE
                                     ? xmm_arg_regs[xmm_reg_idx++]
                                     : int_arg_regs[int_reg_idx++];

          struct AsmType eb_type =
              cls.eightbytes[eb] == ABI_SSE
                  ? (struct AsmType){.kind = AsmType_DOUBLE}
                  : (struct AsmType){.kind = AsmType_QUADWORD};

          struct AsmOperand src_reg = {
              .kind = AsmOperand_REG, .as.reg = reg, .asm_type = eb_type};

          struct AsmOperand mem_dst = {
              .kind = AsmOperand_MEMORY,
              .as.mem = {.base = R10, .offset = eb * 8},
              .asm_type = eb_type};

          vec_insert(&func.body, ((struct AsmInstr){
                                     .kind = AsmInstr_MOV,
                                     .as.mov = {.src = src_reg, .dst = mem_dst},
                                     .asm_type = eb_type}));
        }
      }
    }
  }

  for (int i = 0; i < ir_func->body.len; i++) {
    codegen_instr(&ir_func->body.data[i], &func.body, &ir_func->retval);
  }

  return func;
}

struct AsmResult codegen(struct IRProgram *ir_prog)
{
  struct AsmProgram prog = {0};
  struct AsmResult result;

  result.is_ok = true;
  result.msg = NULL;

  for (int i = 0; i < ir_prog->funcs.len; i++) {
    vec_insert(&prog.funcs, codegen_fn(ir_prog->funcs.data[i]));
  }

  result.prog = prog;

  return result;
}

struct AsmProgram *fixup(struct AsmProgram *prog)
{
  for (int i = 0; i < prog->funcs.len; i++) {
    VecAsmInstr instrs = {0};
    for (int j = 0; j < prog->funcs.data[i].body.len; j++) {
      struct AsmInstr *asminstr = &prog->funcs.data[i].body.data[j];
      switch (asminstr->kind) {
        case AsmInstr_CMP: {
          bool is_float, is_both_stack, is_dst_imm, is_dst_float_stack;

          is_float = (asminstr->as.cmp.asm_type.kind == AsmType_FLOAT ||
                      asminstr->as.cmp.asm_type.kind == AsmType_DOUBLE);

          is_both_stack = (asminstr->as.cmp.lhs.kind == AsmOperand_STACK &&
                           asminstr->as.cmp.rhs.kind == AsmOperand_STACK);

          is_dst_imm = (asminstr->as.cmp.rhs.kind == AsmOperand_IMM);

          is_dst_float_stack =
              (is_float && asminstr->as.cmp.rhs.kind == AsmOperand_STACK);

          if (is_both_stack || is_dst_imm || is_dst_float_stack) {
            enum AsmRegister scratch_reg = is_float ? XMM8 : R10;
            struct AsmOperand scratch_op = {
                .kind = AsmOperand_REG,
                .as.reg = scratch_reg,
                .asm_type = asminstr->as.cmp.asm_type};
            struct AsmInstr i1 = {0}, i2 = {0};

            i1.kind = AsmInstr_MOV;
            i1.as.mov.src = asminstr->as.cmp.rhs;
            i1.as.mov.dst = scratch_op;
            i1.asm_type = asminstr->as.cmp.asm_type;

            i2.kind = AsmInstr_CMP;
            i2.as.cmp.lhs = asminstr->as.cmp.lhs;
            i2.as.cmp.rhs = scratch_op;
            i2.as.cmp.asm_type = asminstr->as.cmp.asm_type;
            i2.asm_type = asminstr->as.cmp.asm_type;

            vec_insert(&instrs, i1);
            vec_insert(&instrs, i2);
          } else {
            vec_insert(&instrs, *asminstr);
          }
          break;
        }
        case AsmInstr_CVT: {
          if (asminstr->as.cvt.dst.kind == AsmOperand_STACK) {
            bool is_float_dst =
                (asminstr->as.cvt.dst.asm_type.kind == AsmType_FLOAT ||
                 asminstr->as.cvt.dst.asm_type.kind == AsmType_DOUBLE);

            struct AsmOperand scratch_op = {
                .kind = AsmOperand_REG,
                .as.reg = is_float_dst ? XMM8 : R10,
                .asm_type = asminstr->as.cvt.dst.asm_type};

            struct AsmInstr i1 = {0}, i2 = {0};

            i1.kind = AsmInstr_CVT;
            i1.as.cvt.kind = asminstr->as.cvt.kind;
            i1.as.cvt.src = asminstr->as.cvt.src;
            i1.as.cvt.dst = scratch_op;
            i1.asm_type = asminstr->asm_type;

            i2.kind = AsmInstr_MOV;
            i2.as.mov.src = scratch_op;
            i2.as.mov.dst = asminstr->as.cvt.dst;
            i2.asm_type = asminstr->as.cvt.dst.asm_type;

            vec_insert(&instrs, i1);
            vec_insert(&instrs, i2);
          } else {
            vec_insert(&instrs, *asminstr);
          }
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

  return prog;
}
