#include "preprocessor.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "str.h"
#include "utils.h"

#define FILE_INIT_SIZE 4
#define LINE_INIT_SIZE 256
#define ARRAY_GROUTH_RATE 2

void pp_init(SourceState* state, Arena* arena) {
    state->arena = arena;
    state->files = malloc(FILE_INIT_SIZE * sizeof(SourceFile));
    state->file_count = 0;
    state->file_capacity = FILE_INIT_SIZE;
    state->lines = malloc(LINE_INIT_SIZE * sizeof(SourceLine));
    state->line_count = 0;
    state->line_capacity = LINE_INIT_SIZE;
    state->last_include = 0;
}

void pp_deinit(SourceState* state) {
    free(state->files);
    free(state->lines);
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

Error* pp_expand(SourceState* state, const char* filename, int depth) {
    if (state->file_count == 0) {
        // original source
        SourceFile orig_file = {0};
        orig_file.filename = arena_alloc(state->arena, strlen(filename) + 1);
        strcpy(orig_file.filename, filename);
        orig_file.pos.index = 0;

        append_file(state, orig_file);
    }

    int last_include = state->last_include;
    int curr_file_index = state->file_count - 1;
    state->last_include = curr_file_index;

    if (depth > MAX_INCLUDE_DEPTH) {
        return error(state->files[last_include].pos,
                     "#include nested too deeply\n");
    }

    char* src = read_entire_file(filename);
    if (!src) {
        return error(state->files[last_include].pos, "%s\n", strerror(errno));
    }

    size_t pos = 0;
    char c = *(src + pos);
    int lineno = 1;
    while (c != '\0') {
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

        if (c != '\0') {
            pos++;
            c = *(src + pos);
        }

        // process line
        char* p = line.content;

        while (*p == ' ' || *p == '\t') {
            p++;
        }

        if (strncmp("#include", p, 8) == 0) {
            p += 8;
            while (*p == ' ' || *p == '\t') {
                p++;
            }

            if (*p != '"') {
                SourcePos pos;
                pos.line = line;
                pos.index = p - line.content;
                return error(pos, "#include expects \"FILENAME\"\n");
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
                return error(pos, "missing terminating \" character\n");
            }

            char* dir = dirname(filename);

            size_t inc_path_len = strlen(dir) + 1 + s.len;
            char* inc_path = malloc(inc_path_len + 1);

            snprintf(inc_path, inc_path_len + 1, "%s/%.*s", dir, s.len, s.ptr);
            free(dir);

            SourceFile file = {0};
            file.filename = arena_alloc(state->arena, strlen(inc_path) + 1);
            strcpy(file.filename, inc_path);
            file.pos.index = include_index;
            file.pos.line = line;

            append_file(state, file);

            Error* err = pp_expand(state, inc_path, depth + 1);
            if (err != NULL) {
                return err;
            }
            free(inc_path);
        } else {
            append_line(state, line);
        }
    }

    state->last_include = last_include;
    return NULL;
}
