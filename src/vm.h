#ifndef VM_H
#define VM_H

#include "parser.h"

extern int pc, sp, bp;

void codegen(ASTNode* node);
void print_code(void);
int vm_run(void);

#endif
