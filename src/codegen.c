#include "codegen.h"

#include <assert.h>

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

static void emit_node(FILE* out, ASTNode* node);

static void emit_stmts(FILE* out, StatementListNode* stmts) {
    ASTNodeList* iter = stmts->stmts;
    while (iter) {
        emit_node(out, iter->node);
        iter = iter->next;
    }
}

static void emit_intlit(FILE* out, IntLitNode* lit) {
    genf(out, "    movl $%d, %%eax", lit->val);
}

static void emit_strlit(FILE* out, StrLitNode* lit) {
    genf(out, "    movl $DAT_%d, %%eax", add_data(lit->val));
}

static void emit_binop(FILE* out, BinaryOpNode* binop) {
    emit_node(out, binop->left);

    if (binop->op == TK_LOR) {
        int label = add_label();
        genf(out, "    testl %%eax, %%eax");
        genf(out, "    jnz LAB_%d", label);
        emit_node(out, binop->right);
        genf(out, "LAB_%d:", label);
    } else if (binop->op == TK_LAND) {
        int label = add_label();
        genf(out, "    testl %%eax, %%eax");
        genf(out, "    jz LAB_%d", label);
        emit_node(out, binop->right);
        genf(out, "LAB_%d:", label);
    } else {
        genf(out, "    pushl %%eax");

        emit_node(out, binop->right);
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
}

static void emit_unaryop(FILE* out, UnaryOpNode* unaryop) {
    emit_node(out, unaryop->node);
    switch (unaryop->op) {
        case TK_ADD:
            break;

        case TK_SUB:
            genf(out, "    negl %%eax");
            break;

        case TK_NOT:
            genf(out, "    notl %%eax");
            break;

        case TK_LNOT:
            genf(out, "    testl %%eax, %%eax");
            genf(out, "    sete %%al");
            genf(out, "    movzbl %%al, %%eax");
            break;

        default:
            assert(0);
    }
}

static void emit_var(FILE* out, VarNode* var) {
    if (var->ste->is_global) {
        genf(out, "    movl VAR_%.*s, %%eax", var->ste->ident.len,
             var->ste->ident.ptr);
    } else {
        genf(out, "    movl %d(%%ebp), %%eax", var->ste->offset);
    }
}

static void emit_assign(FILE* out, AssignNode* assign) {
    // TODO: Error handling
    assert(assign->left->type == NODE_VAR);

    VarNode* lvalue = (VarNode*)assign->left;

    if (assign->op != TK_ASSIGN) {
        emit_node(out, assign->right);
        genf(out, "    movl %%eax, %%ecx");

        if (lvalue->ste->is_global) {
            genf(out, "    movl VAR_%.*s, %%eax", lvalue->ste->ident.len,
                 lvalue->ste->ident.ptr);
        } else {
            genf(out, "    movl %d(%%ebp), %%eax", lvalue->ste->offset);
        }

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
        emit_node(out, assign->right);
    }

    if (lvalue->ste->is_global) {
        genf(out, "    movl %%eax, VAR_%.*s", lvalue->ste->ident.len,
             lvalue->ste->ident.ptr);
    } else {
        genf(out, "    movl %%eax, %d(%%ebp)", lvalue->ste->offset);
    }
}

static void emit_if(FILE* out, IfStatementNode* if_node) {
    /*
     *      <cond>
     *      JZ else_label
     *      <then_block>
     *      JMP end_label
     *  else_label:
     *      <else_block>
     *  end_label:
     */

    int end_label = add_label();
    int else_label = add_label();

    emit_node(out, if_node->expr);

    genf(out, "    testl %%eax, %%eax");
    genf(out, "    jz LAB_%d", else_label);

    emit_node(out, if_node->then_block);

    genf(out, "    jmp LAB_%d", end_label);

    genf(out, "LAB_%d:", else_label);

    if (if_node->else_block) {
        emit_node(out, if_node->else_block);
    }

    genf(out, "LAB_%d:", end_label);
}

static void emit_while(FILE* out, WhileNode* while_node) {
    /*
     *  loop_label:
     *      <cond>
     *      JZ end_label
     *      <block>
     *      <inc>
     *      JMP loop_label
     *  end_lable:
     */

    int loop_label = add_label();
    int end_label = add_label();

    genf(out, "LAB_%d:", loop_label);

    emit_node(out, while_node->expr);

    genf(out, "    testl %%eax, %%eax");
    genf(out, "    jz LAB_%d", end_label);

    emit_node(out, while_node->block);
    if (while_node->inc) {
        emit_node(out, while_node->inc);
    }

    genf(out, "    jmp LAB_%d", loop_label);

    genf(out, "LAB_%d:", end_label);
}

static void emit_call(FILE* out, CallNode* call) {
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

    ASTNodeList* curr = call->args;
    int arg_size = 0;
    while (curr) {
        emit_node(out, curr->node);
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
}

static void emit_print(FILE* out, PrintNode* print_node) {
    int arg_count = 0;
    ASTNodeList* curr = print_node->args;
    while (curr) {
        emit_node(out, curr->node);
        genf(out, "    pushl %%eax");
        arg_count++;
        curr = curr->next;
    }

    genf(out, "    pushl $DAT_%d", add_data(print_node->fmt));
    arg_count++;

    genf(out, "    call printf");
    genf(out, "    addl $%d, %%esp", arg_count * 4);
}

static void emit_ret(FILE* out, ReturnNode* ret) {
    if (ret->expr) {
        emit_node(out, ret->expr);
    }
    genf(out, "    jmp LAB_%d", out_label);
}

static void emit_node(FILE* out, ASTNode* node) {
    switch (node->type) {
        case NODE_STMTS:
            emit_stmts(out, (StatementListNode*)node);
            break;

        case NODE_INTLIT:
            emit_intlit(out, (IntLitNode*)node);
            break;

        case NODE_STRLIT:
            emit_strlit(out, (StrLitNode*)node);
            break;

        case NODE_BINARYOP:
            emit_binop(out, (BinaryOpNode*)node);
            break;

        case NODE_UNARYOP:
            emit_unaryop(out, (UnaryOpNode*)node);
            break;

        case NODE_VAR:
            emit_var(out, (VarNode*)node);
            break;

        case NODE_ASSIGN:
            emit_assign(out, (AssignNode*)node);
            break;

        case NODE_IF:
            emit_if(out, (IfStatementNode*)node);
            break;

        case NODE_WHILE:
            emit_while(out, (WhileNode*)node);
            break;

        case NODE_CALL:
            emit_call(out, (CallNode*)node);
            break;

        case NODE_PRINT:
            emit_print(out, (PrintNode*)node);
            break;

        case NODE_RET:
            emit_ret(out, (ReturnNode*)node);
            break;

        default:
            assert(0);
    }
}

static void emit_func(FILE* out, FuncSymbolTableEntry* func) {
    out_label = add_label();

    genf(out, "FUNC_%.*s:", func->ident.len, func->ident.ptr);

    genf(out, "    pushl %%ebp");
    genf(out, "    movl %%esp, %%ebp");
    genf(out, "    subl $%d, %%esp", *func->sym->stack_size);

    emit_node(out, func->node);

    genf(out, "LAB_%d:", out_label);
    genf(out, "    leave");
    genf(out, "    ret");
}

void codegen(FILE* out, ASTNode* node, SymbolTable* sym) {
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
                emit_func(out, func);
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

    emit_node(out, node);

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
}
