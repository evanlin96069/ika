#ifndef SOURCE_H
#define SOURCE_H

#include "utl/utlvector.h"

typedef struct SourceLine SourceLine;

struct SourceLine {
    int file_index;
    int lineno;
    char* content;

    struct SourceLine* next;
};

typedef struct SourcePos {
    SourceLine line;
    size_t index;
} SourcePos;

typedef struct SourceFile {
    char* filename;
    int is_open;
    SourcePos pos;
} SourceFile;

typedef UtlVector(SourceFile) SourceFiles;

typedef struct SourceState {
    SourceFiles files;
    SourceLine* lines;
} SourceState;

#endif
