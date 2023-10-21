#ifndef VM_H
#define VM_H

#include "parser.h"

extern int pc, sp, bp;

int codegen(ASTNode* node, SymbolTable* sym);
void print_code(void);
int vm_run(void);

#endif
