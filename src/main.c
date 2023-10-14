#include <stdio.h>
#include <string.h>

#include "parser.h"
#include "symbol_table.h"

int eval(ASTNode* node) {
    switch (node->type) {
        case NODE_LIT: {
            LitNode* lit = (LitNode*)node;
            return lit->val;
        }

        case NODE_BINOP: {
            BinOpNode* binop = (BinOpNode*)node;
            int left = eval(binop->left);
            int right = eval(binop->right);
            switch (binop->op) {
                case '+':
                    return left + right;
                case '-':
                    return left - right;
                case '*':
                    return left * right;
                case '/':
                    return left / right;
                default:
                    return -1;
            }
        }

        case NODE_UNARYOP: {
            UnaryOpNode* unaryop = (UnaryOpNode*)node;
            int val = eval(unaryop->node);
            switch (unaryop->op) {
                case '+':
                    return val;
                case '-':
                    return -val;
                default:
                    return -1;
            }
        }

        case NODE_VAR: {
            VarNode* var = (VarNode*)node;
            return var->ste->val;
        }

        case NODE_ASSIGN: {
            AssignNode* assign = (AssignNode*)node;
            int val = eval(assign->right);
            assign->left->ste->val = val;
            return val;
        }

        default:
            return -1;
    }
}

int readline(char* buf, int n) {
    printf(">>> ");
    return fgets(buf, n, stdin) != NULL;
}

int main(void) {
    Arena arena;
    Arena sym_arena;
    arena_init(&arena, 1 << 10, NULL);
    arena_init(&sym_arena, 1 << 10, NULL);

    SymbolTable sym;
    symbol_table_init(&sym, &sym_arena);

    char buf[64];
    while (readline(buf, sizeof(buf))) {
        ParserState parser;
        parser_init(&parser, &sym, &arena);

        ASTNode* node = parser_parse(&parser, buf);
        if (!node)
            continue;

        if (node->type != NODE_ERR) {
            printf("%d\n", eval(node));
        } else {
            ErrorNode* err = (ErrorNode*)node;
            printf("%ld: error: %s\n", parser.token_pos, err->msg);
            printf("| %s", buf);
            printf("| %*c^\n", (int)parser.token_pos, ' ');
        }
    }
    printf("\n");

    arena_deinit(&arena);
    arena_deinit(&sym_arena);

    return 0;
}
