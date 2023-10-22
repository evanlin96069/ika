#include "vm.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define DATA_SIZE 1 << 10
#define TEXT_SIZE 1 << 16
#define STACK_SIZE 1 << 16

static uint8_t *tp, *dp;
static uint8_t data[DATA_SIZE];
static uint8_t text[TEXT_SIZE];

int pc, sp, bp;
static int ax;
static uint8_t stack[STACK_SIZE];

typedef enum RegisterType {
    IMM,  // immediate value, not a register
    AX,
    BP,
    SP,
    PC,
} RegisterType;

enum Instruction {
    MOV,   // MOV dst IMM[val]|REG
    LDR,   // LDR dst IMM[val]|REG[offset]
    STR,   // STR src IMM[val]|REG[offset]
    JMP,   // JMP addr
    JZ,    // JZ addr
    JNZ,   // JNZ addr
    PUSH,  // PUSH IMM[val]|REG
    LEV,
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
    WRITE,  // WRITE DATA LEN
    PRINT,
    EXIT,
};

static void _codegen(ASTNode* node, int in_func) {
    switch (node->type) {
        case NODE_STMTS: {
            StatementListNode* stmts = (StatementListNode*)node;
            ASTNodeList* iter = stmts->stmts;
            while (iter) {
                _codegen(iter->node, in_func);
                iter = iter->next;
            }
        } break;

        case NODE_INTLIT: {
            IntLitNode* lit = (IntLitNode*)node;
            *tp++ = MOV;
            *tp++ = AX;
            *tp++ = IMM;
            *((int*)tp) = lit->val;
            tp += sizeof(int);
        } break;

        case NODE_STRLIT: {
            StrLitNode* str_node = (StrLitNode*)node;

            *tp++ = WRITE;
            *((int*)tp) = (int)(dp - data);
            tp += sizeof(int);
            *((int*)tp) = (int)str_node->str.len;
            tp += sizeof(int);

            memcpy(dp, str_node->str.ptr, str_node->str.len);
            dp += str_node->str.len;
        } break;

        case NODE_BINARYOP: {
            BinaryOpNode* binop = (BinaryOpNode*)node;
            _codegen(binop->left, in_func);

            if (binop->op == TK_LOR) {
                *tp++ = JNZ;
                int* label = (int*)tp;
                tp += sizeof(int);
                _codegen(binop->right, in_func);
                *label = (int)(tp - text);
            } else if (binop->op == TK_LAND) {
                *tp++ = JZ;
                int* label = (int*)tp;
                tp += sizeof(int);
                _codegen(binop->right, in_func);
                *label = (int)(tp - text);
            } else {
                *tp++ = PUSH;
                *tp++ = AX;
                _codegen(binop->right, in_func);

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
            _codegen(unaryop->node, in_func);
            switch (unaryop->op) {
                case TK_ADD:
                    break;

                case TK_SUB:
                    *tp++ = PUSH;
                    *tp++ = AX;
                    *tp++ = MOV;
                    *tp++ = AX;
                    *tp++ = IMM;
                    *((int*)tp) = -1;
                    tp += sizeof(int);
                    *tp++ = MUL;
                    break;

                case TK_NOT:
                    *tp++ = PUSH;
                    *tp++ = AX;
                    *tp++ = MOV;
                    *tp++ = AX;
                    *tp++ = IMM;
                    *((int*)tp) = -1;
                    tp += sizeof(int);
                    *tp++ = XOR;
                    break;

                case TK_LNOT:
                    *tp++ = PUSH;
                    *tp++ = AX;
                    *tp++ = MOV;
                    *tp++ = AX;
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
            *tp++ = LDR;
            *tp++ = AX;
            if (var->ste->is_global) {
                *tp++ = IMM;
            } else {
                *tp++ = BP;
            }
            *((int*)tp) = var->ste->offset;
            tp += sizeof(int);
        } break;

        case NODE_ASSIGN: {
            AssignNode* assign = (AssignNode*)node;
            _codegen(assign->right, in_func);
            *tp++ = STR;
            *tp++ = AX;
            if (assign->left->ste->is_global) {
                *tp++ = IMM;
            } else {
                *tp++ = BP;
            }
            *((int*)tp) = assign->left->ste->offset;
            tp += sizeof(int);
        } break;

        case NODE_PRINT: {
            PrintNode* print_node = (PrintNode*)node;
            _codegen(print_node->expr, in_func);
            *tp++ = PRINT;
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

            _codegen(if_node->expr, in_func);

            *tp++ = JZ;
            int* else_label = (int*)tp;
            tp += sizeof(int);

            _codegen(if_node->then_block, in_func);

            *tp++ = JMP;
            int* end_label = (int*)tp;
            tp += sizeof(int);

            *else_label = (int)(tp - text);
            if (if_node->else_block) {
                _codegen(if_node->else_block, in_func);
            }
            *end_label = (int)(tp - text);
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

            int loop_label = (int)(tp - text);

            _codegen(while_node->expr, in_func);

            *tp++ = JZ;
            int* end_label = (int*)tp;
            tp += sizeof(int);

            _codegen(while_node->block, in_func);
            if (while_node->inc) {
                _codegen(while_node->inc, in_func);
            }

            *tp++ = JMP;
            *((int*)tp) = loop_label;
            tp += sizeof(int);

            *end_label = (int)(tp - text);
        } break;

        case NODE_CALL: {
            CallNode* call = (CallNode*)node;

            /*
             *  |-----------|
             *  |return addr|
             *  |-----------|
             *  |  prev bp  |
             *  |-----------| <-- new bp
             *  |    args   |
             *  |-----------|
             */

            *tp++ = PUSH;
            *tp++ = IMM;
            int* ret_addr = (int*)tp;
            tp += sizeof(int);

            *tp++ = PUSH;
            *tp++ = BP;

            int arg_count = 0;
            ASTNodeList* curr = call->args;
            while (curr) {
                arg_count++;
                _codegen(curr->node, in_func);
                *tp++ = PUSH;
                *tp++ = AX;
                curr = curr->next;
            }

            // TODO: Improve function call
            *tp++ = PUSH;
            *tp++ = SP;

            *tp++ = MOV;
            *tp++ = AX;
            *tp++ = IMM;
            *((int*)tp) = arg_count * 4;
            tp += sizeof(int);

            *tp++ = SUB;

            *tp++ = MOV;
            *tp++ = BP;
            *tp++ = AX;

            *tp++ = JMP;
            *((int*)tp) = call->ste->offset;
            tp += sizeof(int);

            *ret_addr = (int)(tp - text);
        } break;

        case NODE_RET: {
            ReturnNode* ret = (ReturnNode*)node;
            _codegen(ret->expr, in_func);
            if (in_func) {
                *tp++ = LEV;
            } else {
                *tp++ = EXIT;
            }
        } break;

        default:
            assert(0);
    }
}

int codegen(ASTNode* node, SymbolTable* sym) {
    tp = text;
    dp = data;

    SymbolTableEntry* curr = sym->ste;
    while (curr) {
        if (curr->type == SYM_FUNC) {
            FuncSymbolTableEntry* func = (FuncSymbolTableEntry*)curr;
            func->offset = (int)(tp - text);

            *tp++ = PUSH;
            *tp++ = BP;

            *tp++ = MOV;
            *tp++ = AX;
            *tp++ = IMM;
            *((int*)tp) = *func->sym->stack_size;
            tp += sizeof(int);

            *tp++ = ADD;

            *tp++ = MOV;
            *tp++ = SP;
            *tp++ = AX;

            _codegen(func->node, 1);

            *tp++ = MOV;
            *tp++ = AX;
            *tp++ = IMM;
            *((int*)tp) = 0;
            tp += sizeof(int);

            *tp++ = LEV;
        }
        curr = curr->next;
    }

    int entry = (int)(tp - text);

    _codegen(node, 0);
    *tp++ = MOV;
    *tp++ = AX;
    *tp++ = IMM;
    *((int*)tp) = 0;
    tp += sizeof(int);
    *tp++ = EXIT;

    return entry;
}

static inline const char* get_register_name(RegisterType reg) {
    switch (reg) {
        case AX:
            return "AX";
        case BP:
            return "BP";
        case SP:
            return "SP";
        case PC:
            return "PC";
        case IMM:
            return "IMM";
        default:
            assert(0);
    }
}

static int print_inst(const uint8_t* s) {
    int i = 0;
    int inst = s[i++];
    switch (inst) {
        case MOV: {
            int dst = s[i++];
            int reg = s[i++];
            if (reg != IMM) {
                printf("MOV\t%s,\t%s\n", get_register_name(dst),
                       get_register_name(reg));
            } else {
                int val = *((int*)(s + i));
                i += sizeof(int);
                printf("MOV\t%s,\t%d\n", get_register_name(dst), val);
            }
        } break;

        case LDR: {
            int dst = s[i++];
            int reg = s[i++];
            int offset = *((int*)(s + i));
            i += sizeof(int);
            if (reg != IMM) {
                printf("LDR\t%s,\t%s[%d]\n", get_register_name(dst),
                       get_register_name(reg), offset);
            } else {
                printf("LDR\t%s,\t%d\n", get_register_name(dst), offset);
            }
        } break;

        case STR: {
            int src = s[i++];
            int reg = s[i++];
            int offset = *((int*)(s + i));
            i += sizeof(int);
            if (reg != IMM) {
                printf("STR\t%s,\t%s[%d]\n", get_register_name(src),
                       get_register_name(reg), offset);
            } else {
                printf("STR\t%s,\t%d\n", get_register_name(src), offset);
            }
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

        case PUSH: {
            int reg = s[i++];
            if (reg != IMM) {
                printf("PUSH\t%s\n", get_register_name(reg));
            } else {
                int val = *((int*)(s + i));
                i += sizeof(int);
                printf("PUSH\t%d\n", val);
            }
        } break;

        case LEV:
            printf("LEV\n");
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

static inline int* get_register(RegisterType reg) {
    switch (reg) {
        case AX:
            return &ax;
        case BP:
            return &bp;
        case SP:
            return &sp;
        case PC:
            return &pc;
        case IMM:
            return NULL;
        default:
            assert(0);
    }
}

int vm_run(void) {
    while (1) {
#if 0
        printf("bp: %d sp: %d ax: %d\n", bp, sp, ax);
        printf("%d:\t", pc);
        print_inst(text + pc);
#endif
        int inst = text[pc++];
        switch (inst) {
            case MOV: {
                int* dst = get_register(text[pc++]);
                int* src = get_register(text[pc++]);
                if (!src) {  // IMM
                    int val = *((int*)(text + pc));
                    pc += sizeof(int);
                    *dst = val;
                } else {
                    *dst = *src;
                }
            } break;

            case LDR: {
                int* dst = get_register(text[pc++]);
                int* reg = get_register(text[pc++]);
                int base = reg ? *reg : 0;
                int offset = *((int*)(text + pc));
                pc += sizeof(int);
                *dst = *((int*)(stack + base + offset));
            } break;

            case STR: {
                int* src = get_register(text[pc++]);
                int* reg = get_register(text[pc++]);
                int base = reg ? *reg : 0;
                int offset = *((int*)(text + pc));
                pc += sizeof(int);
                *((int*)(stack + base + offset)) = *src;
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

            case PUSH: {
                int* reg = get_register(text[pc++]);
                if (!reg) {  // IMM
                    int val = *((int*)(text + pc));
                    pc += sizeof(int);
                    *((int*)(stack + sp)) = val;
                } else {
                    *((int*)(stack + sp)) = *reg;
                }
                sp += sizeof(int);
            } break;

            case LEV:
                pc = *((int*)(stack + (bp - sizeof(int) * 2)));
                sp = bp - sizeof(int) * 2;
                bp = *((int*)(stack + (bp - sizeof(int))));
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
                ax = (*((int*)(stack + sp))) - ax;
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
                return ax;

            default:
                assert(0);
        }
    }
}
