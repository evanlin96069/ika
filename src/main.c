#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "codegen.h"
#include "error.h"
#include "opt.h"
#include "parser.h"
#include "preprocessor.h"
#include "sema.h"
#include "symbol_table.h"
#include "utils.h"

#define ARENA_SIZE (1 << 14)

void usage(void) {
    fprintf(stderr,
            "Usage: ikac [options] file\n"
            "Options:\n"
            "  -E               Preprocess only; do not compile, assemble or "
            "link.\n"
            "  -S               Compile only; do not assemble or link.\n"
            "  -o <file>        Place the output into <file>.\n"
            "  -e <name>        Use the name as entrypoint.\n"
            "  -?               Display this information.\n");
}

int main(int argc, char* argv[]) {
    const char* entrypoint = "main";
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
        case 'e':
            entrypoint = OPTARG(argc, argv);
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

    Arena arena;
    arena_init(&arena, ARENA_SIZE, NULL);

    SourceState src;
    pp_init(&src, &arena);

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

        arena_deinit(&arena);
        pp_deinit(&src);
        return 0;
    }

    // Parse
    SymbolTable sym;
    symbol_table_init(&sym, 0, NULL, 1, &arena);

    ParserState parser;
    parser_init(&parser, &sym, &arena);

    ASTNode* node = parser_parse(&parser, &src);
    if (node->type == NODE_ERR) {
        err = ((ErrorNode*)node)->val;
        print_err(&src, err);
        return 1;
    }

    Str entry_sym = str(entrypoint);

    // Semantic analysis
    SemaState sema_state = {
        .arena = &arena,
    };

    err = sema(&sema_state, node, &sym, entry_sym);
    if (err != NULL) {
        print_err(&src, err);
        return 1;
    }

    // Code generation
    if (s_flag) {
        asm_out_path = out_path;
    }

    if (!asm_out_path) {
        asm_out_path = "out.s";
    }

    if (!out_path) {
#ifdef _WIN32
        out_path = "a.exe";
#else
        out_path = "a.out";
#endif
    }

    FILE* out = fopen(asm_out_path, "w");
    if (!out) {
        ika_log(LOG_ERROR, "cannot open file %s: %s\n", out_path,
                strerror(errno));
        return 1;
    }

    CodegenState codegen_state = {
        .out = out,
        .arena = &arena,
    };

    codegen(&codegen_state, node, &sym, entry_sym);
    fclose(out);

    arena_deinit(&arena);
    pp_deinit(&src);

    // Invoke cc
    if (!s_flag) {
#ifdef _WIN32
        char cmd[1024];
        sprintf(cmd, "gcc -m32 -o \"%s\" \"%s\"", out_path, asm_out_path);
        if (system(cmd) != 0) {
            ika_log(LOG_ERROR, "failed to compile %s into %s\n", asm_out_path,
                    out_path);
            _unlink(asm_out_path);
            exit(1);
        }
        _unlink(asm_out_path);
#else
        char* const args[] = {
            "gcc", "-m32", "-o", (char*)out_path, (char*)asm_out_path, NULL,
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
                unlink(asm_out_path);
                exit(1);
            }
        } else {
            execvp(args[0], args);
            fprintf(stderr, "exec failed");
            exit(1);
        }

        unlink(asm_out_path);
#endif
    }

    return 0;
}
