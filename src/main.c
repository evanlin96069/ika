#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "codegen.h"
#include "error.h"
#include "opt.h"
#include "parser.h"
#include "preprocessor.h"
#include "symbol_table.h"
#include "utils.h"

void usage(void) {
    fprintf(stderr,
            "Usage: ikac [options] file\n"
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
    }

    if (*argv == NULL) {
        ika_log(LOG_ERROR, "no input file\n");
        return 1;
    }

    src_path = *argv;

    Arena pp_arena;
    arena_init(&pp_arena, 1 << 10, NULL);

    SourceState src;
    pp_init(&src, &pp_arena);

    // Read input file

    Error* err = pp_expand(&src, src_path, 0);

    if (err != NULL) {
        print_err(&src, err);
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

        for (size_t i = 0; i < src.line_count; i++) {
            fprintf(pp_out, "%s\n", src.lines[i].content);
        }

        if (out_path) {
            fclose(pp_out);
        }

        arena_deinit(&pp_arena);
        pp_deinit(&src);
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

    ASTNode* node = parser_parse(&parser, &src);
    if (node->type == NODE_ERR) {
        err = ((ErrorNode*)node)->val;
        print_err(&src, err);
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

    CodegenState state = {
        .out = out,
        .arena = &arena,
    };

    err = codegen(&state, node, &sym);
    fclose(out);

    if (err != NULL) {
        print_err(&src, err);
        return 1;
    }

    arena_deinit(&pp_arena);
    arena_deinit(&arena);
    arena_deinit(&sym_arena);
    pp_deinit(&src);

    // Invoke cc
    if (!s_flag) {
        char* const args[] = {
            "gcc", "-m32",          "-no-pie",
            "-o",  (char*)out_path, (char*)asm_out_path,
            NULL,
        };

        pid_t pid = fork();
        if (pid == -1) {
            fprintf(stderr, "fork failed");
            exit(1);
        }

        if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);
            if (status != 0) {
                ika_log(LOG_ERROR, "failed to compile %s into %s\n",
                        asm_out_path, out_path);
            }
        } else {
            execvp(args[0], args);
            fprintf(stderr, "exec failed");
            exit(1);
        }

        unlink(asm_out_path);
    }

    return 0;
}
