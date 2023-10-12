#include <stdio.h>
#include <string.h>

#include "parser.h"

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
    arena_init(&arena, 1 << 10, NULL);
    ParserState parser;
    parser_init(&parser, &arena);

    char buf[64];
    while (readline(buf, sizeof(buf))) {
        ASTNode* node = parser_parse(&parser, buf);
        if (node->type != NODE_ERR) {
            printf("%d\n", eval(node));
        } else {
            ErrorNode* err = (ErrorNode*)node;
            printf("Error: %s at %ld\n", err->msg, err->pos);
            printf("%s", buf);
            for (size_t i = 0; i < strlen(buf); i++) {
                putchar((i == err->pos) ? '^' : '~');
            }
            putchar('\n');
        }
    }
    printf("\n");
    arena_deinit(&arena);
    return 0;
}
