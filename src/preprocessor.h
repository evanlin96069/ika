#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#include "arena.h"
#include "source.h"

#define MAX_INCLUDE_DEPTH 15

struct SymbolTable;

typedef struct PPState {
    Arena* arena;
    SourceState src;

    int last_include;
    struct SymbolTable* sym;  // for #define
} PPState;

void pp_init(PPState* state, Arena* arena);

void pp_deinit(PPState* state);

struct Error;

struct Error* pp_expand(PPState* state, const char* filename, int depth);

#endif
