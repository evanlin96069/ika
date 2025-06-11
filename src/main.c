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
    fprintf(
        stderr,
        "Usage: ikac [options] file\n"
        "Options:\n"
        "  -E               Preprocess only; do not compile, assemble or "
        "link.\n"
        "  -S               Compile only; do not assemble or link.\n"
        "  -o <file>        Place the output into <file>.\n"
        "  -e <entry>       Specify the program entry point.\n"
        "  -D <macro>       Define a <macro>.\n"
        "  -I <dir>         Add <dir> to the end of the main include path.\n"
        "  -?               Display this information.\n");
}

int main(int argc, char* argv[]) {
    const char* entrypoint = "main";
    const char* src_path;
    const char* out_path = NULL;
    const char* asm_out_path = NULL;
    int s_flag = 0;
    int e_flag = 0;

    UtlArenaAllocator arena = utlarena_init(ARENA_SIZE, &never_fail_allocator);
    UtlAllocator* temp_allocator = &never_fail_allocator;

    Paths include_paths = utlvector_init(temp_allocator);

    SymbolTable define_sym;  // symbol table for #define
    symbol_table_init(&define_sym, 0, NULL, 0, &arena);

#define DEFINE_MACRO(name) symbol_table_append_sym(&define_sym, str(name))

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
        case 'D':
            DEFINE_MACRO(OPTARG(argc, argv));
            break;
        case 'I':
            utlvector_push(&include_paths, OPTARG(argc, argv));
            break;
        case '?':
            usage();
            return 0;
    }

    if (*argv == NULL) {
        utlvector_deinit(&include_paths);
        utlarena_deinit(&arena);
        ika_log(LOG_ERROR, "no input file\n");
        return 1;
    }

    src_path = *argv;

#ifdef __unix__
    DEFINE_MACRO("__unix__");
#endif

#ifdef __linux__
    DEFINE_MACRO("__linux__");
#elif _WIN32
    DEFINE_MACRO("__windows__");
#else
    // Add more targets here
#endif

    PPState pp_state;
    const SourceState* src = &pp_state.src;
    pp_init(&pp_state, &arena, temp_allocator, &include_paths, &define_sym);

    // Read input file

    Error* err = pp_expand(&pp_state, src_path);
    pp_finalize(&pp_state);
    utlvector_deinit(&include_paths);

    if (err != NULL) {
        print_err(src, err);
        utlarena_deinit(&arena);
        return 1;
    }

    if (e_flag) {
        FILE* pp_out = stdout;
        if (out_path) {
            pp_out = fopen(out_path, "w");
            if (!pp_out) {
                ika_log(LOG_ERROR, "cannot open file %s: %s\n", out_path,
                        strerror(errno));
                utlarena_deinit(&arena);
                return 1;
            }
        }

        SourceLine* line = src->lines;
        while (line) {
            fprintf(pp_out, "%s\n", line->content);
            line = line->next;
        }

        if (out_path) {
            fclose(pp_out);
        }

        utlarena_deinit(&arena);
        return 0;
    }

    // Parse
    SymbolTable sym;
    symbol_table_init(&sym, 0, NULL, 1, &arena);

    ParserState parser;
    parser_init(&parser, &sym, &arena, temp_allocator);

    ASTNode* node = parser_parse(&parser, src);
    if (node->type == NODE_ERR) {
        err = ((ErrorNode*)node)->val;
        print_err(src, err);
        utlarena_deinit(&arena);
        return 1;
    }

    Str entry_sym = str(entrypoint);

    // Semantic analysis
    SemaState sema_state = {
        .arena = &arena,
    };

    err = sema(&sema_state, node, &sym, entry_sym);
    if (err != NULL) {
        print_err(src, err);
        utlarena_deinit(&arena);
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
        utlarena_deinit(&arena);
        return 1;
    }

    CodegenState codegen_state = {
        .out = out,
    };

    codegen(&codegen_state, node, &sym, entry_sym);
    fclose(out);

    utlarena_deinit(&arena);

    // Invoke cc
    if (!s_flag) {
#ifdef _WIN32
        char cmd[1024];
        sprintf(cmd, "gcc -m32 -o \"%s\" \"%s\"", out_path, asm_out_path);
        if (system(cmd) != 0) {
            ika_log(LOG_ERROR, "failed to compile %s into %s\n", asm_out_path,
                    out_path);
            _unlink(asm_out_path);
            exit(EXIT_FAILURE);
        }
        _unlink(asm_out_path);
#else
        char* const args[] = {
            "gcc", "-m32",          "-no-pie",
            "-o",  (char*)out_path, (char*)asm_out_path,
            NULL,
        };

        pid_t pid = fork();
        if (pid == -1) {
            ika_log(LOG_ERROR, "fork failed");
            exit(EXIT_FAILURE);
        }

        if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);
            if (status != 0) {
                ika_log(LOG_ERROR, "failed to compile %s into %s\n",
                        asm_out_path, out_path);
                unlink(asm_out_path);
                exit(EXIT_FAILURE);
            }
        } else {
            execvp(args[0], args);
            ika_log(LOG_ERROR, "exec failed");
            exit(EXIT_FAILURE);
        }

        unlink(asm_out_path);
#endif
    }

    return 0;
}
