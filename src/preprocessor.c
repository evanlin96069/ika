#include "preprocessor.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "symbol_table.h"
#include "utils.h"

#define FILE_INIT_SIZE 4
#define LINE_INIT_SIZE 256
#define ARRAY_GROUTH_RATE 2

void pp_init(PPState* state, Arena* arena) {
    state->arena = arena;
    state->src.files = malloc(FILE_INIT_SIZE * sizeof(SourceFile));
    state->src.file_count = 0;
    state->src.file_capacity = FILE_INIT_SIZE;
    state->src.lines = malloc(LINE_INIT_SIZE * sizeof(SourceLine));
    state->src.line_count = 0;
    state->src.line_capacity = LINE_INIT_SIZE;
    state->last_include = 0;

    state->sym =
        arena_alloc(state->arena, sizeof(SymbolTable));
    symbol_table_init(state->sym, 0, NULL, 0, state->arena);
}

void pp_deinit(PPState* state) {
    free(state->src.files);
    free(state->src.lines);
}

static Error* error(SourcePos pos, const char* fmt, ...) {
    Error* error = malloc(sizeof(Error));
    error->pos = pos;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(error->msg, ERROR_MAX_LENGTH, fmt, ap);
    va_end(ap);

    return error;
}

static inline void append_file(SourceState* state, SourceFile file) {
    if (state->file_count + 1 > state->file_capacity) {
        state->file_capacity *= ARRAY_GROUTH_RATE;
        state->files =
            realloc(state->files, state->file_capacity * sizeof(SourceFile));
    }
    state->files[state->file_count] = file;
    state->file_count++;
}

static inline void append_line(SourceState* state, SourceLine line) {
    if (state->line_count + 1 > state->line_capacity) {
        state->line_capacity *= ARRAY_GROUTH_RATE;
        state->lines =
            realloc(state->lines, state->line_capacity * sizeof(SourceLine));
    }
    state->lines[state->line_count] = line;
    state->line_count++;
}

typedef enum PPType {
    PP_NONE,
    PP_INCLUDE,
    PP_DEFINE,
    PP_UNDEF,
    PP_IF,
    PP_ELIF,
    PP_ELSE,
    PP_ENDIF,
    PP_ERROR,
} PPType;

typedef struct StrPPType {
    const char* s;
    PPType pp_type;
} StrPPType;

static const StrPPType str_pp[] = {
    {"include", PP_INCLUDE}, {"define", PP_DEFINE}, {"undef", PP_UNDEF},
    {"if", PP_IF}, {"elif", PP_DEFINE}, {"else", PP_ELSE},
    {"endif", PP_ENDIF}, {"error", PP_ERROR},
};

Error* pp_expand(PPState* state, const char* filename, int depth) {
    if (state->src.file_count == 0) {
        // original source
        SourceFile orig_file = {0};
        orig_file.filename = arena_alloc(state->arena, strlen(filename) + 1);
        strcpy(orig_file.filename, filename);
        orig_file.pos.index = 0;
        orig_file.is_open = 0;

        append_file(&state->src, orig_file);
    }

    int last_include = state->last_include;
    int curr_file_index = state->src.file_count - 1;
    state->last_include = curr_file_index;

    if (depth > MAX_INCLUDE_DEPTH) {
        return error(state->src.files[last_include].pos,
                     "#include nested too deeply");
    }

    char* src = read_entire_file(filename);
    if (!src) {
        return error(state->src.files[last_include].pos, "failed to read file: %s",
                     strerror(errno));
    }
    state->src.files[curr_file_index].is_open = 1;

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
        line.content = arena_alloc(state->arena, line_len + 1);
        memcpy(line.content, src + start_pos, line_len);
        line.content[line_len] = '\0';

        // fix CRLF
        if (line.content[line_len - 1] == '\r') {
            line.content[line_len - 1] = '\0';
        }

        if (c != '\0') {
            pos++;
            c = *(src + pos);
        }

        // process line
        char* p = line.content;

        while (*p == ' ' || *p == '\t') {
            p++;
        }

        if (*p != '#') {
            append_line(&state->src, line);
            continue;
        }
        p++;

        while (*p == ' ' || *p == '\t') {
            p++;
        }

        PPType pp_type = PP_NONE;
        for (unsigned int i = 0; i < sizeof(str_pp) / sizeof(str_pp[0]);
             i++) {
            int pp_len = strlen(str_pp[i].s);
            if (strncmp(str_pp[i].s, p, pp_len) == 0) {
                pp_type = str_pp[i].pp_type;
                p += pp_len;
            }
            break;
        }

        if (pp_type == PP_NONE) {
            SourcePos pos;
            pos.line = line;
            pos.index = p - line.content;
            return error(pos, "invalid preprocessing directive");
        }

        switch (pp_type) {
            case PP_INCLUDE: {
                while (*p == ' ' || *p == '\t') {
                    p++;
                }

                if (*p != '"') {
                    SourcePos pos;
                    pos.line = line;
                    pos.index = p - line.content;
                    return error(pos, "#include expects \"FILENAME\"");
                }

                int include_index = p - line.content;

                p++;
                Str s = {
                    .ptr = p,
                    .len = 0,
                };

                while (*p != '"' && *p != '\0') {
                    s.len++;
                    p++;
                }

                if (*p != '"') {
                    SourcePos pos;
                    pos.line = line;
                    pos.index = p - line.content;
                    return error(pos, "missing terminating \" character");
                }

                Str dir = get_dir_name(str(filename));

                char inc_path[OS_PATH_MAX];
                snprintf(inc_path, sizeof(inc_path), "%.*s/%.*s", dir.len, dir.ptr,
                        s.len, s.ptr);

                if (curr_file_index == 0) {
                    state->src.files[0].pos.index = include_index;
                    state->src.files[0].pos.line = line;
                } else {
                    SourceFile file = {0};
                    file.filename = arena_alloc(state->arena, strlen(inc_path) + 1);
                    strcpy(file.filename, inc_path);
                    file.pos.index = include_index;
                    file.pos.line = line;
                    file.is_open = 0;

                    append_file(&state->src, file);
                }

                Error* err = pp_expand(state, inc_path, depth + 1);
                if (err != NULL) {
                    return err;
                }
            } break;
            case PP_DEFINE:
                break;
            case PP_UNDEF:
                break;
            case PP_IF:
                break;
            case PP_ELIF:
                break;
            case PP_ELSE:
                break;
            case PP_ENDIF:
                break;
            case PP_ERROR:
                break;
            default: 
                UNREACHABLE();
        }
    } while (c != '\0');

    state->last_include = last_include;

    free(src);

    return NULL;
}
