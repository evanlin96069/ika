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
    data[data_count] = str;
    return data_count++;
}

static void _codegen(FILE* out, ASTNode* node, int stack_size) {
    switch (node->type) {
        case NODE_STMTS: {
            StatementListNode* stmts = (StatementListNode*)node;
            ASTNodeList* iter = stmts->stmts;
            while (iter) {
                _codegen(out, iter->node, stack_size);
                iter = iter->next;
            }
        } break;

        case NODE_INTLIT: {
            IntLitNode* lit = (IntLitNode*)node;
            genf(out, "    movl $%d, %%eax", lit->val);
        } break;

        case NODE_STRLIT: {
            StrLitNode* str_node = (StrLitNode*)node;

            int index = add_data(&str_node->str);

            genf(out, "    movl $DAT_%d, (%%esp)", index);
            genf(out, "    calll puts");
        } break;

        case NODE_BINARYOP: {
            BinaryOpNode* binop = (BinaryOpNode*)node;
            _codegen(out, binop->left, stack_size);

            if (binop->op == TK_LOR) {
                int label = add_label();
                genf(out, "    testl %%eax, %%eax");
                genf(out, "    jnz LAB_%d", label);
                _codegen(out, binop->right, stack_size);
                genf(out, "LAB_%d:", label);
            } else if (binop->op == TK_LAND) {
                int label = add_label();
                genf(out, "    testl %%eax, %%eax");
                genf(out, "    jz LAB_%d", label);
                _codegen(out, binop->right, stack_size);
                genf(out, "LAB_%d:", label);
            } else {
                // Something might be wrong here, but it works
                genf(out, "    movl %%eax, (%%esp)");
                genf(out, "    subl $4, %%esp");

                _codegen(out, binop->right, stack_size);
                genf(out, "    movl %%eax, %%ebx");

                genf(out, "    addl $4, %%esp");
                genf(out, "    movl (%%esp), %%eax");


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
            _codegen(out, unaryop->node, stack_size);
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
            ;
            if (var->ste->is_global) {
                genf(out, "    movl VAR_%ld, %%eax",
                     var->ste->offset / sizeof(int));
            } else {
                genf(out, "    movl %d(%%ebp), %%eax", var->ste->offset + 8);
            }
        } break;

        case NODE_ASSIGN: {
            AssignNode* assign = (AssignNode*)node;
            if (assign->op != TK_ASSIGN) {
                _codegen(out, assign->right, stack_size);
                genf(out, "    movl %%eax, %%ebx");

                if (assign->left->ste->is_global) {
                    genf(out, "    movl VAR_%ld, %%eax",
                         assign->left->ste->offset / sizeof(int));
                } else {
                    genf(out, "    movl %d(%%ebp), %%eax",
                         assign->left->ste->offset + 8);
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
                _codegen(out, assign->right, stack_size);
            }
            if (assign->left->ste->is_global) {
                genf(out, "    movl %%eax, VAR_%ld",
                     assign->left->ste->offset / sizeof(int));
            } else {
                genf(out, "    movl %%eax, %d(%%ebp)",
                     assign->left->ste->offset + 8);
            }
        } break;

        case NODE_PRINT: {
            PrintNode* print_node = (PrintNode*)node;
            _codegen(out, print_node->expr, stack_size);
            genf(out, "    movl $FMT, (%%esp)");
            genf(out, "    movl %%eax, 4(%%esp)");
            genf(out, "    calll printf");
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

            _codegen(out, if_node->expr, stack_size);

            genf(out, "    testl %%eax, %%eax");
            genf(out, "    jz LAB_%d", else_label);

            _codegen(out, if_node->then_block, stack_size);

            genf(out, "    jmp LAB_%d", end_label);

            genf(out, "LAB_%d:", else_label);

            if (if_node->else_block) {
                _codegen(out, if_node->else_block, stack_size);
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

            _codegen(out, while_node->expr, stack_size);

            genf(out, "    testl %%eax, %%eax");
            genf(out, "    jz LAB_%d", end_label);

            _codegen(out, while_node->block, stack_size);
            if (while_node->inc) {
                _codegen(out, while_node->inc, stack_size);
            }

            genf(out, "    jmp LAB_%d", loop_label);

            genf(out, "LAB_%d:", end_label);
        } break;

        case NODE_CALL: {
            CallNode* call = (CallNode*)node;

            int arg_count = 0;
            ASTNodeList* curr = call->args;
            while (curr) {
                _codegen(out, curr->node, stack_size);
                genf(out, "    movl %%eax, %ld(%%esp)",
                     arg_count * sizeof(int));
                arg_count++;
                curr = curr->next;
            }
            genf(out, "    calll FUN_%d", call->ste->offset);
        } break;

        case NODE_RET: {
            ReturnNode* ret = (ReturnNode*)node;
            _codegen(out, ret->expr, stack_size);
            genf(out, "    addl $%d, %%esp", stack_size);
            genf(out, "    popl %%ebp");
            genf(out, "    retl");
        } break;

        default:
            assert(0);
    }
}

void codegen(FILE* out, ASTNode* node, SymbolTable* sym) {
    SymbolTableEntry* curr = sym->ste;

    int var_count = *sym->stack_size;
    if (var_count > 0) {
        genf(out, ".data");
        for (int i = 0; i < var_count; i++) {
            genf(out, "VAR_%d:", i);
            genf(out, "    .int 0");
        }
    }

    genf(out, ".text");

    while (curr) {
        if (curr->type == SYM_FUNC) {
            FuncSymbolTableEntry* func = (FuncSymbolTableEntry*)curr;
            int stack_size = *func->sym->stack_size + 4;

            func->offset = add_func();
            genf(out, ".global FUN_%d", func->offset);
            genf(out, "FUN_%d:", func->offset);

            genf(out, "    pushl %%ebp");
            genf(out, "    movl %%esp, %%ebp");
            genf(out, "    subl $%d, %%esp", stack_size);

            _codegen(out, func->node, stack_size);
            genf(out, "    addl $%d, %%esp", stack_size);
            genf(out, "    popl %%ebp");
            genf(out, "    retl");
            genf(out, ".type FUN_%d, @function", func->offset);
            fprintf(out, "\n");
        }
        curr = curr->next;
    }

    genf(out, ".global main");
    genf(out, "main:");
    genf(out, "    pushl %%ebp");
    genf(out, "    movl %%esp, %%ebp");
    genf(out, "    subl $4, %%esp");
    _codegen(out, node, 4);
    genf(out, "    addl $4, %%esp");
    genf(out, "    movl $0, %%eax");
    genf(out, "    popl %%ebp");
    genf(out, "    retl");
    genf(out, ".type main, @function");
    fprintf(out, "\n");

    genf(out, ".data");

    for (int i = 0; i < data_count; i++) {
        genf(out, "DAT_%d:", i);
        genf(out, "    .asciz \"%.*s\"", (int)data[i]->len, data[i]->ptr);
    }
    genf(out, "FMT:");
    genf(out, "    .asciz \"%%d\\n\"");
}
