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

static int add_func(void) {
    static int func_count = 0;
    return func_count++;
}

static int add_label(void) {
    static int label_count = 0;
    return label_count++;
}

#define MAX_DATA_COUNT 256
static Str* data[MAX_DATA_COUNT];
static int data_count = 0;

static int add_data(Str* str) {
    for (int i = 0; i < data_count; i++) {
        if (str_eql(*str, *data[i])) {
            return i;
        }
    }
    data[data_count] = str;
    return data_count++;
}

static void _codegen(FILE* out, ASTNode* node, int out_label) {
    switch (node->type) {
        case NODE_STMTS: {
            StatementListNode* stmts = (StatementListNode*)node;
            ASTNodeList* iter = stmts->stmts;
            while (iter) {
                _codegen(out, iter->node, out_label);
                iter = iter->next;
            }
        } break;

        case NODE_INTLIT: {
            IntLitNode* lit = (IntLitNode*)node;
            genf(out, "    movl $%d, %%eax", lit->val);
        } break;

        case NODE_BINARYOP: {
            BinaryOpNode* binop = (BinaryOpNode*)node;
            _codegen(out, binop->left, out_label);

            if (binop->op == TK_LOR) {
                int label = add_label();
                genf(out, "    testl %%eax, %%eax");
                genf(out, "    jnz LAB_%d", label);
                _codegen(out, binop->right, out_label);
                genf(out, "LAB_%d:", label);
            } else if (binop->op == TK_LAND) {
                int label = add_label();
                genf(out, "    testl %%eax, %%eax");
                genf(out, "    jz LAB_%d", label);
                _codegen(out, binop->right, out_label);
                genf(out, "LAB_%d:", label);
            } else {
                genf(out, "    pushl %%eax");

                _codegen(out, binop->right, out_label);
                genf(out, "    movl %%eax, %%ebx");

                genf(out, "    popl %%eax");

                switch (binop->op) {
                    case TK_ADD:
                        genf(out, "    addl %%ebx, %%eax");
                        break;

                    case TK_SUB:
                        genf(out, "    subl %%ebx, %%eax");
                        break;

                    case TK_MUL:
                        genf(out, "    imull %%ebx, %%eax");
                        break;

                    case TK_DIV:
                        genf(out, "    cdq");
                        genf(out, "    idivl %%ebx");
                        break;

                    case TK_MOD:
                        genf(out, "    cdq");
                        genf(out, "    idivl %%ebx");
                        genf(out, "    movl %%edx, %%eax");
                        break;

                    case TK_SHL:
                        genf(out, "    movl %%ebx, %%ecx");
                        genf(out, "    shll %%cl, %%eax");
                        break;

                    case TK_SHR:
                        genf(out, "    movl %%ebx, %%ecx");
                        genf(out, "    sarl %%cl, %%eax");
                        break;

                    case TK_AND:
                        genf(out, "    andl %%ebx, %%eax");
                        break;

                    case TK_XOR:
                        genf(out, "    xorl %%ebx, %%eax");
                        break;

                    case TK_OR:
                        genf(out, "    orl %%ebx, %%eax");
                        break;

                    case TK_EQ:
                        genf(out, "    cmpl %%ebx, %%eax");
                        genf(out, "    sete %%al");
                        genf(out, "    movzbl %%al, %%eax");
                        break;

                    case TK_NE:
                        genf(out, "    cmpl %%ebx, %%eax");
                        genf(out, "    setne %%al");
                        genf(out, "    movzbl %%al, %%eax");
                        break;

                    case TK_LT:
                        genf(out, "    cmpl %%ebx, %%eax");
                        genf(out, "    setl %%al");
                        genf(out, "    movzbl %%al, %%eax");
                        break;

                    case TK_LE:
                        genf(out, "    cmpl %%ebx, %%eax");
                        genf(out, "    setle %%al");
                        genf(out, "    movzbl %%al, %%eax");
                        break;

                    case TK_GT:
                        genf(out, "    cmpl %%ebx, %%eax");
                        genf(out, "    setg %%al");
                        genf(out, "    movzbl %%al, %%eax");
                        break;

                    case TK_GE:
                        genf(out, "    cmpl %%ebx, %%eax");
                        genf(out, "    setge %%al");
                        genf(out, "    movzbl %%al, %%eax");
                        break;

                    default:
                        assert(0);
                }
            }
        } break;

        case NODE_UNARYOP: {
            UnaryOpNode* unaryop = (UnaryOpNode*)node;
            _codegen(out, unaryop->node, out_label);
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
        } break;

        case NODE_VAR: {
            VarNode* var = (VarNode*)node;
            if (var->ste->is_global) {
                genf(out, "    movl VAR_%d, %%eax", var->ste->offset / 4);
            } else {
                genf(out, "    movl %d(%%ebp), %%eax", var->ste->offset);
            }
        } break;

        case NODE_ASSIGN: {
            AssignNode* assign = (AssignNode*)node;
            if (assign->op != TK_ASSIGN) {
                _codegen(out, assign->right, out_label);
                genf(out, "    movl %%eax, %%ebx");

                if (assign->left->ste->is_global) {
                    genf(out, "    movl VAR_%d, %%eax",
                         assign->left->ste->offset / 4);
                } else {
                    genf(out, "    movl %d(%%ebp), %%eax",
                         assign->left->ste->offset);
                }

                switch (assign->op) {
                    case TK_AADD:
                        genf(out, "    addl %%ebx, %%eax");
                        break;

                    case TK_ASUB:
                        genf(out, "    subl %%ebx, %%eax");
                        break;

                    case TK_AMUL:
                        genf(out, "    imull %%ebx, %%eax");
                        break;

                    case TK_ADIV:
                        genf(out, "    cdq");
                        genf(out, "    idivl %%ebx");
                        break;

                    case TK_AMOD:
                        genf(out, "    cdq");
                        genf(out, "    idivl %%ebx");
                        genf(out, "    movl %%edx, %%eax");
                        break;

                    case TK_ASHL:
                        genf(out, "    movl %%ebx, %%ecx");
                        genf(out, "    shll %%cl, %%eax");
                        break;

                    case TK_ASHR:
                        genf(out, "    movl %%ebx, %%ecx");
                        genf(out, "    sarl %%cl, %%eax");
                        break;

                    case TK_AAND:
                        genf(out, "    andl %%ebx, %%eax");
                        break;

                    case TK_AXOR:
                        genf(out, "    xorl %%ebx, %%eax");
                        break;

                    case TK_AOR:
                        genf(out, "    orl %%ebx, %%eax");
                        break;

                    default:
                        assert(0);
                }
            } else {
                _codegen(out, assign->right, out_label);
            }
            if (assign->left->ste->is_global) {
                genf(out, "    movl %%eax, VAR_%d",
                     assign->left->ste->offset / 4);
            } else {
                genf(out, "    movl %%eax, %d(%%ebp)",
                     assign->left->ste->offset);
            }
        } break;

        case NODE_IF: {
            IfStatementNode* if_node = (IfStatementNode*)node;

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

            _codegen(out, if_node->expr, out_label);

            genf(out, "    testl %%eax, %%eax");
            genf(out, "    jz LAB_%d", else_label);

            _codegen(out, if_node->then_block, out_label);

            genf(out, "    jmp LAB_%d", end_label);

            genf(out, "LAB_%d:", else_label);

            if (if_node->else_block) {
                _codegen(out, if_node->else_block, out_label);
            }

            genf(out, "LAB_%d:", end_label);
        } break;

        case NODE_WHILE: {
            WhileNode* while_node = (WhileNode*)node;

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

            _codegen(out, while_node->expr, out_label);

            genf(out, "    testl %%eax, %%eax");
            genf(out, "    jz LAB_%d", end_label);

            _codegen(out, while_node->block, out_label);
            if (while_node->inc) {
                _codegen(out, while_node->inc, out_label);
            }

            genf(out, "    jmp LAB_%d", loop_label);

            genf(out, "LAB_%d:", end_label);
        } break;

        case NODE_CALL: {
            CallNode* call = (CallNode*)node;

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

            int arg_count = 0;
            ASTNodeList* curr = call->args;
            while (curr) {
                _codegen(out, curr->node, out_label);
                genf(out, "    pushl %%eax");
                arg_count++;
                curr = curr->next;
            }
            genf(out, "    call FUN_%d", call->ste->label);
            if (arg_count > 0) {
                genf(out, "    addl $%d, %%esp", arg_count * 4);
            }
        } break;

        case NODE_PRINT: {
            PrintNode* print_node = (PrintNode*)node;

            int arg_count = 0;
            ASTNodeList* curr = print_node->args;
            while (curr) {
                _codegen(out, curr->node, out_label);
                genf(out, "    pushl %%eax");
                arg_count++;
                curr = curr->next;
            }

            genf(out, "    pushl $DAT_%d", add_data(&print_node->fmt));
            arg_count++;

            genf(out, "    call printf");
            genf(out, "    addl $%d, %%esp", arg_count * 4);
        } break;

        case NODE_RET: {
            ReturnNode* ret = (ReturnNode*)node;
            _codegen(out, ret->expr, out_label);
            genf(out, "    jmp LAB_%d", out_label);
        } break;

        default:
            assert(0);
    }
}

void codegen(FILE* out, ASTNode* node, SymbolTable* sym) {
    SymbolTableEntry* curr = sym->ste;

    SymbolTableEntry* ste = sym->ste;
    for (int i = 0; ste; ste = ste->next, i++) {
        if (ste->type == SYM_VAR) {
            if (i == 0) {
                genf(out, ".data");
            }
            genf(out, "VAR_%d:", i);
            genf(out, "    .int 0");
        }
    }

    genf(out, ".text");
    while (curr) {
        if (curr->type == SYM_FUNC) {
            FuncSymbolTableEntry* func = (FuncSymbolTableEntry*)curr;
            int out_label = add_label();

            func->label = add_func();
            genf(out, ".global FUN_%d", func->label);
            genf(out, "FUN_%d:", func->label);

            genf(out, "    pushl %%ebp");
            genf(out, "    movl %%esp, %%ebp");
            genf(out, "    subl $%d, %%esp", *func->sym->stack_size);

            _codegen(out, func->node, out_label);

            genf(out, "LAB_%d:", out_label);
            genf(out, "    leave");
            genf(out, "    ret");
            genf(out, ".type FUN_%d, @function", func->label);
            fprintf(out, "\n");
        }
        curr = curr->next;
    }

    int main_out_label = add_label();

    genf(out, ".global main");
    genf(out, "main:");
    genf(out, "    pushl %%ebp");
    genf(out, "    movl %%esp, %%ebp");

    _codegen(out, node, main_out_label);

    genf(out, "    movl $0, %%eax");
    genf(out, "LAB_%d:", main_out_label);
    genf(out, "    leave");
    genf(out, "    ret");
    genf(out, ".type main, @function");
    fprintf(out, "\n");

    genf(out, ".data");
    for (int i = 0; i < data_count; i++) {
        genf(out, "DAT_%d:", i);
        genf(out, "    .asciz \"%.*s\\n\"", (int)data[i]->len, data[i]->ptr);
    }
}
