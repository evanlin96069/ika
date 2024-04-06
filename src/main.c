#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen.h"
#include "opt.h"
#include "parser.h"
#include "symbol_table.h"

static void print_err(const char* src, const char* file_name, Error* err) {
    const char* line = src;
    int line_num = 1;
    int line_pos = 0;

    size_t pos = 0;
    while (pos < err->pos) {
        if (src[pos] == '\n') {
            line = src + pos + 1;
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
    const char* src_path;
    const char* out_path = "out.s";

    FOR_OPTS(argc, argv, {
        case 'o':
            out_path = OPTARG(argc, argv);
            break;
    });

    if (*argv == NULL) {
        fprintf(stderr, "\x1b[31merror:\x1b[0m no input file\n");
        return 1;
    }

    src_path = *argv;

    FILE* fp = fopen(src_path, "r");
    if (!fp) {
        fprintf(stderr, "\x1b[31merror:\x1b[0m cannot open file %s: %s\n",
                src_path, strerror(errno));
        return 1;
    }

    FILE* out = fopen(out_path, "w");
    if (!out) {
        fprintf(stderr, "\x1b[31merror:\x1b[0m cannot open file %s: %s\n",
                out_path, strerror(errno));
        return 1;
    }

    long size;
    fseek(fp, 0L, SEEK_END);
    size = ftell(fp);
    rewind(fp);

    if (size == 0) {
        fclose(fp);
        return 0;
    }

    char* buf = malloc(size + 1);
    if (!buf) {
        fprintf(stderr, "\x1b[31merror:\x1b[0m Failed to allocate memory\n");
        fclose(fp);
        return 1;
    }

    int n_read = fread(buf, size, 1, fp);
    fclose(fp);

    if (n_read != 1) {
        fprintf(stderr, "\x1b[31merror:\x1b[0m Failed to read file\n");
        free(buf);
        return 1;
    }

    buf[size] = '\0';

    Arena sym_arena;
    arena_init(&sym_arena, 1 << 10, NULL);
    SymbolTable sym;
    symbol_table_init(&sym, 0, NULL, 1, &sym_arena);

    Arena arena;
    arena_init(&arena, 1 << 10, NULL);
    ParserState parser;
    parser_init(&parser, &sym, &arena);

    Error* err = NULL;

    ASTNode* node = parser_parse(&parser, buf);
    if (node->type == NODE_ERR) {
        err = ((ErrorNode*)node)->val;
        print_err(buf, src_path, err);
        free(err);

        arena_deinit(&arena);
        arena_deinit(&sym_arena);
        free(buf);

        return 1;
    }

    err = codegen(out, node, &sym);
    if (err != NULL) {
        print_err(buf, src_path, err);
        free(err);
    }

    arena_deinit(&arena);
    arena_deinit(&sym_arena);
    free(buf);

    return 0;
}
