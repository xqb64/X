#include "parser.h"
#include "ir.h"
#include "codegen.h"
#include "util.h"

#include <assert.h>
#include <stdio.h>

#if defined(DEBUG_PARSER) && defined(DEBUG_ENABLE_DUMPS)
void print_type(Type *type, int spaces)
{
  switch (type->kind) {
    case STRUCT_T:
      printf("%s", type->as.struct_name);
      break;
    case VOID_T:
      printf("void");
      break;
    case PTR_T:
      printf("*");
      print_type(type->as.base, spaces);
      break;
    case U8_T:
      printf("u8");
      break;
    case U16_T:
      printf("u16");
      break;
    case U32_T:
      printf("u32");
      break;
    case U64_T:
      printf("u64");
      break;
    case I8_T:
      printf("i8");
      break;
    case I16_T:
      printf("i16");
      break;
    case I32_T:
      printf("i32");
      break;
    case I64_T:
      printf("i64");
      break;
    case F32_T:
      printf("f32");
      break;
    case F64_T:
      printf("f64");
      break;
    case BOOL_T:
      printf("bool");
      break;
    case STR_T:
      printf("str");
      break;
    case FN_T: {
      printf("fn(\n");

      print_indent(spaces + 2);
      printf("args: [\n");
      for (int i = 0; i < type->as.func.params.len; i++) {
        print_indent(spaces + 4);
        print_type(&type->as.func.params.data[i], spaces + 4);
        printf(",\n");
      }

      if (type->as.func.is_variadic) {
        print_indent(spaces + 4);
        printf("...\n");
      }

      print_indent(spaces + 2);
      printf("],\n");

      print_indent(spaces + 2);
      printf("retval: ");
      if (type->as.func.retval) {
        print_type(type->as.func.retval, spaces + 2);
      } else {
        printf("void");
      }
      printf("\n");

      print_indent(spaces);
      printf(")");
      break;
    }
    case UNKNOWN_T:
      printf("unknown");
      break;
    default:
      assert(0);
  }
}

static void print_binary_op(enum ExprBinKind kind)
{
  switch (kind) {
    case EXPR_BIN_ADD:
      printf("ADD");
      break;
    case EXPR_BIN_SUB:
      printf("SUB");
      break;
    case EXPR_BIN_MUL:
      printf("MUL");
      break;
    case EXPR_BIN_DIV:
      printf("DIV");
      break;
    case EXPR_BIN_LESS:
      printf("LESS");
      break;
    case EXPR_BIN_GREATER:
      printf("GREATER");
      break;
    case EXPR_BIN_LESS_EQUAL:
      printf("LESS EQUAL");
      break;
    case EXPR_BIN_GREATER_EQUAL:
      printf("GREATER EQUAL");
      break;
    case EXPR_BIN_EQUAL_EQUAL:
      printf("EQUAL EQUAL");
      break;
    case EXPR_BIN_BANG_EQUAL:
      printf("BANG EQUAL");
      break;
    case EXPR_BIN_BITWISE_AND:
      printf("BITWISE AND");
      break;
    case EXPR_BIN_BITWISE_XOR:
      printf("BITWISE XOR");
      break;
    case EXPR_BIN_BITWISE_OR:
      printf("BITWISE OR");
      break;
    case EXPR_BIN_SHIFT_LEFT:
      printf("SHIFT LEFT");
      break;
    case EXPR_BIN_SHIFT_RIGHT:
      printf("SHIFT RIGHT");
      break;
    case EXPR_BIN_LOGICAL_AND:
      printf("LOGICAL AND");
      break;
    case EXPR_BIN_LOGICAL_OR:
      printf("LOGICAL OR");
      break;
    default:
      assert(0);
  }
}

void print_expr(struct Expr *expr, int spaces)
{
  switch (expr->kind) {
    case EXPR_LITERAL: {
      if (expr->as.literal.kind == LITERAL_BOOL) {
        printf("Literal(\n");
        print_indent(spaces + 2);
        printf("v: %s,\n", expr->as.literal.as.boolean ? "true" : "false");
        printf("\n");
        print_indent(spaces);
        printf(")");
      } else if (expr->as.literal.kind == LITERAL_NUM) {
        printf("Literal(\n");
        print_indent(spaces + 2);

        switch (expr->as.literal.type.kind) {
          case I8_T: {
            printf("v: %d,\n", expr->as.literal.as.i8);
            break;
          }
          case U8_T: {
            printf("v: %d,\n", expr->as.literal.as.u8);
            break;
          }
          case I16_T: {
            printf("v: %d,\n", expr->as.literal.as.i16);
            break;
          }
          case U16_T: {
            printf("v: %d,\n", expr->as.literal.as.u16);
            break;
          }
          case I32_T: {
            printf("v: %d,\n", expr->as.literal.as.i32);
            break;
          }
          case U32_T: {
            printf("v: %d,\n", expr->as.literal.as.u32);
            break;
          }
          case I64_T: {
            printf("v: %lld,\n", expr->as.literal.as.i64);
            break;
          }
          case U64_T: {
            printf("v: %llu,\n", expr->as.literal.as.u64);
            break;
          }
          case F32_T: {
            printf("v: %f,\n", expr->as.literal.as.f32);
            break;
          }
          case F64_T: {
            printf("v: %f,\n", expr->as.literal.as.f64);
            break;
          }
          default:
            assert(0);
        }
        print_indent(spaces + 2);
        printf("type: ");
        print_type(&expr->as.literal.type, spaces);
        printf("\n");
        print_indent(spaces);
        printf(")");
      } else {
        printf("Literal(\"%s\")", expr->as.literal.as.str);
      }
      break;
    }
    case EXPR_VARIABLE: {
      printf("Variable(%s)", expr->as.var.name);
      break;
    }
    case EXPR_UNARY: {
      printf("Unary(\n");

      print_indent(spaces + 2);
      printf("expr = ");
      print_expr(expr->as.unary.expr, spaces + 4);
      printf(",\n");

      print_indent(spaces);
      printf(")");

      break;
    }
    case EXPR_BINARY: {
      printf("Binary(\n");

      print_indent(spaces + 2);
      printf("lhs = ");
      print_expr(expr->as.binary.lhs, spaces + 4);
      printf(",\n");

      print_indent(spaces + 2);
      printf("rhs = ");
      print_expr(expr->as.binary.rhs, spaces + 4);
      printf(",\n");

      print_indent(spaces + 2);
      printf("kind = ");
      print_binary_op(expr->as.binary.kind);
      printf(",\n");

      print_indent(spaces + 2);
      printf("type = ");
      print_type(&expr->type, spaces);
      printf(",\n");

      print_indent(spaces);
      printf(")");
      break;
    }
    case EXPR_ASSIGN: {
      printf("Assign(\n");

      print_indent(spaces + 2);
      printf("lhs = ");
      print_expr(expr->as.assign.lhs, spaces + 4);
      printf(",\n");

      print_indent(spaces + 2);
      printf("rhs = ");
      print_expr(expr->as.assign.rhs, spaces + 4);
      printf(",\n");

      print_indent(spaces);
      printf(")");
      break;
    }
    case EXPR_COMPOUND_ASSIGN: {
      printf("CompoundAssign(\n");
      print_indent(spaces + 2);
      printf("op = ");
      print_binary_op(expr->as.compound_assign.kind);
      printf(",\n");
      print_indent(spaces + 2);
      printf("lhs = ");
      print_expr(expr->as.compound_assign.lhs, spaces + 4);
      printf(",\n");
      print_indent(spaces + 2);
      printf("rhs = ");
      print_expr(expr->as.compound_assign.rhs, spaces + 4);
      printf(",\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case EXPR_CALL: {
      printf("Call(\n");

      print_indent(spaces + 2);
      printf("target = ");
      print_expr(expr->as.call.target, spaces + 2);
      printf(",\n");

      print_indent(spaces + 2);
      printf("arguments: [\n");

      for (int i = 0; i < expr->as.call.arguments.len; i++) {
        print_indent(spaces + 4);
        print_expr(&expr->as.call.arguments.data[i], spaces + 4);
        printf(",\n");
      }

      print_indent(spaces + 2);
      printf("]\n");

      print_indent(spaces);
      printf(")");
      break;
    }
    case EXPR_SIZEOF: {
      printf("SizeOf(\n");
      print_indent(spaces + 2);
      print_expr(expr->as.sizeof_expr.expr, spaces + 2);
      printf("\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case EXPR_ADDROF: {
      printf("AddrOf(\n");
      print_indent(spaces + 2);
      print_expr(expr->as.addrof.expr, spaces + 2);
      printf("\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case EXPR_DEREF: {
      printf("Deref(\n");
      print_indent(spaces + 2);
      print_expr(expr->as.deref.expr, spaces + 2);
      printf("\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case EXPR_CAST: {
      printf("Cast(\n");
      print_indent(spaces + 2);
      print_expr(expr->as.cast.expr, spaces + 2);
      printf("\n");
      print_indent(spaces + 2);
      printf("target_type: ");
      print_type(&expr->as.cast.target_type, spaces + 2);
      printf("\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case EXPR_STRUCT_INIT: {
      printf("StructInit(\n");
      print_indent(spaces + 2);
      printf("name = %s,\n", expr->as.struct_init.struct_name);
      print_indent(spaces + 2);
      printf("values = [\n");
      for (int i = 0; i < expr->as.struct_init.values.len; i++) {
        print_indent(spaces + 4);
        print_expr(expr->as.struct_init.values.data[i].expr, spaces + 4);
        printf(",\n");
      }
      print_indent(spaces + 2);
      printf("]\n");
      print_indent(spaces);
      printf(")");
      break;
    }
    case EXPR_MEMBER: {
      printf("Member(\n");
      print_indent(spaces + 2);
      printf("target = ");
      print_expr(expr->as.member.target, spaces + 4);
      printf(",\n");
      print_indent(spaces + 2);
      printf("field = %s,\n", expr->as.member.field_name);
      print_indent(spaces + 2);
      printf("is_arrow = %s\n", expr->as.member.is_arrow ? "true" : "false");
      print_indent(spaces);
      printf(")");
      break;
    }
    default:
      assert(0);
  }
}

void print_stmt(struct Stmt *stmt, int spaces)
{
  switch (stmt->kind) {
    case STMT_FN: {
      print_indent(spaces);
      printf("STMT_FN(\n");

      print_indent(spaces + 2);
      printf("name = %s,\n", stmt->as.fn.name);

      print_indent(spaces + 2);
      printf("params = [\n");
      for (int i = 0; i < stmt->as.fn.params.len; i++) {
        print_indent(spaces + 4);
        printf("%s: ", stmt->as.fn.params.data[i].name);
        print_type(&stmt->as.fn.params.data[i].type, 0);
        printf(",\n");
      }
      print_indent(spaces + 2);
      printf("],\n");

      print_indent(spaces + 2);
      printf("body = [\n");
      for (int i = 0; i < stmt->as.fn.body.len; i++) {
        print_stmt(&stmt->as.fn.body.data[i], spaces + 4);
      }
      print_indent(spaces + 2);
      printf("],\n");

      print_indent(spaces + 2);
      printf("retval = ");
      print_type(&stmt->as.fn.retval, spaces + 4);
      printf("\n");

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_LET: {
      print_indent(spaces);
      printf("STMT_LET(\n");

      print_indent(spaces + 2);
      printf("name = %s,\n", stmt->as.let.name);

      print_indent(spaces + 2);
      printf("type = ");
      print_type(&stmt->as.let.type, spaces + 4);
      printf(",\n");

      print_indent(spaces + 2);
      printf("init = ");
      print_expr(stmt->as.let.init, spaces + 2);
      printf("\n");

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_RET: {
      print_indent(spaces);
      printf("STMT_RET(\n");

      print_indent(spaces + 2);
      printf("val = ");
      if (stmt->as.ret.val) {
        print_expr(stmt->as.ret.val, spaces + 2);
      } else {
        printf("NULL");
      }
      printf("\n");

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_IF: {
      print_indent(spaces);
      printf("STMT_IF(\n");

      print_indent(spaces + 2);
      printf("cond = ");
      print_expr(&stmt->as.if_stmt.cond, spaces + 2);
      printf(",\n");

      print_indent(spaces + 2);
      printf("then = \n");
      print_stmt(stmt->as.if_stmt.then_block, spaces + 2);

      if (stmt->as.if_stmt.else_block) {
        print_indent(spaces + 2);
        printf("else = \n");
        print_stmt(stmt->as.if_stmt.else_block, spaces + 4);
      }

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_DO_WHILE: {
      print_indent(spaces);
      printf("STMT_DO_WHILE(\n");

      print_indent(spaces + 2);
      printf("label = %s,\n", stmt->as.do_while_stmt.label
                                  ? stmt->as.do_while_stmt.label
                                  : "NULL");

      print_indent(spaces + 2);
      printf("cond = ");
      print_expr(&stmt->as.do_while_stmt.cond, spaces + 2);
      printf(",\n");

      print_indent(spaces + 2);
      printf("body = \n");
      print_stmt(stmt->as.do_while_stmt.body, spaces + 2);

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_WHILE: {
      print_indent(spaces);
      printf("STMT_WHILE(\n");

      print_indent(spaces + 2);
      printf("label = %s,\n",
             stmt->as.while_stmt.label ? stmt->as.while_stmt.label : "NULL");

      print_indent(spaces + 2);
      printf("cond = ");
      print_expr(&stmt->as.while_stmt.cond, spaces + 2);
      printf(",\n");

      print_indent(spaces + 2);
      printf("body = \n");
      print_stmt(stmt->as.while_stmt.body, spaces + 2);

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_LOOP: {
      print_indent(spaces);
      printf("STMT_LOOP(\n");

      print_indent(spaces + 2);
      printf("body = \n");
      print_stmt(stmt->as.loop.body, spaces + 2);

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_BREAK: {
      print_indent(spaces);
      printf("STMT_BREAK(\n");

      print_indent(spaces + 2);
      printf("label = %s\n",
             stmt->as.break_stmt.label ? stmt->as.break_stmt.label : "NULL");

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_CONTINUE: {
      print_indent(spaces);
      printf("STMT_CONTINUE(\n");

      print_indent(spaces + 2);
      printf("label = %s\n", stmt->as.continue_stmt.label
                                 ? stmt->as.continue_stmt.label
                                 : "NULL");

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_BLOCK: {
      print_indent(spaces);
      printf("STMT_BLOCK(\n");

      print_indent(spaces + 2);
      printf("body = [\n");
      for (int i = 0; i < stmt->as.block.stmts.len; i++) {
        print_stmt(&stmt->as.block.stmts.data[i], spaces + 4);
      }

      print_indent(spaces + 2);
      printf("]\n");

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_EXTERN: {
      print_indent(spaces);
      printf("STMT_EXTERN(\n");

      print_indent(spaces + 2);
      printf("name = %s,\n", stmt->as.extern_stmt.name);

      print_indent(spaces + 2);
      printf("params = [\n");
      for (int i = 0; i < stmt->as.extern_stmt.params.len; i++) {
        print_indent(spaces + 4);
        printf("%s: ", stmt->as.extern_stmt.params.data[i].name);
        print_type(&stmt->as.extern_stmt.params.data[i].type, 0);
        printf(",\n");
      }
      if (stmt->as.extern_stmt.is_variadic) {
        print_indent(spaces + 4);
        printf("...\n");
      }
      print_indent(spaces + 2);
      printf("],\n");

      print_indent(spaces + 2);
      printf("retval = ");
      print_type(&stmt->as.extern_stmt.retval, spaces + 4);
      printf("\n");

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_EXPR: {
      print_indent(spaces);
      printf("STMT_EXPR(\n");

      print_indent(spaces + 2);
      printf("expr = ");
      print_expr(&stmt->as.expr_stmt.expr, spaces + 2);
      printf("\n");

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_STRUCT: {
      print_indent(spaces);
      printf("%s(\n",
             stmt->as.struct_stmt.is_union ? "STMT_UNION" : "STMT_STRUCT");

      print_indent(spaces + 2);
      printf("name = %s,\n", stmt->as.struct_stmt.name);

      print_indent(spaces + 2);
      printf("fields = [\n");

      for (int i = 0; i < stmt->as.struct_stmt.fields.len; i++) {
        print_indent(spaces + 4);
        printf("Field(name: %s, type: ",
               stmt->as.struct_stmt.fields.data[i].name);

        print_type(&stmt->as.struct_stmt.fields.data[i].type, spaces + 6);

        printf(", offset: %d),\n", stmt->as.struct_stmt.fields.data[i].offset);
      }

      print_indent(spaces + 2);
      printf("]\n");

      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_ENUM: {
      print_indent(spaces);
      printf("STMT_ENUM(\n");
      print_indent(spaces + 2);
      printf("name = %s,\n", stmt->as.enum_stmt.name);
      print_indent(spaces + 2);
      printf("variants = [\n");
      for (int i = 0; i < stmt->as.enum_stmt.variants.len; i++) {
        print_indent(spaces + 4);
        printf("%s = %d,\n", stmt->as.enum_stmt.variants.data[i].name,
               stmt->as.enum_stmt.variants.data[i].value);
      }
      print_indent(spaces + 2);
      printf("]\n");
      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_LABELED: {
      print_indent(spaces);
      printf("STMT_LABELED(\n");
      print_stmt(stmt->as.labeled.stmt, spaces + 2);
      print_indent(spaces);
      printf(")\n");
      break;
    }
    case STMT_GOTO: {
      print_indent(spaces);
      printf("STMT_GOTO(\n");
      print_indent(spaces + 2);
      printf("label = \"%s\",\n", stmt->as.goto_stmt.label);
      print_indent(spaces);
      printf(")\n");
      break;
    }
    default:
      assert(0 && "Unhandled statement kind in print_stmt");
  }
}

void print_ast(struct AST *ast)
{
  for (int i = 0; i < ast->stmts.len; i++) {
    print_stmt(&ast->stmts.data[i], 0);
  }
}
#endif 

#if defined(DEBUG_TOKENIZER) && defined(DEBUG_ENABLE_DUMPS)
void print_token(struct Token *token)
{
  switch (token->kind) {
    case TOKEN_FN:
      printf("fn");
      break;
    case TOKEN_LET:
      printf("let");
      break;
    case TOKEN_MUT:
      printf("mut");
      break;
    case TOKEN_AS:
      printf("as");
      break;
    case TOKEN_IF:
      printf("if");
      break;
    case TOKEN_ELSE:
      printf("else");
      break;
    case TOKEN_DO:
      printf("do");
      break;
    case TOKEN_WHILE:
      printf("while");
      break;
    case TOKEN_LOOP:
      printf("loop");
      break;
    case TOKEN_BREAK:
      printf("break");
      break;
    case TOKEN_CONTINUE:
      printf("continue");
      break;
    case TOKEN_GOTO:
      printf("goto");
      break;
    case TOKEN_RET:
      printf("ret");
      break;
    case TOKEN_EXTERN:
      printf("extern");
      break;
    case TOKEN_VOID:
      printf("void");
      break;
    case TOKEN_SIZEOF:
      printf("sizeof");
      break;
    case TOKEN_STRUCT:
      printf("struct");
      break;
    case TOKEN_UNION:
      printf("union");
      break;
    case TOKEN_ENUM:
      printf("enum");
      break;
    case TOKEN_BOOL:
      printf("bool");
      break;
    case TOKEN_TRUE:
      printf("true");
      break;
    case TOKEN_FALSE:
      printf("false");
      break;
    case TOKEN_I8:
      printf("i8");
      break;
    case TOKEN_I16:
      printf("i16");
      break;
    case TOKEN_I32:
      printf("i32");
      break;
    case TOKEN_I64:
      printf("i64");
      break;
    case TOKEN_U8:
      printf("u8");
      break;
    case TOKEN_U16:
      printf("u16");
      break;
    case TOKEN_U32:
      printf("u32");
      break;
    case TOKEN_U64:
      printf("u64");
      break;
    case TOKEN_F32:
      printf("f32");
      break;
    case TOKEN_F64:
      printf("f64");
      break;
    case TOKEN_STR:
      printf("str");
      break;
    case TOKEN_LPAREN:
      printf("LParen");
      break;
    case TOKEN_RPAREN:
      printf("RParen");
      break;
    case TOKEN_LBRACE:
      printf("LBrace");
      break;
    case TOKEN_RBRACE:
      printf("RBrace");
      break;
    case TOKEN_PLUS:
      printf("plus");
      break;
    case TOKEN_MINUS:
      printf("minus");
      break;
    case TOKEN_STAR:
      printf("star");
      break;
    case TOKEN_SLASH:
      printf("slash");
      break;
    case TOKEN_COMMA:
      printf("comma");
      break;
    case TOKEN_COLON:
      printf("colon");
      break;
    case TOKEN_SEMICOLON:
      printf("semicolon");
      break;
    case TOKEN_LESS:
      printf("less");
      break;
    case TOKEN_GREATER:
      printf("greater");
      break;
    case TOKEN_EQUAL:
      printf("equal");
      break;
    case TOKEN_BANG:
      printf("bang");
      break;
    case TOKEN_PIPE:
      printf("pipe");
      break;
    case TOKEN_AMPERSAND:
      printf("ampersand");
      break;
    case TOKEN_CARET:
      printf("caret");
      break;
    case TOKEN_TILDE:
      printf("tilde");
      break;
    case TOKEN_DOT:
      printf("dot");
      break;
    case TOKEN_LESS_EQUAL:
      printf("less equal");
      break;
    case TOKEN_GREATER_EQUAL:
      printf("greater equal");
      break;
    case TOKEN_EQUAL_EQUAL:
      printf("equal equal");
      break;
    case TOKEN_BANG_EQUAL:
      printf("bang equal");
      break;
    case TOKEN_PIPE_PIPE:
      printf("pipe pipe");
      break;
    case TOKEN_AMPERSAND_AMPERSAND:
      printf("ampersand ampersand");
      break;
    case TOKEN_ARROW:
      printf("arrow");
      break;
    case TOKEN_PLUS_EQUAL:
      printf("plus equal");
      break;
    case TOKEN_MINUS_EQUAL:
      printf("minus equal");
      break;
    case TOKEN_STAR_EQUAL:
      printf("star equal");
      break;
    case TOKEN_SLASH_EQUAL:
      printf("slash equal");
      break;
    case TOKEN_AMPERSAND_EQUAL:
      printf("ampersand equal");
      break;
    case TOKEN_PIPE_EQUAL:
      printf("pipe equal");
      break;
    case TOKEN_CARET_EQUAL:
      printf("caret equal");
      break;
    case TOKEN_LESS_LESS:
      printf("less less");
      break;
    case TOKEN_GREATER_GREATER:
      printf("greater greater");
      break;
    case TOKEN_LESS_LESS_EQUAL:
      printf("less less equal");
      break;
    case TOKEN_GREATER_GREATER_EQUAL:
      printf("greater greater equal");
      break;
    case TOKEN_ELLIPSIS:
      printf("ellipsis");
      break;
    case TOKEN_IDENTIFIER:
      printf("ident(%.*s)", token->len, token->start);
      break;
    case TOKEN_NUMBER:
    case TOKEN_FP_NUMBER:
      printf("%.*s", token->len, token->start);
      break;
    case TOKEN_STRING:
      printf("string(\"%.*s\")", token->len, token->start);
      break;
    case TOKEN_ERROR:
      printf("ERROR");
      break;
    case TOKEN_EOF:
      printf("EOF");
      break;
    default:
      assert(0 && "unhandled TokenKind variant");
  }

  printf("\n");
}

void print_tokens(VecToken *tokens)
{
  for (int i = 0; i < tokens->len; i++) {
    print_token(&tokens->data[i]);
  }
}
#endif

#if defined(DEBUG_ENABLE_DUMPS) && (defined (DEBUG_IR) || defined(DEBUG_IR_OPT))
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

#if defined (DEBUG_ENABLE_DUMPS) && (defined (DEBUG_CODEGEN_RAW) || defined (DEBUG_CODEGEN_REGALLOC) || defined (DEBUG_CODEGEN_FIXUP))
static void print_asm_type(struct AsmType type)
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
    case AsmType_FLOAT:
      printf("AsmType_FLOAT");
      break;
    case AsmType_DOUBLE:
      printf("AsmType_DOUBLE");
      break;
    default:
      assert(0);
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

static void print_condition_code(enum ConditionCode cc)
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

static void print_asm_binary_op(enum AsmInstrBinaryKind kind)
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

static void print_asm_instr(struct AsmInstr *instr, int spaces)
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

static void print_asm_fn(struct AsmFunction *fn)
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

void print_asm(struct AsmProgram *prog)
{
  for (int i = 0; i < prog->funcs.len; i++) {
    print_asm_fn(&prog->funcs.data[i]);
  }
}
#endif
