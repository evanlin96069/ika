#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>

#include "arena.h"
#include "ast.h"

#define MAX_DATA_COUNT 256

typedef struct CodegenState {
    FILE* out;
    Arena* arena;

    int label_count;

    int data_count;
    Str data[MAX_DATA_COUNT];

    int ret_label;
    const Type* ret_type;

    int in_loop;
    int break_label;
    int continue_label;
} CodegenState;

void codegen(CodegenState* state, ASTNode* node, SymbolTable* sym,
             Str entry_sym);

#endif
