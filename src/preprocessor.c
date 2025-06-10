#include "preprocessor.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "lexer.h"
#include "symbol_table.h"
#include "utils.h"

#define FILE_INIT_SIZE 4
#define LINE_INIT_SIZE 256
#define ARRAY_GROUTH_RATE 2

void pp_init(PPState* state, UtlArenaAllocator* arena,
             UtlAllocator* temp_allocator) {
    state->arena = arena;
    state->temp_allocator = temp_allocator;

    state->src.files = (SourceFiles)utlvector_init(state->temp_allocator);
    state->src.lines = (SourceLines)utlvector_init(state->temp_allocator);

    state->last_include = 0;

    state->sym = utlarena_alloc(state->arena, sizeof(SymbolTable));
    symbol_table_init(state->sym, 0, NULL, 0, state->arena);
}

void pp_finalize(PPState* state) {
    UtlAllocator* allocator = utlarena_allocator(state->arena);
    SourceFiles new_files = utlvector_init(allocator);
    SourceLines new_lines = utlvector_init(allocator);

    utlvector_pushall(&new_files, state->src.files.data, state->src.files.size);
    utlvector_pushall(&new_lines, state->src.lines.data, state->src.lines.size);

    utlvector_deinit(&state->src.files);
    utlvector_deinit(&state->src.lines);

    state->src.files = new_files;
    state->src.lines = new_lines;
}

static Error* error(UtlArenaAllocator* arena, SourcePos pos, const char* fmt,
                    ...) {
    Error* error = utlarena_alloc(arena, sizeof(Error));
    error->pos = pos;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(error->msg, ERROR_MAX_LENGTH, fmt, ap);
    va_end(ap);

    return error;
}

// Preprocessor Parser
// A very simple version parser just for preprocessor expression
// binary operators: || && == !=
// unary operators: !
typedef struct PP_ParserState {
    UtlArenaAllocator* arena;
    UtlAllocator* temp_allocator;
    SymbolTable* sym;

    SourcePos line_pos;
    const char* line;
    int pos;

    SourcePos prev_token_end;
    SourcePos token_start;
    SourcePos token_end;
} PP_ParserState;

static void pp_parser_init(PP_ParserState* state, UtlArenaAllocator* arena,
                           UtlAllocator* temp_allocator, SymbolTable* sym,
                           SourcePos pos) {
    state->arena = arena;
    state->temp_allocator = temp_allocator;
    state->sym = sym;

    state->line_pos = pos;
    state->line = pos.line.content + pos.index;
    state->pos = 0;

    state->prev_token_end = pos;
    state->token_start = pos;
    state->token_end = pos;
}

static const StrToken str_tf[] = {
    {"true", TK_TRUE},
    {"false", TK_FALSE},
};

static Token pp_next_token_internal(PP_ParserState* state, int peek) {
    int start, end;
    Token tk = next_token_from_line(state->arena, state->temp_allocator,
                                    state->line + state->pos, str_tf,
                                    ARRAY_SIZE(str_tf), &start, &end);
    if (!peek) {
        state->prev_token_end = state->token_end;
        state->token_start.index = state->line_pos.index + state->pos + start;
        state->token_end.index = state->line_pos.index + state->pos + end;
        state->pos += end;
    }

    return tk;
}

static inline Token pp_next_token(PP_ParserState* state) {
    return pp_next_token_internal(state, 0);
}

static inline Token pp_peek_token(PP_ParserState* state) {
    return pp_next_token_internal(state, 1);
}

static Error* pp_expr(PP_ParserState* parser, int min_precedence, int* result);

static Error* pp_primary(PP_ParserState* state, int* result) {
    Error* err;
    Token tk = pp_next_token(state);
    switch (tk.type) {
        case TK_TRUE:
            *result = 1;
            break;

        case TK_FALSE:
            *result = 0;
            break;

        case TK_IDENT:
            *result = (symbol_table_find(state->sym, tk.str, 1) != NULL);
            break;

        case TK_LPAREN:
            err = pp_expr(state, 0, result);
            if (err != NULL) {
                return err;
            }
            tk = pp_next_token(state);
            if (tk.type != TK_RPAREN) {
                return error(state->arena, state->prev_token_end,
                             "expected ')'");
            }
            break;

        case TK_LNOT:
            err = pp_primary(state, result);
            if (err != NULL) {
                return err;
            }
            *result = !(*result);
            break;

        case TK_ERR:
            return error(state->arena, state->token_end, "%.*s", tk.str.len,
                         tk.str.ptr);

        default:
            return error(state->arena, state->token_start,
                         "invalid preprocessor expression");
    }

    return NULL;
}

static inline int get_precedence(TkType type) {
    switch (type) {
        case TK_LOR:
            return 1;

        case TK_LAND:
            return 2;

        case TK_EQ:
        case TK_NE:
            return 3;

        default:
            return -1;
    }
}

static Error* pp_expr(PP_ParserState* state, int min_precedence, int* result) {
    int right;
    Error* err = pp_primary(state, result);
    if (err != NULL) {
        return err;
    }

    int precedence = get_precedence(pp_peek_token(state).type);
    while (precedence != -1 && precedence >= min_precedence) {
        Token tk = pp_next_token(state);

        int next_precedence = precedence;
        next_precedence++;

        if (next_precedence < min_precedence) {
            return NULL;
        }

        err = pp_expr(state, next_precedence, &right);
        if (err != NULL) {
            return err;
        }

        switch (tk.type) {
            case TK_LOR:
                *result = (*result || right);
                break;

            case TK_LAND:
                *result = (*result && right);
                break;

            case TK_EQ:
                *result = (*result == right);
                break;

            case TK_NE:
                *result = (*result != right);
                break;

            default:
                break;
        }
        precedence = get_precedence(pp_peek_token(state).type);
    }

    return NULL;
}

typedef enum PP_Type {
    PP_NONE,
    PP_INCLUDE,
    PP_DEFINE,
    PP_UNDEF,
    PP_IF,
    PP_ELIF,
    PP_ELSE,
    PP_ENDIF,
    PP_WARNING,
    PP_ERROR,
} PP_Type;

static const StrToken str_pp[] = {
    {"include", PP_INCLUDE}, {"define", PP_DEFINE},   {"undef", PP_UNDEF},
    {"if", PP_IF},           {"elif", PP_DEFINE},     {"else", PP_ELSE},
    {"endif", PP_ENDIF},     {"warning", PP_WARNING}, {"error", PP_ERROR},
};

Error* pp_expand(PPState* state, const char* filename, int depth) {
    if (state->src.files.size == 0) {
        // original source
        SourceFile orig_file = {0};
        size_t size = strlen(filename) + 1;
        orig_file.filename = utlarena_alloc(state->arena, size);
        memcpy(orig_file.filename, filename, size);
        orig_file.pos.index = 0;
        orig_file.is_open = 0;

        utlvector_push(&state->src.files, orig_file);
    }

    int last_include = state->last_include;
    int curr_file_index = state->src.files.size - 1;
    state->last_include = curr_file_index;

    if (depth > MAX_INCLUDE_DEPTH) {
        return error(state->arena, state->src.files.data[last_include].pos,
                     "#include nested too deeply");
    }

    char* src = read_entire_file(utlarena_allocator(state->arena), filename);
    if (!src) {
        return error(state->arena, state->src.files.data[last_include].pos,
                     "failed to read file: %s", strerror(errno));
    }

    state->src.files.data[curr_file_index].is_open = 1;

    size_t pos = 0;
    char c = *(src + pos);
    int lineno = 1;
    do {
        // read line
        size_t start_pos = pos;
        while (c != '\n' && c != '\0') {
            pos++;
            c = *(src + pos);
        }

        size_t line_len = pos - start_pos;
        SourceLine line;
        line.file_index = curr_file_index;
        line.lineno = lineno;
        lineno++;

        line.content = src + start_pos;
        if (c != '\0') {
            pos++;
            c = *(src + pos);
        }
        line.content[line_len] = '\0';

        // fix CRLF
        if (line.content[line_len - 1] == '\r') {
            line.content[line_len - 1] = '\0';
        }

        // process line
        char* p = line.content;

        while (*p == ' ' || *p == '\t') {
            p++;
        }

        if (*p != '#') {
            utlvector_push(&state->src.lines, line);
            continue;
        }
        p++;

        while (*p == ' ' || *p == '\t') {
            p++;
        }

        SourcePos pp_start_pos;
        pp_start_pos.line = line;
        pp_start_pos.index = p - line.content;

        PP_Type pp_type = PP_NONE;
        for (unsigned int i = 0; i < ARRAY_SIZE(str_pp); i++) {
            int pp_len = strlen(str_pp[i].s);
            if (strncmp(str_pp[i].s, p, pp_len) == 0) {
                pp_type = str_pp[i].token_type;
                p += pp_len;
                break;
            }
        }

        SourcePos curr_pos;
        curr_pos.line = line;
        curr_pos.index = p - line.content;

        if (pp_type == PP_NONE) {
            return error(state->arena, curr_pos,
                         "invalid preprocessing directive");
        }

        PP_ParserState parser;
        pp_parser_init(&parser, state->arena, state->temp_allocator, state->sym,
                       curr_pos);

#define PP_NO_MORE_TOKENS()                                              \
    do {                                                                 \
        if (pp_next_token(&parser).type != TK_EOF) {                     \
            return error(state->arena, parser.token_start,               \
                         "single-line comment or end-of-line expected"); \
        }                                                                \
    } while (0)

        switch (pp_type) {
            case PP_INCLUDE: {
                Token tk = pp_next_token(&parser);
                if (tk.type == TK_ERR) {
                    return error(state->arena, parser.token_end, "%.*s",
                                 tk.str.len, tk.str.ptr);
                }

                if (tk.type != TK_STR) {
                    return error(state->arena, parser.token_start,
                                 "#include expects \"FILENAME\"");
                }

                Str dir = get_dir_name(str(filename));
                char inc_path[OS_PATH_MAX];
                int inc_path_size =
                    snprintf(inc_path, sizeof(inc_path), "%.*s/%.*s", dir.len,
                             dir.ptr, tk.str.len, tk.str.ptr);
                assert(inc_path_size > 0);

                if (curr_file_index == 0) {
                    state->src.files.data[0].pos = parser.token_start;
                } else {
                    SourceFile file = {0};
                    file.filename = utlarena_alloc(state->arena, inc_path_size);
                    memcpy(file.filename, inc_path, inc_path_size);
                    file.pos = parser.token_start;
                    file.is_open = 0;

                    utlvector_push(&state->src.files, file);
                }

                PP_NO_MORE_TOKENS();

                Error* err = pp_expand(state, inc_path, depth + 1);
                if (err != NULL) {
                    return err;
                }
            } break;

            case PP_DEFINE: {
                Token tk = pp_next_token(&parser);
                if (tk.type == TK_ERR) {
                    return error(state->arena, parser.token_end, "%.*s",
                                 tk.str.len, tk.str.ptr);
                }

                if (tk.type != TK_IDENT) {
                    return error(state->arena, parser.token_start,
                                 "#define expects an identifier");
                }

                symbol_table_append_sym(state->sym, tk.str);

                PP_NO_MORE_TOKENS();
            } break;

            case PP_UNDEF: {
                Token tk = pp_next_token(&parser);
                if (tk.type == TK_ERR) {
                    return error(state->arena, parser.token_end, "%.*s",
                                 tk.str.len, tk.str.ptr);
                }

                if (tk.type != TK_IDENT) {
                    return error(state->arena, parser.token_start,
                                 "#undef expects an identifier");
                }

                symbol_table_remove(state->sym, tk.str);

                PP_NO_MORE_TOKENS();
            } break;

            case PP_IF: {
                int result;
                Error* err = pp_expr(&parser, 0, &result);
                if (err != NULL) {
                    return err;
                }
                ika_log(LOG_DEBUG, "#if %d\n", result);

                PP_NO_MORE_TOKENS();
            } break;

            case PP_ELIF: {
                int result;
                Error* err = pp_expr(&parser, 0, &result);
                if (err != NULL) {
                    return err;
                }

                PP_NO_MORE_TOKENS();
            } break;

            case PP_ELSE:
                PP_NO_MORE_TOKENS();
                break;

            case PP_ENDIF:
                PP_NO_MORE_TOKENS();
                break;

            case PP_ERROR: {
                while (*p == ' ' || *p == '\t') {
                    p++;
                }
                return error(state->arena, pp_start_pos, "%s", p);
            } break;

            case PP_WARNING: {
                while (*p == ' ' || *p == '\t') {
                    p++;
                }
                print_warn(&state->src,
                           error(state->arena, pp_start_pos, "%s", p));
            } break;

            default:
                UNREACHABLE();
        }
    } while (c != '\0');

    state->last_include = last_include;

    return NULL;
}
