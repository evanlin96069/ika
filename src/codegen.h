#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>

#include "arena.h"
#include "parser.h"
#include "type.h"

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

typedef enum { RESULT_OK, RESULT_ERROR } ResultType;

typedef struct TypeInfo {
    int is_lvalue;
    Type type;
} TypeInfo;

typedef struct EmitResult {
    ResultType type;
    union {
        TypeInfo info;
        Error* error;
    };
} EmitResult;

Error* codegen(CodegenState* state, ASTNode* node, SymbolTable* sym);

#endif
