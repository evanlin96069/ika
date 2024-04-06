#include "codegen.h"

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>

#ifdef _DEBUG

#define genf(out, ...)                                  \
    do {                                                \
        fprintf(out, __VA_ARGS__);                      \
        fprintf(out, " # %s:%d\n", __FILE__, __LINE__); \
    } while (0)

#else

#define genf(out, ...)             \
    do {                           \
        fprintf(out, __VA_ARGS__); \
        fprintf(out, "\n");        \
    } while (0)

#endif

static int add_label(void) {
    static int label_count = 0;
    return label_count++;
}

#define MAX_DATA_COUNT 256
static Str data[MAX_DATA_COUNT];
static int data_count = 0;

static int add_data(Str str) {
    for (int i = 0; i < data_count; i++) {
        if (str_eql(str, data[i])) {
            return i;
        }
    }
    data[data_count] = str;
    return data_count++;
}

// out label for current function
static int out_label;

static EmitResult error(int pos, const char* fmt, ...) {
    EmitResult result;
    result.type = RESULT_ERROR;
    result.error = malloc(sizeof(Error));
    result.error->pos = pos;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(result.error->msg, ERROR_MAX_LENGTH, fmt, ap);
    va_end(ap);

    return result;
}

static EmitResult emit_node(FILE* out, ASTNode* node);

static EmitResult emit_stmts(FILE* out, StatementListNode* stmts) {
    EmitResult result;

    ASTNodeList* iter = stmts->stmts;
    while (iter) {
        result = emit_node(out, iter->node);
        if (result.type == RESULT_ERROR) {
            return result;
        }

        iter = iter->next;
    }

    result.type = RESULT_OK;
    return result;
}

static EmitResult emit_intlit(FILE* out, IntLitNode* lit) {
    EmitResult result = {RESULT_OK};
    genf(out, "    movl $%d, %%eax", lit->val);
    result.info.is_lvalue = 0;
    return result;
}

static EmitResult emit_strlit(FILE* out, StrLitNode* lit) {
    EmitResult result = {RESULT_OK};
    genf(out, "    movl $DAT_%d, %%eax", add_data(lit->val));
    result.info.is_lvalue = 0;
    return result;
}

static EmitResult emit_rvalify(FILE* out) {
    EmitResult result = {RESULT_OK};
    genf(out, "    movl (%%eax), %%eax");
    result.info.is_lvalue = 0;
    return result;
}

static EmitResult emit_binop(FILE* out, BinaryOpNode* binop) {
    EmitResult result;

    result = emit_node(out, binop->left);
    if (result.type == RESULT_ERROR) {
        return result;
    }

    if (result.info.is_lvalue) {
        emit_rvalify(out);
    }

    if (binop->op == TK_LOR) {
        int label = add_label();
        genf(out, "    testl %%eax, %%eax");
        genf(out, "    jnz LAB_%d", label);

        result = emit_node(out, binop->right);
        if (result.type == RESULT_ERROR) {
            return result;
        }

        if (result.info.is_lvalue) {
            emit_rvalify(out);
        }

        genf(out, "LAB_%d:", label);
    } else if (binop->op == TK_LAND) {
        int label = add_label();
        genf(out, "    testl %%eax, %%eax");
        genf(out, "    jz LAB_%d", label);

        result = emit_node(out, binop->right);
        if (result.type == RESULT_ERROR) {
            return result;
        }

        if (result.info.is_lvalue) {
            emit_rvalify(out);
        }

        genf(out, "LAB_%d:", label);
    } else {
        genf(out, "    pushl %%eax");

        result = emit_node(out, binop->right);
        if (result.type == RESULT_ERROR) {
            return result;
        }

        if (result.info.is_lvalue) {
            emit_rvalify(out);
        }

        genf(out, "    movl %%eax, %%ecx");
        genf(out, "    popl %%eax");

        switch (binop->op) {
            case TK_ADD:
                genf(out, "    addl %%ecx, %%eax");
                break;

            case TK_SUB:
                genf(out, "    subl %%ecx, %%eax");
                break;

            case TK_MUL:
                genf(out, "    imull %%ecx, %%eax");
                break;

            case TK_DIV:
                genf(out, "    cdq");
                genf(out, "    idivl %%ecx");
                break;

            case TK_MOD:
                genf(out, "    cdq");
                genf(out, "    idivl %%ecx");
                genf(out, "    movl %%edx, %%eax");
                break;

            case TK_SHL:
                genf(out, "    movl %%ecx, %%edx");
                genf(out, "    shll %%cl, %%eax");
                break;

            case TK_SHR:
                genf(out, "    movl %%ecx, %%edx");
                genf(out, "    sarl %%cl, %%eax");
                break;

            case TK_AND:
                genf(out, "    andl %%ecx, %%eax");
                break;

            case TK_XOR:
                genf(out, "    xorl %%ecx, %%eax");
                break;

            case TK_OR:
                genf(out, "    orl %%ecx, %%eax");
                break;

            case TK_EQ:
                genf(out, "    cmpl %%ecx, %%eax");
                genf(out, "    sete %%al");
                genf(out, "    movzbl %%al, %%eax");
                break;

            case TK_NE:
                genf(out, "    cmpl %%ecx, %%eax");
                genf(out, "    setne %%al");
                genf(out, "    movzbl %%al, %%eax");
                break;

            case TK_LT:
                genf(out, "    cmpl %%ecx, %%eax");
                genf(out, "    setl %%al");
                genf(out, "    movzbl %%al, %%eax");
                break;

            case TK_LE:
                genf(out, "    cmpl %%ecx, %%eax");
                genf(out, "    setle %%al");
                genf(out, "    movzbl %%al, %%eax");
                break;

            case TK_GT:
                genf(out, "    cmpl %%ecx, %%eax");
                genf(out, "    setg %%al");
                genf(out, "    movzbl %%al, %%eax");
                break;

            case TK_GE:
                genf(out, "    cmpl %%ecx, %%eax");
                genf(out, "    setge %%al");
                genf(out, "    movzbl %%al, %%eax");
                break;

            default:
                assert(0);
        }
    }

    result.type = RESULT_OK;
    result.info.is_lvalue = 0;
    return result;
}

static EmitResult emit_unaryop(FILE* out, UnaryOpNode* unaryop) {
    EmitResult result;

    result = emit_node(out, unaryop->node);
    if (result.type == RESULT_ERROR) {
        return result;
    }

    switch (unaryop->op) {
        case TK_ADD:
            if (result.info.is_lvalue) {
                emit_rvalify(out);
            }

            result.info.is_lvalue = 0;
            break;

        case TK_SUB:
            if (result.info.is_lvalue) {
                emit_rvalify(out);
            }

            genf(out, "    negl %%eax");

            result.info.is_lvalue = 0;
            break;

        case TK_NOT:
            if (result.info.is_lvalue) {
                emit_rvalify(out);
            }

            genf(out, "    notl %%eax");

            result.info.is_lvalue = 0;
            break;

        case TK_LNOT:
            if (result.info.is_lvalue) {
                emit_rvalify(out);
            }

            genf(out, "    testl %%eax, %%eax");
            genf(out, "    sete %%al");
            genf(out, "    movzbl %%al, %%eax");

            result.info.is_lvalue = 0;
            break;

        case TK_MUL:
            if (result.info.is_lvalue) {
                emit_rvalify(out);
            }
            result.info.is_lvalue = 1;
            break;

        case TK_AND:
            if (result.info.is_lvalue == 0) {
                return error(unaryop->pos,
                             "lvalue required as unary '&' operand");
            }
            result.info.is_lvalue = 0;
            break;

        default:
            assert(0);
    }

    result.type = RESULT_OK;
    return result;
}

static EmitResult emit_var(FILE* out, VarNode* var) {
    EmitResult result;
    if (var->ste->is_global) {
        genf(out, "    movl $VAR_%.*s, %%eax", var->ste->ident.len,
             var->ste->ident.ptr);
    } else {
        genf(out, "    leal %d(%%ebp), %%eax", var->ste->offset);
    }

    result.type = RESULT_OK;
    result.info.is_lvalue = 1;
    return result;
}

static EmitResult emit_assign(FILE* out, AssignNode* assign) {
    EmitResult result;

    result = emit_node(out, assign->left);
    if (result.type == RESULT_ERROR) {
        return result;
    }

    if (result.info.is_lvalue == 0) {
        return error(assign->pos,
                     "lvalue required as left operand of assignment");
    }

    genf(out, "    pushl %%eax");

    if (assign->op != TK_ASSIGN) {
        emit_rvalify(out);
        genf(out, "    pushl %%eax");

        result = emit_node(out, assign->right);
        if (result.type == RESULT_ERROR) {
            return result;
        }

        if (result.info.is_lvalue) {
            emit_rvalify(out);
        }

        genf(out, "    movl %%eax, %%ecx");
        genf(out, "    popl %%eax");

        switch (assign->op) {
            case TK_AADD:
                genf(out, "    addl %%ecx, %%eax");
                break;

            case TK_ASUB:
                genf(out, "    subl %%ecx, %%eax");
                break;

            case TK_AMUL:
                genf(out, "    imull %%ecx, %%eax");
                break;

            case TK_ADIV:
                genf(out, "    cdq");
                genf(out, "    idivl %%ecx");
                break;

            case TK_AMOD:
                genf(out, "    cdq");
                genf(out, "    idivl %%ecx");
                genf(out, "    movl %%edx, %%eax");
                break;

            case TK_ASHL:
                genf(out, "    movl %%ecx, %%edx");
                genf(out, "    shll %%cl, %%eax");
                break;

            case TK_ASHR:
                genf(out, "    movl %%ecx, %%edx");
                genf(out, "    sarl %%cl, %%eax");
                break;

            case TK_AAND:
                genf(out, "    andl %%ecx, %%eax");
                break;

            case TK_AXOR:
                genf(out, "    xorl %%ecx, %%eax");
                break;

            case TK_AOR:
                genf(out, "    orl %%ecx, %%eax");
                break;

            default:
                assert(0);
        }
    } else {
        result = emit_node(out, assign->right);
        if (result.type == RESULT_ERROR) {
            return result;
        }

        if (result.info.is_lvalue) {
            emit_rvalify(out);
        }
    }

    genf(out, "    movl %%eax, %%ecx");
    genf(out, "    popl %%eax");

    genf(out, "    movl %%ecx, (%%eax)");
    genf(out, "    movl %%ecx, %%eax");

    result.type = RESULT_OK;
    result.info.is_lvalue = 0;
    return result;
}

static EmitResult emit_if(FILE* out, IfStatementNode* if_node) {
    /*
     *      <cond>
     *      JZ else_label
     *      <then_block>
     *      JMP end_label
     *  else_label:
     *      <else_block>
     *  end_label:
     */

    EmitResult result;

    int end_label = add_label();
    int else_label = add_label();

    result = emit_node(out, if_node->expr);
    if (result.type == RESULT_ERROR) {
        return result;
    }

    if (result.info.is_lvalue) {
        emit_rvalify(out);
    }

    genf(out, "    testl %%eax, %%eax");
    genf(out, "    jz LAB_%d", else_label);

    result = emit_node(out, if_node->then_block);
    if (result.type == RESULT_ERROR) {
        return result;
    }

    genf(out, "    jmp LAB_%d", end_label);

    genf(out, "LAB_%d:", else_label);

    if (if_node->else_block) {
        result = emit_node(out, if_node->else_block);
        if (result.type == RESULT_ERROR) {
            return result;
        }
    }

    genf(out, "LAB_%d:", end_label);

    result.type = RESULT_OK;
    return result;
}

static EmitResult emit_while(FILE* out, WhileNode* while_node) {
    /*
     *  loop_label:
     *      <cond>
     *      JZ end_label
     *      <block>
     *      <inc>
     *      JMP loop_label
     *  end_lable:
     */

    EmitResult result;

    int loop_label = add_label();
    int end_label = add_label();

    genf(out, "LAB_%d:", loop_label);

    result = emit_node(out, while_node->expr);
    if (result.type == RESULT_ERROR) {
        return result;
    }

    if (result.info.is_lvalue) {
        emit_rvalify(out);
    }

    genf(out, "    testl %%eax, %%eax");
    genf(out, "    jz LAB_%d", end_label);

    result = emit_node(out, while_node->block);
    if (result.type == RESULT_ERROR) {
        return result;
    }

    if (while_node->inc) {
        result = emit_node(out, while_node->inc);
        if (result.type == RESULT_ERROR) {
            return result;
        }
    }

    genf(out, "    jmp LAB_%d", loop_label);

    genf(out, "LAB_%d:", end_label);

    result.type = RESULT_OK;
    return result;
}

static EmitResult emit_call(FILE* out, CallNode* call) {
    /*
     *  local 3         [ebp]-16 <-ESP
     *  local 2         [ebp]-8
     *  local 1         [ebp]-4
     *  saved EBP       <-EBP
     *  return addr
     *  arg 1           [ebp]+8
     *  arg 2           [ebp]+12
     *  arg 3           [ebp]+16
     */

    EmitResult result;

    ASTNodeList* curr = call->args;
    int arg_size = 0;
    while (curr) {
        result = emit_node(out, curr->node);
        if (result.type == RESULT_ERROR) {
            return result;
        }

        if (result.info.is_lvalue) {
            emit_rvalify(out);
        }

        genf(out, "    pushl %%eax");
        arg_size += 4;
        curr = curr->next;
    }

    if (call->ste->node) {
        genf(out, "    call FUNC_%.*s", call->ste->ident.len,
             call->ste->ident.ptr);
    } else {
        // extern
        genf(out, "    call %.*s", call->ste->ident.len, call->ste->ident.ptr);
    }

    if (call->ste->sym->arg_size) {
        genf(out, "    addl $%d, %%esp", arg_size);
    }

    result.type = RESULT_OK;
    result.info.is_lvalue = 0;
    return result;
}

static EmitResult emit_print(FILE* out, PrintNode* print_node) {
    EmitResult result;

    int arg_count = 0;
    ASTNodeList* curr = print_node->args;
    while (curr) {
        result = emit_node(out, curr->node);
        if (result.type == RESULT_ERROR) {
            return result;
        }

        if (result.info.is_lvalue) {
            emit_rvalify(out);
        }

        genf(out, "    pushl %%eax");
        arg_count++;
        curr = curr->next;
    }

    genf(out, "    pushl $DAT_%d", add_data(print_node->fmt));
    arg_count++;

    genf(out, "    call printf");
    genf(out, "    addl $%d, %%esp", arg_count * 4);

    result.type = RESULT_OK;
    result.info.is_lvalue = 0;
    return result;
}

static EmitResult emit_ret(FILE* out, ReturnNode* ret) {
    EmitResult result;

    if (ret->expr) {
        result = emit_node(out, ret->expr);
        if (result.type == RESULT_ERROR) {
            return result;
        }

        if (result.info.is_lvalue) {
            emit_rvalify(out);
        }
    }
    genf(out, "    jmp LAB_%d", out_label);

    result.type = RESULT_OK;
    return result;
}

static EmitResult emit_node(FILE* out, ASTNode* node) {
    EmitResult result;

    switch (node->type) {
        case NODE_STMTS:
            result = emit_stmts(out, (StatementListNode*)node);
            break;

        case NODE_INTLIT:
            result = emit_intlit(out, (IntLitNode*)node);
            break;

        case NODE_STRLIT:
            result = emit_strlit(out, (StrLitNode*)node);
            break;

        case NODE_BINARYOP:
            result = emit_binop(out, (BinaryOpNode*)node);
            break;

        case NODE_UNARYOP:
            result = emit_unaryop(out, (UnaryOpNode*)node);
            break;

        case NODE_VAR:
            result = emit_var(out, (VarNode*)node);
            break;

        case NODE_ASSIGN:
            result = emit_assign(out, (AssignNode*)node);
            break;

        case NODE_IF:
            result = emit_if(out, (IfStatementNode*)node);
            break;

        case NODE_WHILE:
            result = emit_while(out, (WhileNode*)node);
            break;

        case NODE_CALL:
            result = emit_call(out, (CallNode*)node);
            break;

        case NODE_PRINT:
            result = emit_print(out, (PrintNode*)node);
            break;

        case NODE_RET:
            result = emit_ret(out, (ReturnNode*)node);
            break;

        default:
            assert(0);
    }

    return result;
}

static EmitResult emit_func(FILE* out, FuncSymbolTableEntry* func) {
    EmitResult result;

    out_label = add_label();

    genf(out, "FUNC_%.*s:", func->ident.len, func->ident.ptr);

    genf(out, "    pushl %%ebp");
    genf(out, "    movl %%esp, %%ebp");
    genf(out, "    subl $%d, %%esp", *func->sym->stack_size);

    result = emit_node(out, func->node);
    if (result.type == RESULT_ERROR) {
        return result;
    }

    genf(out, "LAB_%d:", out_label);
    genf(out, "    leave");
    genf(out, "    ret");

    result.type = RESULT_OK;
    return result;
}

Error* codegen(FILE* out, ASTNode* node, SymbolTable* sym) {
    EmitResult result;

    SymbolTableEntry* curr = sym->ste;

    // Global variables
    SymbolTableEntry* ste = sym->ste;
    genf(out, ".data");
    while (ste) {
        if (ste->type == SYM_VAR) {
            genf(out, "VAR_%.*s:", ste->ident.len, ste->ident.ptr);
            genf(out, "    .zero 4");
        }
        ste = ste->next;
    }

    // Functions
    genf(out, ".text");
    while (curr) {
        if (curr->type == SYM_FUNC) {
            FuncSymbolTableEntry* func = (FuncSymbolTableEntry*)curr;
            if (func->node) {
                result = emit_func(out, func);
                if (result.type == RESULT_ERROR) {
                    return result.error;
                }
                fprintf(out, "\n");
            }
        }
        curr = curr->next;
    }

    // main
    out_label = add_label();

    genf(out, ".global main");
    genf(out, "main:");
    genf(out, "    pushl %%ebp");
    genf(out, "    movl %%esp, %%ebp");

    result = emit_node(out, node);
    if (result.type == RESULT_ERROR) {
        return result.error;
    }

    genf(out, "    movl $0, %%eax");
    genf(out, "LAB_%d:", out_label);
    genf(out, "    leave");
    genf(out, "    ret");
    genf(out, ".type main, @function");
    fprintf(out, "\n");

    // Strings
    genf(out, ".data");
    for (int i = 0; i < data_count; i++) {
        genf(out, "DAT_%d:", i);
        genf(out, "    .asciz \"%.*s\"", data[i].len, data[i].ptr);
    }

    return NULL;
}
