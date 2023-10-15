#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "symbol_table.h"

static int eval(ASTNode* node) {
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

        case NODE_BINARYOP: {
            BinaryOpNode* binop = (BinaryOpNode*)node;
            int left = eval(binop->left);
            int right = eval(binop->right);
            switch (binop->op) {
                case TK_ADD:
                    return left + right;
                case TK_SUB:
                    return left - right;
                case TK_MUL:
                    return left * right;
                case TK_DIV:
                    return left / right;
                default:
                    return -1;
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

        case NODE_PRINT: {
            PrintNode* print_node = (PrintNode*)node;
            int val = eval(print_node->stmt);
            printf("%d\n", val);
            return 0;
        }

        default:
            return -1;
    }
}

static void print_err(ParserState* parser, const char* file_name,
                      ErrorNode* err) {
    const char* line = parser->src;
    int line_num = 0;
    int line_pos = 0;

    size_t pos = 0;
    while (pos < err->pos) {
        if (parser->src[pos] == '\n') {
            line = parser->src + pos + 1;
            line_num++;
            line_pos = 0;
        } else {
            line_pos++;
        }
        pos++;
    }

    int line_len = 0;
    while (line[line_len] != '\n' && line[line_len] != '\0') {
        line_len++;
    }

    fprintf(stderr, "%s:%d:%d: \x1b[31merror:\x1b[0m %s\n", file_name, line_num,
            line_pos, err->msg);
    fprintf(stderr, "%5d | %.*s\n", line_num, line_len, line);
    if (line_pos > 0) {
        fprintf(stderr, "      | %*c^\n", line_pos, ' ');
    } else {
        fprintf(stderr, "      | ^\n");
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "\x1b[31merror:\x1b[0m no input file\n");
        return 1;
    }

    FILE* fp = fopen(argv[1], "r");
    if (!fp) {
        fprintf(stderr, "cannot open file %s: %s\n", argv[1], strerror(errno));
        return 1;
    }

    long size;
    fseek(fp, 0L, SEEK_END);
    size = ftell(fp);
    rewind(fp);

    char* buf = malloc(size + 1);
    if (!buf)
        return 1;

    if (fread(buf, size, 1, fp) != 1)
        return 1;
    buf[size] = '\0';

    Arena sym_arena;
    arena_init(&sym_arena, 1 << 10, NULL);
    SymbolTable sym;
    symbol_table_init(&sym, &sym_arena);

    Arena arena;
    arena_init(&arena, 1 << 10, NULL);
    ParserState parser;
    parser_init(&parser, &sym, &arena);

    ASTNode* node = parser_parse(&parser, buf);
    if (node->type == NODE_ERR) {
        print_err(&parser, argv[1], (ErrorNode*)node);

        arena_deinit(&arena);
        arena_deinit(&sym_arena);
        free(buf);

        return 1;
    }

    eval(node);

    arena_deinit(&arena);
    arena_deinit(&sym_arena);
    free(buf);

    return 0;
}
