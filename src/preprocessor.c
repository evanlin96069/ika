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

void pp_init(PPState* state, UtlArenaAllocator* arena,
             UtlAllocator* temp_allocator, const Paths* include_paths,
             SymbolTable* sym) {
    state->arena = arena;
    state->temp_allocator = temp_allocator;

    state->src.files = (SourceFiles)utlvector_init(state->temp_allocator);
    state->src.lines = NULL;

    state->include_paths = include_paths;
    state->sym = sym;
}

void pp_finalize(PPState* state) {
    UtlAllocator* allocator = utlarena_allocator(state->arena);
    SourceFiles new_files = utlvector_init(allocator);
    utlvector_pushall(&new_files, state->src.files.data, state->src.files.size);
    utlvector_deinit(&state->src.files);
    state->src.files = new_files;
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
// A very simple parser just for preprocessor expression
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

typedef struct PP_IfFrame {
    // are all outer conditions true
    int parent_active;
    // has any branch in this chain evaluated true
    int branch_taken;
    // is the line-emission of this branch enabled
    int this_active;
    // where the chain started
    PP_Type type;
    SourcePos pos;
} PP_IfFrame;

static const StrToken str_pp[] = {
    {"include", PP_INCLUDE}, {"define", PP_DEFINE},   {"undef", PP_UNDEF},
    {"if", PP_IF},           {"elif", PP_ELIF},       {"else", PP_ELSE},
    {"endif", PP_ENDIF},     {"warning", PP_WARNING}, {"error", PP_ERROR},
};

void read_lines(UtlAllocator* allocator, char* src, int file_index,
                SourceLine** start, SourceLine** end) {
    SourceLine* lines = NULL;
    SourceLine* curr = NULL;

    size_t pos = 0;
    char c = *(src + pos);
    int lineno = 1;
    do {
        size_t start_pos = pos;
        while (c != '\n' && c != '\0') {
            pos++;
            c = *(src + pos);
        }

        size_t line_len = pos - start_pos;
        SourceLine* line = allocator->alloc(allocator, sizeof(SourceLine));
        line->next = NULL;
        line->file_index = file_index;
        line->lineno = lineno;
        lineno++;

        line->content = src + start_pos;
        if (c != '\0') {
            pos++;
            c = *(src + pos);
        }
        line->content[line_len] = '\0';

        // fix CRLF
        if (line->content[line_len - 1] == '\r') {
            line->content[line_len - 1] = '\0';
        }

        if (!lines) {
            lines = line;
            curr = line;
        } else {
            curr->next = line;
            curr = line;
        }
    } while (c != '\0');

    *start = lines;
    *end = curr;
}

Error* pp_expand(PPState* state, const char* filename) {
    SourceFile orig_file = {0};
    size_t size = strlen(filename) + 1;
    orig_file.filename = utlarena_alloc(state->arena, size);
    memcpy(orig_file.filename, filename, size);
    orig_file.pos.index = 0;
    orig_file.is_open = 0;
    utlvector_push(&state->src.files, orig_file);

    UtlAllocator* allocator = utlarena_allocator(state->arena);
    char* src = read_entire_file(allocator, filename);
    if (!src) {
        return error(state->arena, state->src.files.data[0].pos,
                     "failed to read file: %s", strerror(errno));
    }
    state->src.files.data[0].is_open = 1;

    SourceLine* start;
    SourceLine* end;
    read_lines(allocator, src, 0, &start, &end);
    state->src.lines = start;

    int include_depth = 0;
    SourceLine* include_stack[MAX_INCLUDE_DEPTH];

    SourceLine* prev_line = NULL;
    SourceLine* line = start;

    Error* err = NULL;
    UtlVector(PP_IfFrame) if_stack = utlvector_init(state->temp_allocator);

    while (line) {
        if (include_depth > 0 && line == include_stack[include_depth - 1]) {
            include_depth--;
        }

        int is_active = (if_stack.size == 0 ||
                         if_stack.data[if_stack.size - 1].this_active);

        // process line
        char* p = line->content;

        while (*p == ' ' || *p == '\t') {
            p++;
        }

        if (*p != '#') {
            if (!is_active) {
                goto remove_line;
            }
            prev_line = line;
            line = line->next;
            continue;
        }
        p++;

        while (*p == ' ' || *p == '\t') {
            p++;
        }

        SourcePos pp_start_pos;
        pp_start_pos.line = *line;
        pp_start_pos.index = p - line->content;

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
        curr_pos.line = *line;
        curr_pos.index = p - line->content;

        if (pp_type == PP_NONE) {
            if (!is_active) {
                goto remove_line;
            }
            err = error(state->arena, curr_pos,
                        "invalid preprocessing directive");
            goto defer;
        }

        PP_ParserState parser;
        pp_parser_init(&parser, state->arena, state->temp_allocator, state->sym,
                       curr_pos);

#define PP_NO_MORE_TOKENS()                                             \
    do {                                                                \
        if (pp_next_token(&parser).type != TK_EOF) {                    \
            err = error(state->arena, parser.token_start,               \
                        "single-line comment or end-of-line expected"); \
            goto defer;                                                 \
        }                                                               \
    } while (0)

        switch (pp_type) {
            case PP_INCLUDE: {
                if (!is_active) {
                    break;
                }

                Token tk = pp_next_token(&parser);
                if (tk.type == TK_ERR) {
                    err = error(state->arena, parser.token_end, "%.*s",
                                tk.str.len, tk.str.ptr);
                    goto defer;
                }

                if (tk.type != TK_STR) {
                    err = error(state->arena, parser.token_start,
                                "#include expects \"FILENAME\"");
                    goto defer;
                }

                SourcePos str_pos = parser.token_start;

                PP_NO_MORE_TOKENS();

                if (include_depth + 1 > MAX_INCLUDE_DEPTH) {
                    err = error(state->arena, str_pos,
                                "#include nested too deeply");
                    goto defer;
                }

                char inc_path[OS_PATH_MAX];
                int inc_path_size = 0;
                for (size_t i = 0; i < state->include_paths->size; i++) {
                    inc_path_size = snprintf(
                        inc_path, sizeof(inc_path), "%s" OS_PATH_SEP "%.*s",
                        state->include_paths->data[i], tk.str.len, tk.str.ptr);
                    if (file_is_readable(inc_path)) {
                        break;
                    }
                }

                if (inc_path_size <= 0) {
                    Str dir = get_dir_name(
                        str(state->src.files.data[line->file_index].filename));
                    if (str_eql(dir, str("."))) {
                        inc_path_size =
                            snprintf(inc_path, sizeof(inc_path), "%.*s",
                                     tk.str.len, tk.str.ptr);
                    } else {
                        inc_path_size =
                            snprintf(inc_path, sizeof(inc_path),
                                     "%.*s" OS_PATH_SEP "%.*s", dir.len,
                                     dir.ptr, tk.str.len, tk.str.ptr);
                    }
                }

                assert(inc_path_size > 0);

                SourceFile file = {0};
                file.filename = utlarena_alloc(state->arena, inc_path_size);
                memcpy(file.filename, inc_path, inc_path_size);
                file.pos = parser.token_start;
                file.is_open = 0;

                size_t file_index = state->src.files.size;
                utlvector_push(&state->src.files, file);

                src = read_entire_file(allocator, inc_path);
                if (!src) {
                    err = error(state->arena, str_pos,
                                "failed to read file: %s", strerror(errno));
                    goto defer;
                }

                state->src.files.data[file_index].is_open = 1;

                read_lines(allocator, src, file_index, &start, &end);
                include_stack[include_depth] = end;
                include_depth++;

                end->next = line->next;
                line->next = start;
            } break;

            case PP_DEFINE: {
                if (!is_active) {
                    break;
                }

                Token tk = pp_next_token(&parser);
                if (tk.type == TK_ERR) {
                    err = error(state->arena, parser.token_end, "%.*s",
                                tk.str.len, tk.str.ptr);
                    goto defer;
                }

                if (tk.type != TK_IDENT) {
                    err = error(state->arena, parser.token_start,
                                "#define expects an identifier");
                    goto defer;
                }

                PP_NO_MORE_TOKENS();

                if (symbol_table_find(state->sym, tk.str, 1) == NULL) {
                    symbol_table_append_sym(state->sym, tk.str);
                }
            } break;

            case PP_UNDEF: {
                if (!is_active) {
                    break;
                }

                Token tk = pp_next_token(&parser);
                if (tk.type == TK_ERR) {
                    err = error(state->arena, parser.token_end, "%.*s",
                                tk.str.len, tk.str.ptr);
                    goto defer;
                }

                if (tk.type != TK_IDENT) {
                    err = error(state->arena, parser.token_start,
                                "#undef expects an identifier");
                    goto defer;
                }

                PP_NO_MORE_TOKENS();

                symbol_table_remove(state->sym, tk.str);
            } break;

            case PP_IF: {
                int result = 0;
                if (is_active) {
                    err = pp_expr(&parser, 0, &result);
                    if (err != NULL) {
                        goto defer;
                    }
                    PP_NO_MORE_TOKENS();
                }

                PP_IfFrame frame = {
                    .parent_active = is_active,
                    .branch_taken = result,
                    .this_active = is_active && result,
                    .type = PP_IF,
                    .pos = pp_start_pos,
                };
                utlvector_push(&if_stack, frame);
            } break;

            case PP_ELIF: {
                if (if_stack.size == 0) {
                    err =
                        error(state->arena, pp_start_pos, "#elif without #if");
                    goto defer;
                }

                PP_IfFrame f = if_stack.data[if_stack.size - 1];
                utlvector_pop(&if_stack);
                int result = 0;
                if (f.parent_active && !f.branch_taken) {
                    err = pp_expr(&parser, 0, &result);
                    if (err != NULL) {
                        goto defer;
                    }
                    PP_NO_MORE_TOKENS();
                }

                PP_IfFrame frame = {
                    .parent_active = f.parent_active,
                    .branch_taken = f.branch_taken || result,
                    .this_active = result,
                    .type = PP_ELIF,
                    .pos = pp_start_pos,
                };
                utlvector_push(&if_stack, frame);
            } break;

            case PP_ELSE:
                if (if_stack.size == 0) {
                    err =
                        error(state->arena, pp_start_pos, "#elif without #if");
                    goto defer;
                }

                PP_NO_MORE_TOKENS();

                PP_IfFrame f = if_stack.data[if_stack.size - 1];
                utlvector_pop(&if_stack);

                if (f.type == PP_ELSE) {
                    err =
                        error(state->arena, pp_start_pos, "#else after #else");
                    goto defer;
                }

                PP_IfFrame frame = {
                    .parent_active = f.parent_active,
                    .branch_taken = true,
                    .this_active = f.parent_active && !f.branch_taken,
                    .type = PP_ELSE,
                    .pos = pp_start_pos,
                };
                utlvector_push(&if_stack, frame);
                break;

            case PP_ENDIF:
                if (if_stack.size == 0) {
                    err =
                        error(state->arena, pp_start_pos, "#endif without #if");
                    goto defer;
                }
                utlvector_pop(&if_stack);
                PP_NO_MORE_TOKENS();
                break;

            case PP_ERROR:
                if (!is_active) {
                    break;
                }

                while (*p == ' ' || *p == '\t') {
                    p++;
                }
                err = error(state->arena, pp_start_pos, "%s", p);
                goto defer;

            case PP_WARNING:
                if (!is_active) {
                    break;
                }

                while (*p == ' ' || *p == '\t') {
                    p++;
                }
                print_warn(&state->src,
                           error(state->arena, pp_start_pos, "%s", p));
                break;

            default:
                UNREACHABLE();
        }

    remove_line:
        if (!prev_line) {
            state->src.lines = line->next;
        } else {
            prev_line->next = line->next;
        }
        line = line->next;
    }

    if (if_stack.size != 0) {
        PP_IfFrame f = if_stack.data[if_stack.size - 1];
        const char* msg;
        switch (f.type) {
            case PP_IF:
                msg = "unterminated #if";
                break;
            case PP_ELIF:
                msg = "unterminated #elif";
                break;
            case PP_ELSE:
                msg = "unterminated #else";
                break;
            default:
                UNREACHABLE();
        }
        err = error(state->arena, f.pos, msg);
    }

defer:
    utlvector_deinit(&if_stack);
    return err;
}
