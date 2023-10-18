#include "vm.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define STACK_SIZE 1 << 10

static uint8_t* bp;
static uint8_t stack[STACK_SIZE];

static int eval(ASTNode* node) {
    if (!node) {
        return 0;
    }

    switch (node->type) {
        case NODE_STMTS: {
            StatementListNode* stmts = (StatementListNode*)node;
            ASTNodeList* iter = stmts->stmts;
            while (iter) {
                eval(iter->node);
                iter = iter->next;
            }
            return 0;
        }

        case NODE_INTLIT: {
            IntLitNode* lit = (IntLitNode*)node;
            return lit->val;
        }

        case NODE_STRLIT: {
            StrLitNode* str_node = (StrLitNode*)node;
            printf("%.*s\n", (int)str_node->str.len, str_node->str.ptr);
            return 0;
        }

        case NODE_BINARYOP: {
            BinaryOpNode* binop = (BinaryOpNode*)node;
            int left = eval(binop->left);
            int right = eval(binop->right);
            switch (binop->op) {
                case TK_MUL:
                    return left * right;

                case TK_DIV:
                    return left / right;

                case TK_MOD:
                    return left % right;

                case TK_ADD:
                    return left + right;

                case TK_SUB:
                    return left - right;

                case TK_LT:
                    return left < right;

                case TK_LE:
                    return left <= right;

                case TK_GT:
                    return left > right;

                case TK_GE:
                    return left >= right;

                case TK_EQ:
                    return left == right;

                case TK_NE:
                    return left != right;

                case TK_AND:
                    return left & right;

                case TK_XOR:
                    return left ^ right;

                case TK_OR:
                    return left | right;

                case TK_LAND:
                    return left && right;

                case TK_LOR:
                    return left || right;

                default:
                    assert(0);
            }
        }

        case NODE_UNARYOP: {
            UnaryOpNode* unaryop = (UnaryOpNode*)node;
            int val = eval(unaryop->node);
            switch (unaryop->op) {
                case TK_ADD:
                    return val;

                case TK_SUB:
                    return -val;

                case TK_NOT:
                    return ~val;

                case TK_LNOT:
                    return !val;

                default:
                    assert(0);
            }
        }

        case NODE_VAR: {
            VarNode* var = (VarNode*)node;
            return *((int*)(bp + var->ste->offset));
        }

        case NODE_ASSIGN: {
            AssignNode* assign = (AssignNode*)node;
            int val = eval(assign->right);
            *((int*)(bp + assign->left->ste->offset)) = val;
            return val;
        }

        case NODE_PRINT: {
            PrintNode* print_node = (PrintNode*)node;
            int val = eval(print_node->expr);
            printf("%d\n", val);
            return 0;
        }

        case NODE_IF: {
            IfStatementNode* if_node = (IfStatementNode*)node;
            int cond = eval(if_node->expr);
            if (cond) {
                eval(if_node->then_block);
            } else if (if_node->else_block) {
                eval(if_node->else_block);
            }
            return 0;
        }

        case NODE_WHILE: {
            WhileNode* while_node = (WhileNode*)node;
            while (eval(while_node->expr)) {
                eval(while_node->block);
            }
            return 0;
        }

        default:
            assert(0);
    }
}

int vm_run(ASTNode* node) {
    bp = stack;
    return eval(node);
}
