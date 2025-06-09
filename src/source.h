#ifndef SOURCE_H
#define SOURCE_H

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
    SourceFile* files;
    size_t file_count;
    size_t file_capacity;

    SourceLine* lines;
    size_t line_count;
    size_t line_capacity;
} SourceState;

#endif
