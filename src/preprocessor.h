#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#include <stddef.h>

#include "arena.h"

#define MAX_INCLUDE_DEPTH 15

typedef struct SourceLine {
    int file_index;
    int lineno;
    char* content;
} SourceLine;

typedef struct SourcePos {
    SourceLine line;
    size_t index;
} SourcePos;

typedef struct SourceFile {
    char* filename;
    int is_open;
    SourcePos pos;
} SourceFile;

/*
 *  files and lines arrays are allocated using malloc,
 *  the inner memory allocations (filename, content...) are using the arena.
 */
typedef struct SourceState {
    Arena* arena;

    SourceFile* files;
    size_t file_count;
    size_t file_capacity;

    SourceLine* lines;
    size_t line_count;
    size_t line_capacity;

    int last_include;
} SourceState;

void pp_init(SourceState* state, Arena* arena);

void pp_deinit(SourceState* state);

struct Error;

struct Error* pp_expand(SourceState* state, const char* filename, int depth);

#endif
