#include "vm.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEXT_SIZE 1 << 10
#define DATA_SIZE 1 << 10
#define STACK_SIZE 1 << 10

static uint8_t *tp, *dp;
static uint8_t data[DATA_SIZE];
static uint8_t text[TEXT_SIZE];

int pc, sp, bp;
static int ax;
static uint8_t stack[STACK_SIZE];

enum Instruction {
    IMM = 0,
    LI,
    SI,
    JMP,
    JZ,
    JNZ,
    PUSH,
    OR,
    XOR,
    AND,
    EQ,
    NE,
    LT,
    GT,
    LE,
    GE,
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    WRITE,
    PRINT,
    EXIT,
};

static void _codegen(ASTNode* node) {
    if (!node) {
        return;
    }

    switch (node->type) {
        case NODE_STMTS: {
            StatementListNode* stmts = (StatementListNode*)node;
            ASTNodeList* iter = stmts->stmts;
            while (iter) {
                _codegen(iter->node);
                iter = iter->next;
            }
        } break;

        case NODE_INTLIT: {
            IntLitNode* lit = (IntLitNode*)node;
            *tp++ = IMM;
            *((int*)tp) = lit->val;
            tp += sizeof(int);
        } break;

        case NODE_STRLIT: {
            StrLitNode* str_node = (StrLitNode*)node;

            uint8_t* start = dp;
            memcpy(dp, str_node->str.ptr, str_node->str.len);
            dp += str_node->str.len;

            *tp++ = WRITE;
            *((int*)tp) = (int)(start - data);
            tp += sizeof(int);
            *((int*)tp) = (int)str_node->str.len;
            tp += sizeof(int);
        } break;

        case NODE_BINARYOP: {
            BinaryOpNode* binop = (BinaryOpNode*)node;
            _codegen(binop->left);

            if (binop->op == TK_LOR) {
                *tp++ = JNZ;
                int* label = (int*)tp;
                tp += sizeof(int);
                _codegen(binop->right);
                *label = (int)(tp - text);
            }
            if (binop->op == TK_LAND) {
                *tp++ = JZ;
                int* label = (int*)tp;
                tp += sizeof(int);
                _codegen(binop->right);
                *label = (int)(tp - text);
            } else {
                *tp++ = PUSH;
                _codegen(binop->right);

                switch (binop->op) {
                    case TK_MUL:
                        *tp++ = MUL;
                        break;

                    case TK_DIV:
                        *tp++ = DIV;
                        break;

                    case TK_MOD:
                        *tp++ = MOD;
                        break;

                    case TK_ADD:
                        *tp++ = ADD;
                        break;

                    case TK_SUB:
                        *tp++ = SUB;
                        break;

                    case TK_LT:
                        *tp++ = LT;
                        break;

                    case TK_LE:
                        *tp++ = LE;
                        break;

                    case TK_GT:
                        *tp++ = GT;
                        break;

                    case TK_GE:
                        *tp++ = GE;
                        break;

                    case TK_EQ:
                        *tp++ = EQ;
                        break;

                    case TK_NE:
                        *tp++ = NE;
                        break;

                    case TK_AND:
                        *tp++ = AND;
                        break;

                    case TK_XOR:
                        *tp++ = XOR;
                        break;

                    case TK_OR:
                        *tp++ = OR;
                        break;

                    default:
                        assert(0);
                }
            }
        } break;

        case NODE_UNARYOP: {
            UnaryOpNode* unaryop = (UnaryOpNode*)node;
            _codegen(unaryop->node);
            switch (unaryop->op) {
                case TK_ADD:
                    break;

                case TK_SUB:
                    *tp++ = PUSH;
                    *tp++ = IMM;
                    *((int*)tp) = -1;
                    tp += sizeof(int);
                    *tp++ = MUL;
                    break;

                case TK_NOT:
                    *tp++ = PUSH;
                    *tp++ = IMM;
                    *((int*)tp) = -1;
                    tp += sizeof(int);
                    *tp++ = XOR;
                    break;

                case TK_LNOT:
                    *tp++ = PUSH;
                    *tp++ = IMM;
                    *((int*)tp) = 0;
                    tp += sizeof(int);
                    *tp++ = EQ;
                    break;

                default:
                    assert(0);
            }
        } break;

        case NODE_VAR: {
            VarNode* var = (VarNode*)node;
            *tp++ = LI;
            *((int*)tp) = var->ste->offset;
            tp += sizeof(int);
        } break;

        case NODE_ASSIGN: {
            AssignNode* assign = (AssignNode*)node;
            _codegen(assign->right);
            *tp++ = SI;
            *((int*)tp) = assign->left->ste->offset;
            tp += sizeof(int);
        } break;

        case NODE_PRINT: {
            PrintNode* print_node = (PrintNode*)node;
            _codegen(print_node->expr);
            *tp++ = PRINT;
        } break;

        case NODE_IF: {
            IfStatementNode* if_node = (IfStatementNode*)node;

            _codegen(if_node->expr);

            *tp++ = JZ;
            int* else_label = (int*)tp;
            tp += sizeof(int);

            _codegen(if_node->then_block);

            *tp++ = JMP;
            int* end_label = (int*)tp;
            tp += sizeof(int);

            *else_label = (int)(tp - text);

            if (if_node->else_block) {
                _codegen(if_node->else_block);
            }

            *end_label = (int)(tp - text);
        } break;

        case NODE_WHILE: {
            WhileNode* while_node = (WhileNode*)node;

            int loop_label = (int)(tp - text);

            _codegen(while_node->expr);

            *tp++ = JZ;
            int* end_label = (int*)tp;
            tp += sizeof(int);

            _codegen(while_node->block);

            *tp++ = JMP;
            *((int*)tp) = loop_label;
            tp += sizeof(int);

            *end_label = (int)(tp - text);
        } break;

        default:
            assert(0);
    }
}

void codegen(ASTNode* node) {
    tp = text;
    dp = data;
    _codegen(node);
    *tp++ = EXIT;
}

static int print_inst(const uint8_t* s) {
    int i = 0;
    int inst = s[i++];
    switch (inst) {
        case IMM: {
            int val = *((int*)(s + i));
            i += sizeof(int);
            printf("IMM\t%d\n", val);
        } break;

        case LI: {
            int val = *((int*)(s + i));
            i += sizeof(int);
            printf("LI\t%d\n", val);
        } break;

        case SI: {
            int val = *((int*)(s + i));
            i += sizeof(int);
            printf("SI\t%d\n", val);
        } break;

        case JMP: {
            int val = *((int*)(s + i));
            i += sizeof(int);
            printf("JMP\t%d\n", val);
        } break;

        case JZ: {
            int val = *((int*)(s + i));
            i += sizeof(int);
            printf("JZ\t%d\n", val);
        } break;

        case JNZ: {
            int val = *((int*)(s + i));
            i += sizeof(int);
            printf("JNZ\t%d\n", val);
        } break;

        case PUSH:
            printf("PUSH\n");
            break;

        case OR:
            printf("OR\n");
            break;

        case XOR:
            printf("XOR\n");
            break;

        case AND:
            printf("AND\n");
            break;

        case EQ:
            printf("EQ\n");
            break;

        case NE:
            printf("NE\n");
            break;

        case LT:
            printf("LT\n");
            break;

        case GT:
            printf("GT\n");
            break;

        case LE:
            printf("LE\n");
            break;

        case GE:
            printf("GE\n");
            break;

        case ADD:
            printf("ADD\n");
            break;

        case SUB:
            printf("SUB\n");
            break;

        case MUL:
            printf("MUL\n");
            break;

        case DIV:
            printf("DIV\n");
            break;

        case MOD:
            printf("MOD\n");
            break;

        case WRITE: {
            int offset = *((int*)(s + i));
            i += sizeof(int);
            int len = *((int*)(s + i));
            i += sizeof(int);

            printf("WRITE\t%d,\t%d\n", offset, len);
        } break;

        case PRINT:
            printf("PRINT\n");
            break;

        case EXIT:
            printf("EXIT\n");
            break;

        default:
            assert(0);
    }
    return i;
}

void print_code(void) {
    int text_len = tp - text;
    int i = 0;
    while (i < text_len) {
        printf("%d:\t", i);
        i += print_inst(text + i);
    }
}

int vm_run(void) {
    while (1) {
        // printf("%d:\t", pc);
        // print_inst(text + pc);
        int inst = text[pc++];
        switch (inst) {
            case IMM:
                ax = *((int*)(text + pc));
                pc += sizeof(int);
                break;

            case LI: {
                int offset = *((int*)(text + pc));
                pc += sizeof(int);
                ax = stack[bp + offset];
            } break;

            case SI: {
                int offset = *((int*)(text + pc));
                pc += sizeof(int);
                stack[bp + offset] = ax;
            } break;

            case JMP:
                pc = *((int*)(text + pc));
                break;

            case JZ:
                if (ax == 0) {
                    pc = *((int*)(text + pc));
                } else {
                    pc += sizeof(int);
                }
                break;

            case JNZ:
                if (ax != 0) {
                    pc = *((int*)(text + pc));
                } else {
                    pc += sizeof(int);
                }
                break;

            case PUSH:
                *((int*)(stack + sp)) = ax;
                sp += sizeof(int);
                break;

            case OR:
                sp -= sizeof(int);
                ax = (*((int*)(stack + sp))) | ax;
                break;

            case XOR:
                sp -= sizeof(int);
                ax = (*((int*)(stack + sp))) ^ ax;
                break;

            case AND:
                sp -= sizeof(int);
                ax = (*((int*)(stack + sp))) & ax;
                break;

            case EQ:
                sp -= sizeof(int);
                ax = (*((int*)(stack + sp))) == ax;
                break;

            case NE:
                sp -= sizeof(int);
                ax = (*((int*)(stack + sp))) != ax;
                break;

            case LT:
                sp -= sizeof(int);
                ax = (*((int*)(stack + sp))) < ax;
                break;

            case GT:
                sp -= sizeof(int);
                ax = (*((int*)(stack + sp))) > ax;
                break;

            case LE:
                sp -= sizeof(int);
                ax = (*((int*)(stack + sp))) <= ax;
                break;

            case GE:
                sp -= sizeof(int);
                ax = (*((int*)(stack + sp))) >= ax;
                break;

            case ADD:
                sp -= sizeof(int);
                ax = (*((int*)(stack + sp))) + ax;
                break;

            case SUB:
                sp -= sizeof(int);
                ax = (*((int*)(stack + sp))) + ax;
                break;

            case MUL:
                sp -= sizeof(int);
                ax = (*((int*)(stack + sp))) * ax;
                break;

            case DIV:
                sp -= sizeof(int);
                ax = (*((int*)(stack + sp))) / ax;
                break;

            case MOD:
                sp -= sizeof(int);
                ax = (*((int*)(stack + sp))) % ax;
                break;

            case WRITE: {
                int offset = *((int*)(text + pc));
                pc += sizeof(int);
                int len = *((int*)(text + pc));
                pc += sizeof(int);

                printf("%.*s\n", len, (char*)(data + offset));
            } break;

            case PRINT:
                printf("%d\n", ax);
                break;

            case EXIT:
                return 0;

            default:
                assert(0);
        }
    }
}
