#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>

#include "parser.h"

void codegen(FILE* out, ASTNode* node, SymbolTable* sym);

#endif
