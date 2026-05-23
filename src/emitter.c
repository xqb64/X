#include "emitter.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "codegen.h"

static void emit_operand(FILE *f, struct AsmOperand *op)
{
  switch (op->kind) {
    case AsmOperand_INDEXED: {
      fprintf(f, "(%s, %s, %d)", reg_to_str_64(op->as.indexed.base),
              reg_to_str_64(op->as.indexed.index), op->as.indexed.scale);
      break;
    }
    case AsmOperand_MEMORY: {
      fprintf(f, "%d(", op->as.mem.offset);
      switch (op->as.mem.base) {
        case AX:
          fprintf(f, "%%rax");
          break;
        case BX:
          fprintf(f, "%%rbx");
          break;
        case DI:
          fprintf(f, "%%rdi");
          break;
        case SI:
          fprintf(f, "%%rsi");
          break;
        case DX:
          fprintf(f, "%%rdx");
          break;
        case CX:
          fprintf(f, "%%rcx");
          break;
        case R8:
          fprintf(f, "%%r8");
          break;
        case R9:
          fprintf(f, "%%r9");
          break;
        case R10:
          fprintf(f, "%%r10");
          break;
        case R11:
          fprintf(f, "%%r11");
          break;
        case R12:
          fprintf(f, "%%r12");
          break;
        case R13:
          fprintf(f, "%%r13");
          break;
        case R14:
          fprintf(f, "%%r14");
          break;
        case R15:
          fprintf(f, "%%r15");
          break;
        case BP:
          fprintf(f, "%%rbp");
          break;
        case SP:
          fprintf(f, "%%rsp");
          break;
        default:
          assert(0 && "Invalid memory base register");
      }
      fprintf(f, ")");
      break;
    }
    case AsmOperand_DATA: {
      fprintf(f, "%s(%%rip)", op->as.data);
      break;
    }
    case AsmOperand_IMM: {
      fprintf(f, "$%lld", op->as.imm);
      break;
    }
    case AsmOperand_PSEUDO: {
      assert(0 && "not implemented");
      break;
    }
    case AsmOperand_REG: {
      switch (op->as.reg) {
        case XMM0: {
          fprintf(f, "%%xmm0");
          break;
        }
        case XMM1: {
          fprintf(f, "%%xmm1");
          break;
        }
        case XMM2: {
          fprintf(f, "%%xmm2");
          break;
        }
        case XMM3: {
          fprintf(f, "%%xmm3");
          break;
        }
        case XMM4: {
          fprintf(f, "%%xmm4");
          break;
        }
        case XMM5: {
          fprintf(f, "%%xmm5");
          break;
        }
        case XMM6: {
          fprintf(f, "%%xmm6");
          break;
        }
        case XMM7: {
          fprintf(f, "%%xmm7");
          break;
        }
        case XMM8: {
          fprintf(f, "%%xmm8");
          break;
        }
        case XMM9: {
          fprintf(f, "%%xmm9");
          break;
        }
        case XMM10: {
          fprintf(f, "%%xmm10");
          break;
        }
        case XMM11: {
          fprintf(f, "%%xmm11");
          break;
        }
        case XMM12: {
          fprintf(f, "%%xmm12");
          break;
        }
        case XMM13: {
          fprintf(f, "%%xmm13");
          break;
        }
        case XMM14: {
          fprintf(f, "%%xmm14");
          break;
        }
        case XMM15: {
          fprintf(f, "%%xmm15");
          break;
        }

        case AX: {
          switch (op->asm_type.kind) {
            case AsmType_BYTE: {
              fprintf(f, "%%al");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%ax");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%eax");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%rax");
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
              fprintf(f, "%%bl");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%bx");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%ebx");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%rbx");
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
              fprintf(f, "%%dil");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%di");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%edi");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%rdi");
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
              fprintf(f, "%%sil");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%si");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%esi");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%rsi");
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
              fprintf(f, "%%dl");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%dx");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%edx");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%rdx");
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
              fprintf(f, "%%cl");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%cx");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%ecx");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%rcx");
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
              fprintf(f, "%%r8b");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%r8w");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%r8d");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%r8");
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
              fprintf(f, "%%r9b");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%r9w");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%r9d");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%r9");
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
              fprintf(f, "%%r10b");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%r10w");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%r10d");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%r10");
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
              fprintf(f, "%%r11b");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%r11w");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%r11d");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%r11");
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
              fprintf(f, "%%r12b");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%r12w");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%r12d");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%r12");
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
              fprintf(f, "%%r13b");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%r13w");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%r13d");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%r13");
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
              fprintf(f, "%%r14b");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%r14w");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%r14d");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%r14");
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
              fprintf(f, "%%r15b");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%r15w");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%r15d");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%r15");
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
              fprintf(f, "%%bpl");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%bp");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%ebp");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%rbp");
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
              fprintf(f, "%%spl");
              break;
            }
            case AsmType_WORD: {
              fprintf(f, "%%sp");
              break;
            }
            case AsmType_LONGWORD: {
              fprintf(f, "%%esp");
              break;
            }
            case AsmType_QUADWORD: {
              fprintf(f, "%%rsp");
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
      break;
    }
    case AsmOperand_STACK: {
      fprintf(f, "%d(%%rbp)", op->as.stack_offset);
      break;
    }
    default:
      assert(0);
  }
}

void emit(struct AsmProgram *prog, char *path)
{
  FILE *f;

  f = fopen(path, "w");
  if (global_constants.len > 0) {
    fprintf(f, ".section .rodata\n");
    for (int i = 0; i < global_constants.len; i++) {
      fprintf(f, "%s:\n", global_constants.data[i].name);
      if (strncmp(global_constants.data[i].value, ".long", 5) == 0 ||
          strncmp(global_constants.data[i].value, ".quad", 5) == 0) {
        fprintf(f, "\t%s\n", global_constants.data[i].value);
      } else {
        fprintf(f, "\t.string \"%s\"\n", global_constants.data[i].value);
      }
    }
    fprintf(f, "\n");
  }

  fprintf(f, ".section .text\n");
  for (int i = 0; i < prog->funcs.len; i++) {
    fprintf(f, ".global %s\n", prog->funcs.data[i].name);
    fprintf(f, "%s:\n", prog->funcs.data[i].name);
    for (int j = 0; j < prog->funcs.data[i].body.len; j++) {
      struct AsmInstr *instr = &prog->funcs.data[i].body.data[j];
      switch (instr->kind) {
        case AsmInstr_LBL:
          break;
        default:
          fprintf(f, "\t");
      }
      switch (instr->kind) {
        case AsmInstr_PUSH: {
          fprintf(f, "pushq ");
          emit_operand(f, &instr->as.push.op);
          fprintf(f, "\n");
          break;
        }
        case AsmInstr_POP: {
          fprintf(f, "popq ");
          emit_operand(f, &instr->as.pop.op);
          fprintf(f, "\n");
          break;
        }
        case AsmInstr_MOV: {
          if (instr->asm_type.kind == AsmType_FLOAT) {
            fprintf(f, "movss ");
          } else if (instr->asm_type.kind == AsmType_DOUBLE) {
            fprintf(f, "movsd ");
          } else {
            fprintf(f, "mov");
            switch (instr->asm_type.kind) {
              case AsmType_BYTE:
                fprintf(f, "b ");
                break;
              case AsmType_WORD:
                fprintf(f, "w ");
                break;
              case AsmType_LONGWORD:
                fprintf(f, "l ");
                break;
              case AsmType_QUADWORD:
                fprintf(f, "q ");
                break;
              default:
                assert(0);
            }
          }

          emit_operand(f, &instr->as.mov.src);
          fprintf(f, ", ");
          emit_operand(f, &instr->as.mov.dst);
          fprintf(f, "\n");
          break;
        }
        case AsmInstr_BIN: {
          switch (instr->as.binary.kind) {
            case AsmInstrBinary_ADD: {
              if (instr->asm_type.kind == AsmType_FLOAT) {
                fprintf(f, "addss ");
              } else if (instr->asm_type.kind == AsmType_DOUBLE) {
                fprintf(f, "addsd ");
              } else {
                fprintf(f, "add");
                switch (instr->asm_type.kind) {
                  case AsmType_BYTE:
                    fprintf(f, "b ");
                    break;
                  case AsmType_WORD:
                    fprintf(f, "w ");
                    break;
                  case AsmType_LONGWORD:
                    fprintf(f, "l ");
                    break;
                  case AsmType_QUADWORD:
                    fprintf(f, "q ");
                    break;
                  default:
                    assert(0);
                }
              }
              break;
            }
            case AsmInstrBinary_SUB: {
              if (instr->asm_type.kind == AsmType_FLOAT) {
                fprintf(f, "subss ");
              } else if (instr->asm_type.kind == AsmType_DOUBLE) {
                fprintf(f, "subsd ");
              } else {
                fprintf(f, "sub");
                switch (instr->asm_type.kind) {
                  case AsmType_BYTE:
                    fprintf(f, "b ");
                    break;
                  case AsmType_WORD:
                    fprintf(f, "w ");
                    break;
                  case AsmType_LONGWORD:
                    fprintf(f, "l ");
                    break;
                  case AsmType_QUADWORD:
                    fprintf(f, "q ");
                    break;
                  default:
                    assert(0);
                }
              }
              break;
            }
            case AsmInstrBinary_MUL: {
              if (instr->asm_type.kind == AsmType_FLOAT) {
                fprintf(f, "mulss ");
              } else if (instr->asm_type.kind == AsmType_DOUBLE) {
                fprintf(f, "mulsd ");
              } else {
                fprintf(f, "imul ");
                switch (instr->asm_type.kind) {
                  case AsmType_BYTE:
                    fprintf(f, "b ");
                    break;
                  case AsmType_WORD:
                    fprintf(f, "w ");
                    break;
                  case AsmType_LONGWORD:
                    fprintf(f, "l ");
                    break;
                  case AsmType_QUADWORD:
                    fprintf(f, "q ");
                    break;
                  default:
                    assert(0);
                }
              }
              break;
            }
            case AsmInstrBinary_DIV: {
              if (instr->asm_type.kind == AsmType_FLOAT) {
                fprintf(f, "divss ");
              } else if (instr->asm_type.kind == AsmType_DOUBLE) {
                fprintf(f, "divsd ");
              } else {
                assert(0 && "integer div not implemented");
              }
              break;
            }
            case AsmInstrBinary_BIT_AND:
            case AsmInstrBinary_BIT_XOR:
            case AsmInstrBinary_BIT_OR:
            case AsmInstrBinary_SHL:
            case AsmInstrBinary_SHR:
            case AsmInstrBinary_SAR: {
              switch (instr->as.binary.kind) {
                case AsmInstrBinary_BIT_AND:
                  fprintf(f, "and");
                  break;
                case AsmInstrBinary_BIT_XOR:
                  fprintf(f, "xor");
                  break;
                case AsmInstrBinary_BIT_OR:
                  fprintf(f, "or");
                  break;
                case AsmInstrBinary_SHL:
                  fprintf(f, "sal");
                  break;
                case AsmInstrBinary_SHR:
                  fprintf(f, "shr");
                  break;
                case AsmInstrBinary_SAR:
                  fprintf(f, "sar");
                  break;
                default:
                  assert(0);
              }
              switch (instr->asm_type.kind) {
                case AsmType_BYTE:
                  fprintf(f, "b ");
                  break;
                case AsmType_WORD:
                  fprintf(f, "w ");
                  break;
                case AsmType_LONGWORD:
                  fprintf(f, "l ");
                  break;
                case AsmType_QUADWORD:
                  fprintf(f, "q ");
                  break;
                default:
                  assert(0 && "bitwise operators require integer asm types");
              }
              break;
            }
            default:
              assert(0 && "not implemented");
              break;
          }
          emit_operand(f, &instr->as.binary.lhs);
          fprintf(f, ", ");
          emit_operand(f, &instr->as.binary.rhs);
          fprintf(f, "\n");
          break;
        }
        case AsmInstr_RET: {
          fprintf(f, "ret\n");
          break;
        }
        case AsmInstr_CALL: {
          fprintf(f, "call %s\n", instr->as.call.target);
          break;
        }
        case AsmInstr_JMP: {
          fprintf(f, "jmp .L%s\n", instr->as.jmp.target);
          break;
        }
        case AsmInstr_LBL: {
          fprintf(f, ".L%s:\n", instr->as.lbl.name);
          break;
        }
        case AsmInstr_CMP: {
          char *full_instr;

          switch (instr->as.cmp.asm_type.kind) {
            case AsmType_BYTE:
              full_instr = "cmpb";
              break;
            case AsmType_WORD:
              full_instr = "cmpw";
              break;
            case AsmType_LONGWORD:
              full_instr = "cmpl";
              break;
            case AsmType_QUADWORD:
              full_instr = "cmpq";
              break;
            case AsmType_FLOAT:
              full_instr = "ucomiss";
              break;
            case AsmType_DOUBLE:
              full_instr = "ucomisd";
              break;
            default:
              assert(0);
          }

          fprintf(f, "%s ", full_instr);
          emit_operand(f, &instr->as.cmp.lhs);
          fprintf(f, ", ");
          emit_operand(f, &instr->as.cmp.rhs);
          fprintf(f, "\n");

          break;
        }
        case AsmInstr_JmpCC: {
          char *suffix;

          switch (instr->as.jmpcc.cc) {
            case CC_E:
              suffix = "e";
              break;
            case CC_NE:
              suffix = "ne";
              break;
            case CC_L:
              suffix = "l";
              break;
            case CC_LE:
              suffix = "le";
              break;
            case CC_G:
              suffix = "g";
              break;
            case CC_GE:
              suffix = "ge";
              break;
            case CC_A:
              suffix = "a";
              break;
            case CC_AE:
              suffix = "ae";
              break;
            case CC_B:
              suffix = "b";
              break;
            case CC_BE:
              suffix = "be";
              break;
            default:
              assert(0);
          }

          fprintf(f, "j%s .L%s\n", suffix, instr->as.jmpcc.target);
          break;
        }
        case AsmInstr_SetCC: {
          char *suffix;

          switch (instr->as.setcc.cc) {
            case CC_E:
              suffix = "e";
              break;
            case CC_NE:
              suffix = "ne";
              break;
            case CC_L:
              suffix = "l";
              break;
            case CC_LE:
              suffix = "le";
              break;
            case CC_G:
              suffix = "g";
              break;
            case CC_GE:
              suffix = "ge";
              break;
            case CC_A:
              suffix = "a";
              break;
            case CC_AE:
              suffix = "ae";
              break;
            case CC_B:
              suffix = "b";
              break;
            case CC_BE:
              suffix = "be";
              break;
            default:
              assert(0);
          }

          fprintf(f, "set%s ", suffix);
          emit_operand(f, &instr->as.setcc.op);
          fprintf(f, "\n");
          break;
        }
        case AsmInstr_LEA: {
          fprintf(f, "leaq ");
          emit_operand(f, &instr->as.lea.src);
          fprintf(f, ", ");
          emit_operand(f, &instr->as.lea.dst);
          fprintf(f, "\n");
          break;
        }
        case AsmInstr_UNARY: {
          fprintf(
              f, instr->as.unary.kind == AsmInstrUnary_BIT_NOT ? "not" : "neg");
          switch (instr->asm_type.kind) {
            case AsmType_BYTE:
              fprintf(f, "b");
              break;
            case AsmType_WORD:
              fprintf(f, "w");
              break;
            case AsmType_LONGWORD:
              fprintf(f, "l");
              break;
            case AsmType_QUADWORD:
              fprintf(f, "q");
              break;
            default:
              assert(0);
          }
          fprintf(f, " ");
          emit_operand(f, &instr->as.unary.op);
          fprintf(f, "\n");
          break;
        }
        case AsmInstr_CVT: {
          switch (instr->as.cvt.kind) {
            case AsmCast_SignExtend:
              if (instr->as.cvt.src.asm_type.kind == AsmType_BYTE) {
                if (instr->as.cvt.dst.asm_type.kind == AsmType_WORD) {
                  fprintf(f, "movsbw ");
                } else if (instr->as.cvt.dst.asm_type.kind ==
                           AsmType_LONGWORD) {
                  fprintf(f, "movsbl ");
                } else {
                  fprintf(f, "movsbq ");
                }
              } else if (instr->as.cvt.src.asm_type.kind == AsmType_WORD) {
                if (instr->as.cvt.dst.asm_type.kind == AsmType_LONGWORD) {
                  fprintf(f, "movswl ");
                } else {
                  fprintf(f, "movswq ");
                }
              } else if (instr->as.cvt.src.asm_type.kind == AsmType_LONGWORD) {
                fprintf(f, "movslq ");
              }
              emit_operand(f, &instr->as.cvt.src);
              fprintf(f, ", ");
              emit_operand(f, &instr->as.cvt.dst);
              fprintf(f, "\n");
              break;

            case AsmCast_ZeroExtend:
              if (instr->as.cvt.src.asm_type.kind == AsmType_BYTE) {
                if (instr->as.cvt.dst.asm_type.kind == AsmType_WORD) {
                  fprintf(f, "movzbw ");
                } else if (instr->as.cvt.dst.asm_type.kind ==
                           AsmType_LONGWORD) {
                  fprintf(f, "movzbl ");
                } else {
                  fprintf(f, "movzbq ");
                }
                emit_operand(f, &instr->as.cvt.src);
                fprintf(f, ", ");
                emit_operand(f, &instr->as.cvt.dst);
                fprintf(f, "\n");
              } else if (instr->as.cvt.src.asm_type.kind == AsmType_WORD) {
                if (instr->as.cvt.dst.asm_type.kind == AsmType_LONGWORD) {
                  fprintf(f, "movzwl ");
                } else {
                  fprintf(f, "movzwq ");
                }
                emit_operand(f, &instr->as.cvt.src);
                fprintf(f, ", ");
                emit_operand(f, &instr->as.cvt.dst);
                fprintf(f, "\n");
              } else if (instr->as.cvt.src.asm_type.kind == AsmType_LONGWORD) {
                /* Hardware automatically zero extends the upper 32 bits on
                 * 32-bit register writes */
                fprintf(f, "movl ");
                emit_operand(f, &instr->as.cvt.src);
                fprintf(f, ", ");
                struct AsmOperand narrowed_dst = instr->as.cvt.dst;
                narrowed_dst.asm_type =
                    (struct AsmType){.kind = AsmType_LONGWORD};
                emit_operand(f, &narrowed_dst);
                fprintf(f, "\n");
              }
              break;

            case AsmCast_FloatPromote:
              fprintf(f, "cvtss2sd ");
              emit_operand(f, &instr->as.cvt.src);
              fprintf(f, ", ");
              emit_operand(f, &instr->as.cvt.dst);
              fprintf(f, "\n");
              break;

            case AsmCast_FloatDemote:
              fprintf(f, "cvtsd2ss ");
              emit_operand(f, &instr->as.cvt.src);
              fprintf(f, ", ");
              emit_operand(f, &instr->as.cvt.dst);
              fprintf(f, "\n");
              break;

            case AsmCast_IntToFloat:
              if (instr->as.cvt.src.asm_type.kind == AsmType_QUADWORD) {
                fprintf(f, "cvtsi2ssq ");
              } else {
                fprintf(f, "cvtsi2ssl ");
              }
              emit_operand(f, &instr->as.cvt.src);
              fprintf(f, ", ");
              emit_operand(f, &instr->as.cvt.dst);
              fprintf(f, "\n");
              break;

            case AsmCast_IntToDouble:
              if (instr->as.cvt.src.asm_type.kind == AsmType_QUADWORD) {
                fprintf(f, "cvtsi2sdq ");
              } else {
                fprintf(f, "cvtsi2sdl ");
              }
              emit_operand(f, &instr->as.cvt.src);
              fprintf(f, ", ");
              emit_operand(f, &instr->as.cvt.dst);
              fprintf(f, "\n");
              break;

            case AsmCast_FloatToInt:
            case AsmCast_DoubleToInt:
              if (instr->as.cvt.kind == AsmCast_FloatToInt) {
                if (instr->as.cvt.dst.asm_type.kind == AsmType_QUADWORD) {
                  fprintf(f, "cvttss2siq ");
                } else {
                  fprintf(f, "cvttss2sil ");
                }
              } else {
                if (instr->as.cvt.dst.asm_type.kind == AsmType_QUADWORD) {
                  fprintf(f, "cvttsd2siq ");
                } else {
                  fprintf(f, "cvttsd2sil ");
                }
              }
              emit_operand(f, &instr->as.cvt.src);
              fprintf(f, ", ");
              emit_operand(f, &instr->as.cvt.dst);
              fprintf(f, "\n");
              break;

            default:
              assert(0 && "Unhandled cast type");
          }
          break;
        }
        case AsmInstr_REP_MOVSB: {
          fprintf(f, "rep movsb\n");
          break;
        }
        default:
          break;
      }
    }
  }

  fclose(f);
}
