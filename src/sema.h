#ifndef SEMA_H
#define SEMA_H

#include <stdio.h>

#include "arena.h"
#include "ast.h"

#define MAX_DATA_COUNT 256

typedef struct SemaState {
    Arena* arena;

    int ret_label;
    const Type* ret_type;

    int in_loop;
} SemaState;

Error* sema(SemaState* state, ASTNode* node, SymbolTable* sym);

#endif
