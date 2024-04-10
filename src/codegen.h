#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>

#include "parser.h"

typedef enum { RESULT_OK, RESULT_ERROR } ResultType;

typedef struct TypeInfo {
    int is_lvalue;
    int size;
} TypeInfo;

typedef struct EmitResult {
    ResultType type;
    union {
        TypeInfo info;
        Error* error;
    };
} EmitResult;

Error* codegen(FILE* out, ASTNode* node, SymbolTable* sym);

#endif
