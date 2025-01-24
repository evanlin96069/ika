#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>

#include "arena.h"
#include "parser.h"
#include "type.h"

typedef struct CodegenState {
    FILE* out;
    Arena* arena;
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
