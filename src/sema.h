#ifndef SEMA_H
#define SEMA_H

#include <stdio.h>

#include "arena.h"
#include "ast.h"

#define MAX_DATA_COUNT 256

typedef struct SemaState {
    Arena* arena;

    int return_label;
    const Type* return_type;
    int max_struct_return_size;

    int in_loop;
} SemaState;

Error* sema(SemaState* state, ASTNode* node, SymbolTable* sym, Str entry_sym);

#endif
