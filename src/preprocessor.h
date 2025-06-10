#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#include "source.h"
#include "utl/allocator/utlarena.h"

#define MAX_INCLUDE_DEPTH 15

struct SymbolTable;

typedef struct PPState {
    UtlArenaAllocator* arena;
    UtlAllocator* temp_allocator;
    SourceState src;

    int last_include;
    struct SymbolTable* sym;  // for #define
} PPState;

void pp_init(PPState* state, UtlArenaAllocator* arena,
             UtlAllocator* temp_allocator);

void pp_finalize(PPState* state);

struct Error;

struct Error* pp_expand(PPState* state, const char* filename);

#endif
