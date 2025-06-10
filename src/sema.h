#ifndef SEMA_H
#define SEMA_H

#include "ast.h"
#include "utl/allocator/utlarena.h"

#define MAX_DATA_COUNT 256

typedef struct SemaState {
    UtlArenaAllocator* arena;

    int return_label;
    const Type* return_type;
    int max_struct_return_size;

    int in_loop;
} SemaState;

Error* sema(SemaState* state, ASTNode* node, SymbolTable* sym, Str entry_sym);

#endif
