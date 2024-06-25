#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen.h"
#include "opt.h"
#include "parser.h"
#include "preprocessor.h"
#include "symbol_table.h"
#include "utils.h"

static void print_err(const char* src, const char* filename, Error* err) {
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

    fprintf(stderr, "%s:%d:%d: ", filename, line_num, line_pos);
    ika_log(LOG_ERROR, "%s\n", err->msg);

    fprintf(stderr, "%5d | %.*s\n", line_num, line_len, line);

    if (line_pos > 0) {
        fprintf(stderr, "      | %*c^\n", line_pos, ' ');
    } else {
        fprintf(stderr, "      | ^\n");
    }
}

void usage(void) {
    fprintf(stderr,
            "Usage: ikac [options] file...\n"
            "Options:\n"
            "  -E               Preprocess only; do not compile, assemble or "
            "link.\n"
            "  -S               Compile only; do not assemble or link.\n"
            "  -o <file>        Place the output into <file>.\n"
            "  -?               Display this information.\n");
}

int main(int argc, char* argv[]) {
    const char* src_path;
    const char* out_path = NULL;
    const char* asm_out_path = NULL;
    int s_flag = 0;
    int e_flag = 0;

    // Arg parse
    FOR_OPTS(argc, argv) {
        case 'o':
            out_path = OPTARG(argc, argv);
            break;
        case 'S':
            s_flag = 1;
            break;
        case 'E':
            e_flag = 1;
            break;
        case '?':
            usage();
            return 0;
    };

    if (*argv == NULL) {
        ika_log(LOG_ERROR, "no input file\n");
        return 1;
    }

    src_path = *argv;

    // Read input file
    char* src = pp_expand(src_path, 0);
    if (!src) {
        return 1;
    }

    if (e_flag) {
        FILE* pp_out = stdout;
        if (out_path) {
            pp_out = fopen(out_path, "w");
            if (!pp_out) {
                ika_log(LOG_ERROR, "cannot open file %s: %s\n", out_path,
                        strerror(errno));
                return 1;
            }
        }

        fprintf(pp_out, "%s\n", src);

        if (out_path) {
            fclose(pp_out);
        }

        free(src);
        return 0;
    }

    // Parse
    Arena sym_arena;
    arena_init(&sym_arena, 1 << 10, NULL);
    SymbolTable sym;
    symbol_table_init(&sym, 0, NULL, 1, &sym_arena);

    Arena arena;
    arena_init(&arena, 1 << 10, NULL);
    ParserState parser;
    parser_init(&parser, &sym, &arena);

    Error* err = NULL;

    ASTNode* node = parser_parse(&parser, src);
    if (node->type == NODE_ERR) {
        err = ((ErrorNode*)node)->val;
        print_err(src, src_path, err);
        free(err);

        arena_deinit(&arena);
        arena_deinit(&sym_arena);
        free(src);

        return 1;
    }

    // codegen
    if (s_flag) {
        asm_out_path = out_path;
    }

    if (!asm_out_path) {
        asm_out_path = "out.s";
    }

    if (!out_path) {
        out_path = "a.out";
    }

    FILE* out = fopen(asm_out_path, "w");
    if (!out) {
        ika_log(LOG_ERROR, "cannot open file %s: %s\n", out_path,
                strerror(errno));
        return 1;
    }

    err = codegen(out, node, &sym);
    fclose(out);

    if (err != NULL) {
        print_err(src, src_path, err);
        free(err);

        arena_deinit(&arena);
        arena_deinit(&sym_arena);
        free(src);

        return 1;
    }

    arena_deinit(&arena);
    arena_deinit(&sym_arena);
    free(src);

    // Invoke cc
    if (!s_flag) {
        char command[1024];
        snprintf(command, sizeof(command), "cc %s -m32 -no-pie -o %s",
                 asm_out_path, out_path);
        int ret = system(command);
        if (ret != 0) {
            ika_log(LOG_ERROR, "failed to compile %s into %s\n", asm_out_path,
                    out_path);
        }

        snprintf(command, sizeof(command), "rm %s", asm_out_path);
        ret = system(command);
        if (ret != 0) {
            ika_log(LOG_ERROR, "failed remove file: %s\n", asm_out_path);
        }
    }

    return 0;
}
