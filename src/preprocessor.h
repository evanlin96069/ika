#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#include "source.h"
#include "utl/allocator/utlarena.h"

#define MAX_INCLUDE_DEPTH 15

struct SymbolTable;

typedef UtlVector(const char*) Paths;

typedef struct PPState {
    UtlArenaAllocator* arena;
    UtlAllocator* temp_allocator;
    SourceState src;

    const Paths* include_paths;
    struct SymbolTable* sym;  // for #define
} PPState;

struct SymbolTable;

void pp_init(PPState* state, UtlArenaAllocator* arena,
             UtlAllocator* temp_allocator, const Paths* include_paths,
             struct SymbolTable* sym);

void pp_finalize(PPState* state);

struct Error;

struct Error* pp_expand(PPState* state, const char* filename);

#endif
